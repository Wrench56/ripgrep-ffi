#![allow(clippy::missing_safety_doc)]

use globset::{Glob, GlobBuilder, GlobMatcher, GlobSet, GlobSetBuilder};
use libc::{c_char, c_int, size_t};
use std::ffi::{CStr, OsStr};
use std::os::unix::ffi::OsStrExt;
use std::path::Path;

#[repr(C)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum RgffiGlobErr {
    Ok = 0,
    Match = 1,
    NoMatch = 2,

    GlobErr = -1,
    NullErr = -2,
    Utf8Err = -3,
    BufferTooSmallErr = -4,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct RgffiGlobMatcherOpts {
    pub case_insensitive: bool,
    pub literal_separator: bool,
    pub backslash_escape: bool,
    pub empty_alternates: bool,
    pub allow_unclosed_class: bool,
}

/// Internal GlobMatcher type to optimize general interface
enum RgffiGlobInner {
    Empty,
    Single(GlobMatcher),
    Set(GlobSet),
}

pub struct RgffiGlobMatcher {
    inner: RgffiGlobInner,
}

impl RgffiGlobMatcherOpts {
    fn is_default(&self) -> bool {
        *self
            == Self {
                case_insensitive: false,
                literal_separator: false,
                backslash_escape: cfg!(not(windows)),
                empty_alternates: false,
                allow_unclosed_class: false,
            }
    }
}

unsafe fn cstr_to_str<'a>(s: *const c_char) -> Result<&'a str, RgffiGlobErr> {
    if s.is_null() {
        return Err(RgffiGlobErr::NullErr);
    }

    unsafe { CStr::from_ptr(s) }
        .to_str()
        .map_err(|_| RgffiGlobErr::Utf8Err)
}

fn make_glob(pattern: &str, opts: Option<&RgffiGlobMatcherOpts>) -> Result<Glob, RgffiGlobErr> {
    match opts {
        None => Glob::new(pattern).map_err(|_| RgffiGlobErr::GlobErr),

        Some(opts) if opts.is_default() => Glob::new(pattern).map_err(|_| RgffiGlobErr::GlobErr),

        Some(opts) => {
            let mut builder = GlobBuilder::new(pattern);

            builder
                .case_insensitive(opts.case_insensitive)
                .literal_separator(opts.literal_separator)
                .backslash_escape(opts.backslash_escape)
                .empty_alternates(opts.empty_alternates)
                .allow_unclosed_class(opts.allow_unclosed_class);

            builder.build().map_err(|_| RgffiGlobErr::GlobErr)
        }
    }
}

