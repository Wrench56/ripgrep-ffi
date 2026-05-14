#![allow(clippy::missing_safety_doc)]

use ignore::{DirEntry, WalkBuilder, WalkState};
use libc::{c_char, c_void, size_t};
use std::ffi::{CStr, CString};
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};

const RG_WALKER_FLAG_STDIN: u16 = 1 << 1;
const RG_WALKER_FLAG_SYMLINK: u16 = 1 << 2;

#[cfg(unix)]
const RG_WALKER_FLAG_BLOCK_DEVICE: u16 = 1 << 6;
#[cfg(unix)]
const RG_WALKER_FLAG_CHAR_DEVICE: u16 = 1 << 7;
#[cfg(unix)]
const RG_WALKER_FLAG_FIFO: u16 = 1 << 8;
#[cfg(unix)]
const RG_WALKER_FLAG_SOCKET: u16 = 1 << 9;

#[cfg(windows)]
const RG_WALKER_FLAG_SYMLINK_FILE: u16 = 1 << 14;
#[cfg(windows)]
const RG_WALKER_FLAG_SYMLINK_DIR: u16 = 1 << 15;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub enum RgffiWalkerErr {
    Ok = 0,
    Err = -1,
    NullErr = -2,
    Utf8Err = -3,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub enum RgffiWalkerWalkState {
    Continue = 0,
    Skip = 1,
    Quit = 2,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub enum RgffiWalkerEntryKind {
    Unknown = 0,
    File = 1,
    Dir = 2,
    Symlink = 3,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct RgffiWalkerEntry {
    pub path: *const c_char,
    pub filename: *const c_char,
    pub depth: size_t,
    pub ino: u64,
    pub kind: RgffiWalkerEntryKind,
    pub flags: u16,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct RgffiWalkerOpts {
    pub min_depth: size_t,
    pub max_depth: size_t,
    pub max_filesize: size_t,
    pub threads: size_t,

    pub ignore_file: *const c_char,

    pub follow_links: bool,
    pub hidden: bool,
    pub parents: bool,
    pub ignore: bool,
    pub git_global: bool,
    pub git_ignore: bool,
    pub git_exclude: bool,
    pub require_git: bool,
    pub ignore_case_insensitive: bool,
    pub same_file_system: bool,
    pub skip_stdout: bool,
}

pub type RgffiWalkerFunc = Option<
    unsafe extern "C" fn(
        entry: *const RgffiWalkerEntry,
        payload: *mut c_void,
    ) -> RgffiWalkerWalkState,
>;

pub struct RgffiWalker {
    opts: RgffiWalkerOptsOwned,
    cwd: Option<String>,
    paths: Vec<String>,
}

#[derive(Clone, Debug)]
struct RgffiWalkerOptsOwned {
    min_depth: size_t,
    max_depth: size_t,
    max_filesize: size_t,
    threads: size_t,

    ignore_file: Option<String>,

    follow_links: bool,
    hidden: bool,
    parents: bool,
    ignore: bool,
    git_global: bool,
    git_ignore: bool,
    git_exclude: bool,
    require_git: bool,
    ignore_case_insensitive: bool,
    same_file_system: bool,
    skip_stdout: bool,
}

unsafe fn cstr_to_str<'a>(s: *const c_char) -> Result<&'a str, RgffiWalkerErr> {
    if s.is_null() {
        return Err(RgffiWalkerErr::NullErr);
    }

    unsafe { CStr::from_ptr(s) }
        .to_str()
        .map_err(|_| RgffiWalkerErr::Utf8Err)
}

unsafe fn optional_cstr_to_string(s: *const c_char) -> Result<Option<String>, RgffiWalkerErr> {
    if s.is_null() {
        return Ok(None);
    }

    unsafe { cstr_to_str(s) }.map(|s| Some(s.to_owned()))
}

unsafe fn cstr_array_to_vec(
    ptr: *const *const c_char,
    len: size_t,
) -> Result<Vec<String>, RgffiWalkerErr> {
    if ptr.is_null() || len == 0 {
        return Err(RgffiWalkerErr::NullErr);
    }

    let raw = unsafe { std::slice::from_raw_parts(ptr, len) };
    let mut out = Vec::with_capacity(raw.len());

    for &s in raw {
        out.push(unsafe { cstr_to_str(s)? }.to_owned());
    }

    Ok(out)
}

fn entry_flags(entry: &DirEntry) -> u16 {
    let mut flags = 0u16;

    if entry.is_stdin() {
        flags |= RG_WALKER_FLAG_STDIN;
    }

    if entry.path_is_symlink() {
        flags |= RG_WALKER_FLAG_SYMLINK;
    }

    let Some(ft) = entry.file_type() else {
        return flags;
    };

    #[cfg(unix)]
    {
        use std::os::unix::fs::FileTypeExt;

        if ft.is_block_device() {
            flags |= RG_WALKER_FLAG_BLOCK_DEVICE;
        }

        if ft.is_char_device() {
            flags |= RG_WALKER_FLAG_CHAR_DEVICE;
        }

        if ft.is_fifo() {
            flags |= RG_WALKER_FLAG_FIFO;
        }

        if ft.is_socket() {
            flags |= RG_WALKER_FLAG_SOCKET;
        }
    }

    #[cfg(windows)]
    {
        use std::os::windows::fs::FileTypeExt;

        if ft.is_symlink_file() {
            flags |= RG_WALKER_FLAG_SYMLINK_FILE;
        }

        if ft.is_symlink_dir() {
            flags |= RG_WALKER_FLAG_SYMLINK_DIR;
        }
    }

    flags
}

fn opt_usize(value: size_t) -> Option<usize> {
    if value == size_t::MAX {
        None
    } else {
        Some(value)
    }
}

fn map_walk_state(state: RgffiWalkerWalkState) -> WalkState {
    match state {
        RgffiWalkerWalkState::Continue => WalkState::Continue,
        RgffiWalkerWalkState::Skip => WalkState::Skip,
        RgffiWalkerWalkState::Quit => WalkState::Quit,
    }
}

fn entry_kind(entry: &DirEntry) -> RgffiWalkerEntryKind {
    let Some(ft) = entry.file_type() else {
        return RgffiWalkerEntryKind::Unknown;
    };

    if ft.is_file() {
        RgffiWalkerEntryKind::File
    } else if ft.is_dir() {
        RgffiWalkerEntryKind::Dir
    } else if ft.is_symlink() {
        RgffiWalkerEntryKind::Symlink
    } else {
        RgffiWalkerEntryKind::Unknown
    }
}

fn apply_opts(
    builder: &mut WalkBuilder,
    opts: &RgffiWalkerOptsOwned,
) -> Result<(), RgffiWalkerErr> {
    builder.min_depth(opt_usize(opts.min_depth));
    builder.max_depth(opt_usize(opts.max_depth));

    if opts.max_filesize == size_t::MAX {
        builder.max_filesize(None);
    } else {
        builder.max_filesize(Some(opts.max_filesize as u64));
    }

    builder.threads(opts.threads);
    builder.follow_links(opts.follow_links);
    builder.hidden(opts.hidden);
    builder.parents(opts.parents);
    builder.ignore(opts.ignore);
    builder.git_global(opts.git_global);
    builder.git_ignore(opts.git_ignore);
    builder.git_exclude(opts.git_exclude);
    builder.require_git(opts.require_git);
    builder.ignore_case_insensitive(opts.ignore_case_insensitive);
    builder.same_file_system(opts.same_file_system);
    builder.skip_stdout(opts.skip_stdout);

    if let Some(ignore_file) = &opts.ignore_file
        && builder.add_ignore(ignore_file).is_some()
    {
        return Err(RgffiWalkerErr::Err);
    }

    Ok(())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_walker_default_opts(opts: *mut RgffiWalkerOpts) -> RgffiWalkerErr {
    if opts.is_null() {
        return RgffiWalkerErr::NullErr;
    }

    unsafe {
        *opts = RgffiWalkerOpts {
            min_depth: size_t::MAX,
            max_depth: size_t::MAX,
            max_filesize: size_t::MAX,
            threads: 0,

            ignore_file: ptr::null(),

            follow_links: false,
            hidden: true,
            parents: true,
            ignore: true,
            git_global: true,
            git_ignore: true,
            git_exclude: true,
            require_git: false,
            ignore_case_insensitive: false,
            same_file_system: false,
            skip_stdout: false,
        };
    }

    RgffiWalkerErr::Ok
}

#[unsafe(no_mangle)]
pub extern "C" fn rg_walker_version() -> *const c_char {
    c"rg-walker 0.1.0".as_ptr()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_walker_build(
    walker: *mut *mut RgffiWalker,
    opts: *const RgffiWalkerOpts,
    cwd: *const c_char,
    paths: *const *const c_char,
    paths_len: size_t,
) -> RgffiWalkerErr {
    if walker.is_null() || opts.is_null() {
        return RgffiWalkerErr::NullErr;
    }

    unsafe {
        *walker = ptr::null_mut();
    }

    let opts_ref = unsafe { &*opts };

    let paths = match unsafe { cstr_array_to_vec(paths, paths_len) } {
        Ok(paths) => paths,
        Err(err) => return err,
    };

    let cwd = match unsafe { optional_cstr_to_string(cwd) } {
        Ok(cwd) => cwd,
        Err(err) => return err,
    };

    let ignore_file = match unsafe { optional_cstr_to_string(opts_ref.ignore_file) } {
        Ok(ignore_file) => ignore_file,
        Err(err) => return err,
    };

    let owned_opts = RgffiWalkerOptsOwned {
        min_depth: opts_ref.min_depth,
        max_depth: opts_ref.max_depth,
        max_filesize: opts_ref.max_filesize,
        threads: opts_ref.threads,

        ignore_file,

        follow_links: opts_ref.follow_links,
        hidden: opts_ref.hidden,
        parents: opts_ref.parents,
        ignore: opts_ref.ignore,
        git_global: opts_ref.git_global,
        git_ignore: opts_ref.git_ignore,
        git_exclude: opts_ref.git_exclude,
        require_git: opts_ref.require_git,
        ignore_case_insensitive: opts_ref.ignore_case_insensitive,
        same_file_system: opts_ref.same_file_system,
        skip_stdout: opts_ref.skip_stdout,
    };

    unsafe {
        *walker = Box::into_raw(Box::new(RgffiWalker {
            opts: owned_opts,
            cwd,
            paths,
        }));
    }

    RgffiWalkerErr::Ok
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_walker_run(
    walker: *mut RgffiWalker,
    func: RgffiWalkerFunc,
    payload: *mut c_void,
) -> RgffiWalkerErr {
    if walker.is_null() || func.is_none() {
        return RgffiWalkerErr::NullErr;
    }

    let walker = unsafe { &*walker };
    let func = func.unwrap();

    let Some(first_path) = walker.paths.first() else {
        return RgffiWalkerErr::NullErr;
    };

    let mut builder = WalkBuilder::new(first_path);

    for path in &walker.paths[1..] {
        builder.add(path);
    }

    if let Some(cwd) = &walker.cwd {
        builder.current_dir(cwd);
    }

    if apply_opts(&mut builder, &walker.opts).is_err() {
        return RgffiWalkerErr::Err;
    }

    let had_error = AtomicBool::new(false);
    let payload_bits = payload as usize;

    builder.build_parallel().run(|| {
        let had_error = &had_error;

        Box::new(move |result| match result {
            Ok(dirent) => {
                let path = dirent.path().to_string_lossy().into_owned();

                let Ok(c_path) = CString::new(path) else {
                    had_error.store(true, Ordering::Relaxed);
                    return WalkState::Continue;
                };

                let filename = dirent.file_name().to_string_lossy();

                let path_bytes = c_path.as_bytes();
                let filename_bytes = filename.as_bytes();

                let filename = if !filename_bytes.is_empty() && path_bytes.ends_with(filename_bytes)
                {
                    unsafe { c_path.as_ptr().add(path_bytes.len() - filename_bytes.len()) }
                } else {
                    c_path.as_ptr()
                };

                let ino = dirent.ino().unwrap_or(0);

                let entry = RgffiWalkerEntry {
                    path: c_path.as_ptr(),
                    filename,
                    kind: entry_kind(&dirent),
                    depth: dirent.depth(),
                    ino,
                    flags: entry_flags(&dirent),
                };

                let payload = payload_bits as *mut c_void;
                let state = unsafe { func(&entry, payload) };

                map_walk_state(state)
            }
            Err(_) => {
                had_error.store(true, Ordering::Relaxed);
                WalkState::Continue
            }
        })
    });

    if had_error.load(Ordering::Relaxed) {
        RgffiWalkerErr::Err
    } else {
        RgffiWalkerErr::Ok
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rg_walker_free(walker: *mut RgffiWalker) {
    if walker.is_null() {
        return;
    }

    unsafe {
        drop(Box::from_raw(walker));
    }
}
