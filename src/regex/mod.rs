#![allow(clippy::missing_safety_doc)]

use grep_matcher::{Captures, Matcher};
use grep_regex::{RegexMatcher, RegexMatcherBuilder};
use libc::{c_char, c_int, size_t};
use std::ffi::CStr;
use std::ptr;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub enum RgffiRegexErr {
    Ok = 0,
    Match = 1,
    NoMatch = 2,
    RegexErr = -1,
    NotAllowedErr = -2,
    InvalidLineTerminatorErr = -3,
    BannedErr = -4,
    NullErr = -5,
    Utf8Err = -6,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct RgffiRegexMatcherOpts {
    pub case_insensitive: bool,
    pub case_smart: bool,
    pub multi_line: bool,
    pub dot_matches_new_line: bool,
    pub swap_greed: bool,
    pub ignore_whitespace: bool,
    pub unicode: bool,
    pub octal: bool,
    pub crlf: bool,
    pub word: bool,
    pub fixed_strings: bool,
    pub whole_line: bool,
    pub has_line_terminator: bool,
    pub has_ban_byte: bool,

    pub size_limit: size_t,
    pub dfa_size_limit: size_t,
    pub nest_limit: u32,

    pub line_terminator: u8,
    pub ban_byte: u8,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct RgffiRegexMatch {
    pub start: size_t,
    pub end: size_t,
}

pub struct RgffiMatcher {
    inner: RegexMatcher,
}

pub struct RgffiRegexCaptures {
    inner: grep_regex::RegexCaptures,
}

unsafe fn cstr_to_str<'a>(s: *const c_char) -> Result<&'a str, RgffiRegexErr> {
    if s.is_null() {
        return Err(RgffiRegexErr::NullErr);
    }

    unsafe { CStr::from_ptr(s) }
        .to_str()
        .map_err(|_| RgffiRegexErr::Utf8Err)
}

unsafe fn cstr_array_to_vec<'a>(
    ptr: *const *const c_char,
    len: size_t,
) -> Result<Vec<&'a str>, RgffiRegexErr> {
    if ptr.is_null() && len != 0 {
        return Err(RgffiRegexErr::NullErr);
    }

    let raw = if len == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(ptr, len) }
    };

    let mut out = Vec::with_capacity(raw.len());

    for &s in raw {
        out.push(unsafe { cstr_to_str(s)? });
    }

    Ok(out)
}

unsafe fn bytes_from_raw<'a>(ptr: *const u8, len: size_t) -> Result<&'a [u8], RgffiRegexErr> {
    if ptr.is_null() && len != 0 {
        return Err(RgffiRegexErr::NullErr);
    }

    if len == 0 {
        Ok(&[])
    } else {
        Ok(unsafe { std::slice::from_raw_parts(ptr, len) })
    }
}

fn write_match(out: *mut RgffiRegexMatch, m: grep_matcher::Match) {
    unsafe {
        (*out).start = m.start();
        (*out).end = m.end();
    }
}

