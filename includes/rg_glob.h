#ifndef RG_GLOB_H
#define RG_GLOB_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rg_glob_err {
    RG_GLOB_OK = 0,
    RG_GLOB_MATCH = 1,
    RG_GLOB_NO_MATCH = 2,

    RG_GLOB_ERR = -1,
    RG_GLOB_NULL_ERR = -2,
    RG_GLOB_UTF8_ERR = -3,
    RG_GLOB_BUFFER_TOO_SMALL_ERR = -4,
} rg_glob_err_t;

typedef struct rg_glob_opts {
    bool case_insensitive;
    bool literal_separator;
    bool backslash_escape;
    bool empty_alternates;
    bool allow_unclosed_class;
} rg_glob_opts_t;

typedef struct rg_glob_matcher rg_glob_matcher_t;

rg_glob_err_t rg_glob_default_opts(rg_glob_opts_t* opts);

rg_glob_err_t rg_glob_matcher_build(
    rg_glob_matcher_t** matcher,
    const rg_glob_opts_t* opts,
    const char* const patterns[],
    size_t patterns_len
);

rg_glob_err_t rg_glob_matcher_is_match(
    const rg_glob_matcher_t* matcher, const char* path
);

rg_glob_err_t rg_glob_matcher_matches(
    const rg_glob_matcher_t* matcher,
    const char* path,
    size_t* matches,
    size_t matches_cap,
    size_t* matches_len
);

rg_glob_err_t rg_glob_matcher_len(
    const rg_glob_matcher_t* matcher, size_t* len
);

void rg_glob_matcher_free(rg_glob_matcher_t* matcher);

const char* rg_glob_version(void);

#ifdef __cplusplus
}
#endif

#endif // RG_GLOB_H
