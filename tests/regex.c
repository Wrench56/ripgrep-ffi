#include "kritic.h"
#include "rg_regex.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void make_default_opts(rg_regex_matcher_opts_t* opts) {
    rg_regex_default_opts(opts);
    opts->case_insensitive = true;
    opts->unicode = true;
}

KRITIC_TEST(regex, version) {
    const char* version = rg_regex_version();

    KRITIC_ASSERT(version != NULL);
    KRITIC_ASSERT(strlen(version) > 0);
}

KRITIC_TEST(regex, default_opts) {
    rg_regex_matcher_opts_t opts;
    rg_regex_default_opts(&opts);

    KRITIC_ASSERT(opts.unicode);
    KRITIC_ASSERT_NOT(opts.case_insensitive);
    KRITIC_ASSERT_NOT(opts.fixed_strings);
}

KRITIC_TEST(regex, build_matcher) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {"h([a-z]+)o"};
    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, patterns, 1, false), RG_REGEX_OK
    );

    KRITIC_ASSERT(matcher != NULL);

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, build_many) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {"hello", "world", "test[0-9]+"};
    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, patterns, 3, false), RG_REGEX_OK
    );

    KRITIC_ASSERT(matcher != NULL);

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(
            matcher, "123 WORLD 456", strlen("123 WORLD 456")
        ),
        RG_REGEX_MATCH
    );

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(
            matcher, "abc test42 xyz", strlen("abc test42 xyz")
        ),
        RG_REGEX_MATCH
    );

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(
            matcher, "nothing here", strlen("nothing here")
        ),
        RG_REGEX_NO_MATCH
    );

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, build_literals) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {"a+b", "hello.world"};
    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, patterns, 2, true), RG_REGEX_OK
    );

    KRITIC_ASSERT(matcher != NULL);

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(matcher, "xx a+b yy", strlen("xx a+b yy")),
        RG_REGEX_MATCH
    );

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(
            matcher, "xx hello.world yy", strlen("xx hello.world yy")
        ),
        RG_REGEX_MATCH
    );

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(matcher, "xx aaab yy", strlen("xx aaab yy")),
        RG_REGEX_NO_MATCH
    );

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, find_at, KRITIC_DEPENDS_ON(regex, build_matcher)) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {"h([a-z]+)o"};
    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, patterns, 1, false), RG_REGEX_OK
    );

    const char* text = "123 HeLLo 456";
    rg_regex_match_t m = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_find_at(matcher, &m, text, strlen(text), 0),
        RG_REGEX_MATCH
    );

    KRITIC_ASSERT_EQ(m.start, (size_t) 4);
    KRITIC_ASSERT_EQ(m.end, (size_t) 9);
    KRITIC_ASSERT_EQ_INT(strncmp(text + m.start, "HeLLo", m.end - m.start), 0);

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, is_match, KRITIC_DEPENDS_ON(regex, build_matcher)) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {"h([a-z]+)o"};
    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, patterns, 1, false), RG_REGEX_OK
    );

    const char* text = "123 HeLLo 456";

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(matcher, text, strlen(text)), RG_REGEX_MATCH
    );

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, no_match, KRITIC_DEPENDS_ON(regex, build_matcher)) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {"h([a-z]+)o"};
    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, patterns, 1, false), RG_REGEX_OK
    );

    const char* text = "nothing here";

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(matcher, text, strlen(text)),
        RG_REGEX_NO_MATCH
    );

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, shortest_match, KRITIC_DEPENDS_ON(regex, build_matcher)) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {"h([a-z]+)o"};
    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, patterns, 1, false), RG_REGEX_OK
    );

    const char* text = "123 HeLLo 456";
    size_t shortest = 0;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_shortest_match(matcher, text, strlen(text), &shortest),
        RG_REGEX_MATCH
    );

    KRITIC_ASSERT_EQ(shortest, (size_t) 9);

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, captures, KRITIC_DEPENDS_ON(regex, build_matcher)) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {"h([a-z]+)o"};
    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, patterns, 1, false), RG_REGEX_OK
    );

    const char* text = "123 HeLLo 456";

    rg_regex_captures_t* caps = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_new_captures(matcher, &caps), RG_REGEX_OK
    );

    KRITIC_ASSERT(caps != NULL);

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_captures_at(
            matcher, (const uint8_t*) text, strlen(text), 0, caps
        ),
        RG_REGEX_MATCH
    );

    size_t count = rg_regex_matcher_capture_count(matcher);

    KRITIC_ASSERT_EQ(count, (size_t) 2);

    rg_regex_match_t whole = {0};
    rg_regex_match_t group = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_regex_captures_get(caps, 0, &whole), RG_REGEX_MATCH
    );
    KRITIC_ASSERT_EQ_INT(
        rg_regex_captures_get(caps, 1, &group), RG_REGEX_MATCH
    );

    KRITIC_ASSERT_EQ(whole.start, (size_t) 4);
    KRITIC_ASSERT_EQ(whole.end, (size_t) 9);
    KRITIC_ASSERT_EQ_INT(
        strncmp(text + whole.start, "HeLLo", whole.end - whole.start), 0
    );

    KRITIC_ASSERT_EQ(group.start, (size_t) 5);
    KRITIC_ASSERT_EQ(group.end, (size_t) 8);
    KRITIC_ASSERT_EQ_INT(
        strncmp(text + group.start, "eLL", group.end - group.start), 0
    );

    rg_regex_captures_free(caps);
    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, invalid_pattern_fails) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    const char* patterns[] = {"("};
    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_NE_INT(
        rg_regex_matcher_build(&matcher, &opts, patterns, 1, false), RG_REGEX_OK
    );

    KRITIC_ASSERT(matcher == NULL);
}

KRITIC_TEST(regex, empty_patterns_fail) {
    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_NE_INT(
        rg_regex_matcher_build(&matcher, &opts, NULL, 0, false), RG_REGEX_OK
    );

    KRITIC_ASSERT(matcher == NULL);
}
