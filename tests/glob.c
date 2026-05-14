#include "kritic.h"
#include "rg_glob.h"

#include <stddef.h>
#include <string.h>

static void make_default_opts(rg_glob_opts_t* opts) {
    rg_glob_default_opts(opts);
}

static void make_case_insensitive_opts(rg_glob_opts_t* opts) {
    rg_glob_default_opts(opts);
    opts->case_insensitive = true;
}

KRITIC_TEST(glob, version) {
    const char* version = rg_glob_version();

    KRITIC_ASSERT(version != NULL);
    KRITIC_ASSERT(strlen(version) > 0);
}

KRITIC_TEST(glob, default_opts) {
    rg_glob_opts_t opts;

    KRITIC_ASSERT_EQ_INT(rg_glob_default_opts(&opts), RG_GLOB_OK);

    KRITIC_ASSERT_NOT(opts.case_insensitive);
    KRITIC_ASSERT_NOT(opts.literal_separator);
    KRITIC_ASSERT_NOT(opts.empty_alternates);
    KRITIC_ASSERT_NOT(opts.allow_unclosed_class);
}

KRITIC_TEST(glob, default_opts_null_fails) {
    KRITIC_ASSERT_EQ_INT(rg_glob_default_opts(NULL), RG_GLOB_NULL_ERR);
}

KRITIC_TEST(glob, build_empty_matcher) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, NULL, 0), RG_GLOB_OK
    );

    KRITIC_ASSERT(matcher != NULL);

    size_t len = 999;
    KRITIC_ASSERT_EQ_INT(rg_glob_matcher_len(matcher, &len), RG_GLOB_OK);
    KRITIC_ASSERT_EQ(len, (size_t) 0);

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_is_match(matcher, "src/main.c"), RG_GLOB_NO_MATCH
    );

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, build_single_matcher) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    KRITIC_ASSERT(matcher != NULL);

    size_t len = 0;
    KRITIC_ASSERT_EQ_INT(rg_glob_matcher_len(matcher, &len), RG_GLOB_OK);
    KRITIC_ASSERT_EQ(len, (size_t) 1);

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, build_many_matcher) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
        "*.h",
        "*.rs",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 3), RG_GLOB_OK
    );

    KRITIC_ASSERT(matcher != NULL);

    size_t len = 0;
    KRITIC_ASSERT_EQ_INT(rg_glob_matcher_len(matcher, &len), RG_GLOB_OK);
    KRITIC_ASSERT_EQ(len, (size_t) 3);

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, build_null_out_fails) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(NULL, &opts, patterns, 1), RG_GLOB_NULL_ERR
    );
}

KRITIC_TEST(glob, build_null_patterns_with_nonzero_len_fails) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, NULL, 1), RG_GLOB_NULL_ERR
    );

    KRITIC_ASSERT(matcher == NULL);
}

KRITIC_TEST(glob, build_null_pattern_entry_fails) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
        NULL,
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 2), RG_GLOB_NULL_ERR
    );

    KRITIC_ASSERT(matcher == NULL);
}

KRITIC_TEST(glob, invalid_pattern_fails) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "[",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_NE_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    KRITIC_ASSERT(matcher == NULL);
}

KRITIC_TEST(
    glob, is_match_single, KRITIC_DEPENDS_ON(glob, build_single_matcher)
) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_is_match(matcher, "main.c"), RG_GLOB_MATCH
    );

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(
    glob, no_match_single, KRITIC_DEPENDS_ON(glob, build_single_matcher)
) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_is_match(matcher, "main.rs"), RG_GLOB_NO_MATCH
    );

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, is_match_many, KRITIC_DEPENDS_ON(glob, build_many_matcher)) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
        "*.h",
        "*.rs",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 3), RG_GLOB_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_is_match(matcher, "lib.rs"), RG_GLOB_MATCH
    );

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, no_match_many, KRITIC_DEPENDS_ON(glob, build_many_matcher)) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
        "*.h",
        "*.rs",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 3), RG_GLOB_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_is_match(matcher, "README.md"), RG_GLOB_NO_MATCH
    );

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, case_insensitive_match) {
    rg_glob_opts_t opts;
    make_case_insensitive_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_is_match(matcher, "MAIN.C"), RG_GLOB_MATCH
    );

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(
    glob, matches_single, KRITIC_DEPENDS_ON(glob, build_single_matcher)
) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    size_t matches[1] = {999};
    size_t matches_len = 0;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_matches(matcher, "main.c", matches, 1, &matches_len),
        RG_GLOB_MATCH
    );

    KRITIC_ASSERT_EQ(matches_len, (size_t) 1);
    KRITIC_ASSERT_EQ(matches[0], (size_t) 0);

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, matches_many, KRITIC_DEPENDS_ON(glob, build_many_matcher)) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
        "main.*",
        "*.h",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 3), RG_GLOB_OK
    );

    size_t matches[3] = {999, 999, 999};
    size_t matches_len = 0;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_matches(matcher, "main.c", matches, 3, &matches_len),
        RG_GLOB_MATCH
    );

    KRITIC_ASSERT_EQ(matches_len, (size_t) 2);
    KRITIC_ASSERT_EQ(matches[0], (size_t) 0);
    KRITIC_ASSERT_EQ(matches[1], (size_t) 1);

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(
    glob, matches_no_match, KRITIC_DEPENDS_ON(glob, build_many_matcher)
) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
        "*.h",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 2), RG_GLOB_OK
    );

    size_t matches[2] = {999, 999};
    size_t matches_len = 999;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_matches(matcher, "main.rs", matches, 2, &matches_len),
        RG_GLOB_NO_MATCH
    );

    KRITIC_ASSERT_EQ(matches_len, (size_t) 0);
    KRITIC_ASSERT_EQ(matches[0], (size_t) 999);
    KRITIC_ASSERT_EQ(matches[1], (size_t) 999);

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, matches_buffer_too_small_single) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    size_t matches_len = 0;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_matches(matcher, "main.c", NULL, 0, &matches_len),
        RG_GLOB_BUFFER_TOO_SMALL_ERR
    );

    KRITIC_ASSERT_EQ(matches_len, (size_t) 1);

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, matches_buffer_too_small_many) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
        "main.*",
        "*.h",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 3), RG_GLOB_OK
    );

    size_t matches[1] = {999};
    size_t matches_len = 0;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_matches(matcher, "main.c", matches, 1, &matches_len),
        RG_GLOB_BUFFER_TOO_SMALL_ERR
    );

    KRITIC_ASSERT_EQ(matches_len, (size_t) 2);

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, matches_null_len_fails) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    size_t matches[1] = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_matches(matcher, "main.c", matches, 1, NULL),
        RG_GLOB_NULL_ERR
    );

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, is_match_null_fails) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_is_match(NULL, "main.c"), RG_GLOB_NULL_ERR
    );

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_is_match(matcher, NULL), RG_GLOB_NULL_ERR
    );

    rg_glob_matcher_free(matcher);
}

KRITIC_TEST(glob, len_null_fails) {
    rg_glob_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {
        "*.c",
    };

    rg_glob_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_glob_matcher_build(&matcher, &opts, patterns, 1), RG_GLOB_OK
    );

    size_t len = 0;

    KRITIC_ASSERT_EQ_INT(rg_glob_matcher_len(NULL, &len), RG_GLOB_NULL_ERR);

    KRITIC_ASSERT_EQ_INT(rg_glob_matcher_len(matcher, NULL), RG_GLOB_NULL_ERR);

    rg_glob_matcher_free(matcher);
}