fn make_builder(opts: &RgffiRegexMatcherOpts) -> Result<RegexMatcherBuilder, RgffiRegexErr> {
    let mut builder = RegexMatcherBuilder::new();

    builder
        .case_insensitive(opts.case_insensitive)
        .case_smart(opts.case_smart)
        .multi_line(opts.multi_line)
        .dot_matches_new_line(opts.dot_matches_new_line)
        .swap_greed(opts.swap_greed)
        .ignore_whitespace(opts.ignore_whitespace)
        .unicode(opts.unicode)
        .octal(opts.octal)
        .crlf(opts.crlf)
        .word(opts.word)
        .fixed_strings(opts.fixed_strings)
        .whole_line(opts.whole_line)
        .crlf(opts.crlf);

    if opts.size_limit != 0 {
        builder.size_limit(opts.size_limit);
    }

    if opts.dfa_size_limit != 0 {
        builder.dfa_size_limit(opts.dfa_size_limit);
    }

    if opts.nest_limit != 0 {
        builder.nest_limit(opts.nest_limit);
    }

    if opts.has_line_terminator {
        builder.line_terminator(Some(opts.line_terminator));
    } else {
        builder.line_terminator(None);
    }

    if opts.has_ban_byte {
        builder.ban_byte(Some(opts.ban_byte));
    } else {
        builder.ban_byte(None);
    }

    Ok(builder)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_default_opts(opts: *mut RgffiRegexMatcherOpts) -> c_int {
    if opts.is_null() {
        return RgffiRegexErr::NullErr as c_int;
    }

    unsafe {
        *opts = RgffiRegexMatcherOpts {
            case_insensitive: false,
            case_smart: false,
            multi_line: false,
            dot_matches_new_line: false,
            swap_greed: false,
            ignore_whitespace: false,
            unicode: true,
            octal: false,
            crlf: false,
            word: false,
            fixed_strings: false,
            whole_line: false,
            has_line_terminator: false,
            has_ban_byte: false,

            size_limit: 0,
            dfa_size_limit: 0,
            nest_limit: 0,

            line_terminator: b'\n',
            ban_byte: 0,
        };
    }

    RgffiRegexErr::Ok as c_int
}

#[unsafe(no_mangle)]
pub extern "C" fn rg_regex_version() -> *const c_char {
    c"rg-regex 0.1.0".as_ptr()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_build(
    matcher: *mut *mut RgffiMatcher,
    opts: *const RgffiRegexMatcherOpts,
    patterns: *const *const c_char,
    patterns_len: size_t,
    is_literal: bool,
) -> c_int {
    if patterns_len == 0 {
        return RgffiRegexErr::NotAllowedErr as c_int;
    }

    let patterns = match unsafe { cstr_array_to_vec(patterns, patterns_len) } {
        Ok(v) => v,
        Err(e) => return e as c_int,
    };

    if matcher.is_null() || opts.is_null() {
        return RgffiRegexErr::NullErr as c_int;
    }

    unsafe {
        *matcher = ptr::null_mut();
    }

    let opts = unsafe { &*opts };

    let builder = match make_builder(opts) {
        Ok(builder) => builder,
        Err(e) => return e as c_int,
    };

    let inner = match match (patterns_len, is_literal) {
        (1, false) => builder.build(patterns[0]),
        (_, false) => builder.build_many(&patterns),
        (_, true) => {
            let escaped: Vec<String> = patterns.iter().map(|p| regex_syntax::escape(p)).collect();
            builder.build_literals(&escaped)
        }
    } {
        Ok(matcher) => matcher,
        Err(_) => return RgffiRegexErr::RegexErr as c_int,
    };

    unsafe {
        *matcher = Box::into_raw(Box::new(RgffiMatcher { inner }));
    }

    RgffiRegexErr::Ok as c_int
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_free(matcher: *mut RgffiMatcher) {
    if matcher.is_null() {
        return;
    }

    unsafe {
        drop(Box::from_raw(matcher));
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_find_at(
    matcher: *const RgffiMatcher,
    out_match: *mut RgffiRegexMatch,
    haystack: *const u8,
    haystack_len: size_t,
    at: size_t,
) -> c_int {
    if matcher.is_null() || out_match.is_null() {
        return RgffiRegexErr::NullErr as c_int;
    }

    let matcher = unsafe { &*matcher };

    let haystack = match unsafe { bytes_from_raw(haystack, haystack_len) } {
        Ok(h) => h,
        Err(e) => return e as c_int,
    };

    match matcher.inner.find_at(haystack, at) {
        Ok(Some(m)) => {
            write_match(out_match, m);
            RgffiRegexErr::Match as c_int
        }
        Ok(None) => RgffiRegexErr::NoMatch as c_int,
        Err(_) => RgffiRegexErr::RegexErr as c_int,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_is_match_at(
    matcher: *const RgffiMatcher,
    haystack: *const u8,
    haystack_len: size_t,
    at: size_t,
) -> c_int {
    if matcher.is_null() {
        return RgffiRegexErr::NullErr as c_int;
    }

    let matcher = unsafe { &*matcher };

    let haystack = match unsafe { bytes_from_raw(haystack, haystack_len) } {
        Ok(h) => h,
        Err(e) => return e as c_int,
    };

    match matcher.inner.is_match_at(haystack, at) {
        Ok(false) => RgffiRegexErr::NoMatch as c_int,
        Ok(true) => RgffiRegexErr::Match as c_int,
        Err(_) => RgffiRegexErr::RegexErr as c_int,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_is_match(
    matcher: *const RgffiMatcher,
    haystack: *const u8,
    haystack_len: size_t,
) -> c_int {
    unsafe { rg_regex_matcher_is_match_at(matcher, haystack, haystack_len, 0) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_shortest_match(
    matcher: *const RgffiMatcher,
    haystack: *const u8,
    haystack_len: size_t,
    out_end: *mut size_t,
) -> c_int {
    if matcher.is_null() || out_end.is_null() {
        return RgffiRegexErr::NullErr as c_int;
    }

    let matcher = unsafe { &*matcher };

    let haystack = match unsafe { bytes_from_raw(haystack, haystack_len) } {
        Ok(h) => h,
        Err(e) => return e as c_int,
    };

    match matcher.inner.shortest_match(haystack) {
        Ok(Some(end)) => {
            unsafe {
                *out_end = end;
            }

            RgffiRegexErr::Match as c_int
        }

        Ok(None) => RgffiRegexErr::NoMatch as c_int,

        Err(_) => RgffiRegexErr::RegexErr as c_int,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_new_captures(
    matcher: *const RgffiMatcher,
    captures: *mut *mut RgffiRegexCaptures,
) -> c_int {
    if matcher.is_null() || captures.is_null() {
        return RgffiRegexErr::NullErr as c_int;
    }

    unsafe {
        *captures = ptr::null_mut();
    }

    let matcher = unsafe { &*matcher };

    match matcher.inner.new_captures() {
        Ok(caps) => {
            unsafe {
                *captures = Box::into_raw(Box::new(RgffiRegexCaptures { inner: caps }));
            }
            RgffiRegexErr::Ok as c_int
        }
        Err(_) => RgffiRegexErr::RegexErr as c_int,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_captures_free(captures: *mut RgffiRegexCaptures) {
    if captures.is_null() {
        return;
    }

    unsafe {
        drop(Box::from_raw(captures));
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_capture_count(matcher: *const RgffiMatcher) -> size_t {
    if matcher.is_null() {
        return 0;
    }

    let matcher = unsafe { &*matcher };
    matcher.inner.capture_count() as size_t
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_captures_at(
    matcher: *const RgffiMatcher,
    haystack: *const u8,
    haystack_len: size_t,
    at: size_t,
    captures: *mut RgffiRegexCaptures,
) -> c_int {
    if matcher.is_null() || captures.is_null() {
        return RgffiRegexErr::NullErr as c_int;
    }

    let matcher = unsafe { &*matcher };
    let captures = unsafe { &mut *captures };

    let haystack = match unsafe { bytes_from_raw(haystack, haystack_len) } {
        Ok(h) => h,
        Err(e) => return e as c_int,
    };

    match matcher.inner.captures_at(haystack, at, &mut captures.inner) {
        Ok(false) => RgffiRegexErr::NoMatch as c_int,
        Ok(true) => RgffiRegexErr::Match as c_int,
        Err(_) => RgffiRegexErr::RegexErr as c_int,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_captures_get(
    captures: *const RgffiRegexCaptures,
    index: size_t,
    out_match: *mut RgffiRegexMatch,
) -> c_int {
    if captures.is_null() || out_match.is_null() {
        return RgffiRegexErr::NullErr as c_int;
    }

    let captures = unsafe { &*captures };

    match captures.inner.get(index) {
        Some(m) => {
            write_match(out_match, m);
            RgffiRegexErr::Match as c_int
        }
        None => RgffiRegexErr::NoMatch as c_int,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_replace(
    matcher: *const RgffiMatcher,
    haystack: *const u8,
    haystack_len: size_t,
    replacement: *const u8,
    replacement_len: size_t,
) -> c_int {
    if matcher.is_null() {
        return RgffiRegexErr::NullErr as c_int;
    }

    let matcher = unsafe { &*matcher };

    let haystack = match unsafe { bytes_from_raw(haystack, haystack_len) } {
        Ok(h) => h,
        Err(e) => return e as c_int,
    };

    let replacement = match unsafe { bytes_from_raw(replacement, replacement_len) } {
        Ok(r) => r,
        Err(e) => return e as c_int,
    };

    let mut dst = Vec::new();

    let result = matcher.inner.replace(haystack, &mut dst, |_m, dst| {
        dst.extend_from_slice(replacement);
        true
    });

    if result.is_err() {
        return RgffiRegexErr::RegexErr as c_int;
    }

    RgffiRegexErr::Ok as c_int
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_regex_matcher_find(
    matcher: *const RgffiMatcher,
    out_match: *mut RgffiRegexMatch,
    haystack: *const u8,
    haystack_len: size_t,
) -> c_int {
    unsafe { rg_regex_matcher_find_at(matcher, out_match, haystack, haystack_len, 0) }
}