fn build_matcher(
    patterns: &[&str],
    opts: Option<&RgffiGlobMatcherOpts>,
) -> Result<RgffiGlobMatcher, RgffiGlobErr> {
    match patterns.len() {
        0 => Ok(RgffiGlobMatcher {
            inner: RgffiGlobInner::Empty,
        }),
        1 => {
            let glob = make_glob(patterns[0], opts)?;

            Ok(RgffiGlobMatcher {
                inner: RgffiGlobInner::Single(glob.compile_matcher()),
            })
        }
        _ => {
            let mut builder = GlobSetBuilder::new();

            for pattern in patterns {
                builder.add(make_glob(pattern, opts)?);
            }

            let set = builder.build().map_err(|_| RgffiGlobErr::GlobErr)?;

            Ok(RgffiGlobMatcher {
                inner: RgffiGlobInner::Set(set),
            })
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rg_glob_version() -> *const c_char {
    c"rg-glob 0.1.0".as_ptr()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_glob_default_opts(opts: *mut RgffiGlobMatcherOpts) -> c_int {
    if opts.is_null() {
        return RgffiGlobErr::NullErr as c_int;
    }

    unsafe {
        *opts = RgffiGlobMatcherOpts {
            case_insensitive: false,
            literal_separator: false,
            backslash_escape: cfg!(not(windows)),
            empty_alternates: false,
            allow_unclosed_class: false,
        };
    }

    RgffiGlobErr::Ok as c_int
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_glob_matcher_build(
    matcher: *mut *mut RgffiGlobMatcher,
    opts: *const RgffiGlobMatcherOpts,
    patterns: *const *const c_char,
    patterns_len: size_t,
) -> c_int {
    if matcher.is_null() {
        return RgffiGlobErr::NullErr as c_int;
    }

    unsafe {
        *matcher = std::ptr::null_mut();
    }

    let patterns_raw: &[*const c_char] = if patterns_len == 0 {
        &[]
    } else {
        if patterns.is_null() {
            return RgffiGlobErr::NullErr as c_int;
        }

        unsafe { std::slice::from_raw_parts(patterns, patterns_len) }
    };

    let mut pattern_strs = Vec::with_capacity(patterns_len);

    for &pattern in patterns_raw {
        let pattern = match unsafe { cstr_to_str(pattern) } {
            Ok(pattern) => pattern,
            Err(err) => return err as c_int,
        };

        pattern_strs.push(pattern);
    }

    let opts = if opts.is_null() {
        None
    } else {
        Some(unsafe { &*opts })
    };

    let built = match build_matcher(&pattern_strs, opts) {
        Ok(matcher) => matcher,
        Err(err) => return err as c_int,
    };

    unsafe {
        *matcher = Box::into_raw(Box::new(built));
    }

    RgffiGlobErr::Ok as c_int
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_glob_matcher_free(matcher: *mut RgffiGlobMatcher) {
    if matcher.is_null() {
        return;
    }

    unsafe {
        drop(Box::from_raw(matcher));
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_glob_matcher_is_match(
    matcher: *const RgffiGlobMatcher,
    path: *const c_char,
) -> RgffiGlobErr {
    if matcher.is_null() || path.is_null() {
        return RgffiGlobErr::NullErr;
    }

    let matcher = unsafe { &*matcher };
    let path = unsafe { Path::new(OsStr::from_bytes(CStr::from_ptr(path).to_bytes())) };
    match &matcher.inner {
        RgffiGlobInner::Empty => RgffiGlobErr::NoMatch,
        RgffiGlobInner::Single(matcher) => {
            if matcher.is_match(path) {
                return RgffiGlobErr::Match;
            }
            RgffiGlobErr::NoMatch
        }
        RgffiGlobInner::Set(matcher) => {
            if matcher.is_match(path) {
                return RgffiGlobErr::Match;
            }
            RgffiGlobErr::NoMatch
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_glob_matcher_matches(
    matcher: *const RgffiGlobMatcher,
    path: *const c_char,
    matches: *mut size_t,
    matches_cap: size_t,
    matches_len: *mut size_t,
) -> RgffiGlobErr {
    if matcher.is_null() || path.is_null() || matches_len.is_null() {
        return RgffiGlobErr::NullErr;
    }

    let matcher = unsafe { &*matcher };
    let path = unsafe { Path::new(OsStr::from_bytes(CStr::from_ptr(path).to_bytes())) };

    unsafe {
        *matches_len = 0;
    }

    match &matcher.inner {
        RgffiGlobInner::Empty => RgffiGlobErr::NoMatch,
        RgffiGlobInner::Single(m) => {
            if !m.is_match(path) {
                return RgffiGlobErr::NoMatch;
            }

            unsafe {
                *matches_len = 1;
                if matches_cap > 0 {
                    if matches.is_null() {
                        return RgffiGlobErr::NullErr;
                    }

                    *matches = 0;
                } else {
                    return RgffiGlobErr::BufferTooSmallErr;
                }
            }

            RgffiGlobErr::Match
        }
        RgffiGlobInner::Set(m) => {
            let found = m.matches(path);

            if found.is_empty() {
                return RgffiGlobErr::NoMatch;
            }

            unsafe {
                *matches_len = found.len();

                if found.len() > matches_cap {
                    return RgffiGlobErr::BufferTooSmallErr;
                }

                if matches.is_null() {
                    return RgffiGlobErr::NullErr;
                }

                std::ptr::copy_nonoverlapping(found.as_ptr(), matches, found.len());
            }

            RgffiGlobErr::Match
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_glob_matcher_len(
    matcher: *const RgffiGlobMatcher,
    len: *mut size_t,
) -> RgffiGlobErr {
    if matcher.is_null() || len.is_null() {
        return RgffiGlobErr::NullErr;
    }

    let matcher = unsafe { &*matcher };
    let n = match &matcher.inner {
        RgffiGlobInner::Empty => 0,
        RgffiGlobInner::Single(_) => 1,
        RgffiGlobInner::Set(matcher) => matcher.len(),
    };

    unsafe {
        *len = n;
    }

    RgffiGlobErr::Ok
}
