#ifndef RG_REGEX_H
#define RG_REGEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rg_regex_err {
    OK = 0,
    MATCH = 1,
    NO_MATCH = 2,
    REGEX_ERR = -1,
    NOT_ALLOWED_ERR = -2,
    INVALID_LINE_TERMINATOR_ERR = -3,
    BANNED_ERR = -4,
    NULL_ERR = -5,
    UTF8_ERR = -6,
} rg_regex_err_t;

typedef struct rg_regex_matcher_opts {
    bool case_insensitive;
    bool case_smart;
    bool multi_line;
    bool dot_matches_new_line;
    bool swap_greed;
    bool ignore_whitespace;
    bool unicode;
    bool octal;
    bool crlf;
    bool word;
    bool fixed_strings;
    bool whole_line;
    bool has_line_terminator;
    bool has_ban_byte;
    size_t size_limit;
    size_t dfa_size_limit;
    uint32_t nest_limit;
    uint8_t line_terminator;
    uint8_t ban_byte;
} rg_regex_matcher_opts_t;

typedef struct rg_matcher rg_matcher_t;

typedef struct rg_regex_match {
    size_t start;
    size_t end;
} rg_regex_match_t;

typedef struct rg_regex_captures rg_regex_captures_t;
rg_regex_err_t rg_regex_captures_get(
    rg_regex_captures_t* capture, size_t i, rg_regex_match_t* match
);
void rg_regex_captures_free(rg_regex_captures_t* capture);

rg_regex_err_t rg_regex_default_opts(rg_regex_matcher_opts_t* opts);
rg_regex_err_t rg_regex_matcher_build(
    rg_matcher_t** matcher,
    const rg_regex_matcher_opts_t* opts,
    const char* patterns[],
    size_t patterns_len,
    bool is_literal
);

void rg_regex_matcher_free(rg_matcher_t* matcher);

rg_regex_err_t rg_regex_matcher_find_at(
    rg_matcher_t* matcher,
    rg_regex_match_t* match,
    const char* haystack,
    size_t haystack_len,
    size_t at
);
rg_regex_err_t rg_regex_matcher_find(
    rg_matcher_t* matcher,
    rg_regex_match_t* match,
    const char* haystack,
    size_t haystack_len
);

rg_regex_err_t rg_regex_matcher_captures(
    rg_matcher_t* matcher, const char* haystack, rg_regex_captures_t* captures[]
);
rg_regex_err_t rg_regex_matcher_new_captures(
    const rg_matcher_t* matcher, rg_regex_captures_t** captures
);

rg_regex_err_t rg_regex_matcher_captures_at(
    const rg_matcher_t* matcher,
    const uint8_t* haystack,
    size_t haystack_len,
    size_t at,
    rg_regex_captures_t* captures
);

size_t rg_regex_matcher_capture_count(const rg_matcher_t* matcher);

rg_regex_err_t rg_regex_matcher_replace(
    const rg_matcher_t* matcher,
    const uint8_t* haystack,
    size_t haystack_len,
    const uint8_t* replacement,
    size_t replacement_len
);

rg_regex_err_t rg_regex_matcher_is_match(
    rg_matcher_t* matcher, const char* haystack, size_t haystack_len
);
rg_regex_err_t rg_regex_matcher_is_match_at(
    rg_matcher_t* matcher, const char* haystack, size_t haystack_len, size_t at
);
rg_regex_err_t rg_regex_matcher_shortest_match(
    rg_matcher_t* matcher,
    const char* haystack,
    size_t haystack_len,
    size_t* out_end
);

const char* rg_regex_version(void);

#ifdef __cplusplus
}
#endif

#endif // RG_REGEX_H
