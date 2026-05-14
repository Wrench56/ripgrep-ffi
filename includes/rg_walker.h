#ifndef RG_WALKER_H
#define RG_WALKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rg_walker_err {
    RG_WALKER_OK = 0,
    RG_WALKER_ERR = -1,
    RG_WALKER_NULL_ERR = -2,
    RG_WALKER_UTF8_ERR = -3,
} rg_walker_err_t;

typedef enum rg_walker_walkstate {
    RG_WALKER_CONTINUE = 0,
    RG_WALKER_SKIP = 1,
    RG_WALKER_QUIT = 2,
} rg_walker_walkstate_t;

typedef enum rg_walker_entry_kind {
    RG_WALKER_UNKNOWN = 0,
    RG_WALKER_FILE = 1,
    RG_WALKER_DIR = 2,
    RG_WALKER_SYMLINK = 3,
} rg_walker_entry_kind_t;

typedef enum rg_walker_entry_flags {
    RG_WALKER_FLAG_NONE = 0,
    RG_WALKER_FLAG_STDIN = 1u << 1,
    RG_WALKER_FLAG_SYMLINK = 1u << 2,

    RG_WALKER_FLAG_BLOCK_DEVICE = 1u << 6,
    RG_WALKER_FLAG_CHAR_DEVICE = 1u << 7,
    RG_WALKER_FLAG_FIFO = 1u << 8,
    RG_WALKER_FLAG_SOCKET = 1u << 9,

    RG_WALKER_FLAG_SYMLINK_FILE = 1u << 14,
    RG_WALKER_FLAG_SYMLINK_DIR = 1u << 15,
} rg_walker_entry_flags_t;

typedef struct rg_walker_entry {
    const char* path;
    const char* filename;
    size_t depth;
    uint64_t ino;
    rg_walker_entry_kind_t kind;
    uint16_t flags;
} rg_walker_entry_t;

typedef struct rg_walker_opts {
    size_t min_depth;
    size_t max_depth;
    size_t max_filesize;
    size_t threads;
    const char* ignore_file;
    bool follow_links;
    bool hidden;
    bool parents;
    bool ignore;
    bool git_global;
    bool git_ignore;
    bool git_exclude;
    bool require_git;
    bool ignore_case_insensitive;
    bool same_file_system;
    bool skip_stdout;
} rg_walker_opts_t;

typedef rg_walker_walkstate_t (*rg_walker_func_t)(
    const rg_walker_entry_t* entry, void* payload
);

typedef struct rg_walker rg_walker_t;

rg_walker_err_t rg_walker_default_opts(rg_walker_opts_t* opts);

rg_walker_err_t rg_walker_build(
    rg_walker_t** walker,
    const rg_walker_opts_t* opts,
    const char* cwd,
    const char* const paths[],
    size_t paths_len
);

rg_walker_err_t rg_walker_run(
    rg_walker_t* walker, rg_walker_func_t func, void* payload
);

void rg_walker_free(rg_walker_t* walker);

const char* rg_walker_version(void);

#ifdef __cplusplus
}
#endif

#endif // RG_WALKER_H
