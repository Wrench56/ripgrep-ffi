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

    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, "h([a-z]+)o"), OK
    );

    KRITIC_ASSERT(matcher != NULL);

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, find_at, KRITIC_DEPENDS_ON(regex, build_matcher)) {

    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, "h([a-z]+)o"), OK
    );

    const char* text = "123 HeLLo 456";
    rg_regex_match_t m = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_find_at(matcher, &m, text, strlen(text), 0), MATCH
    );

    KRITIC_ASSERT_EQ(m.start, (size_t) 4);
    KRITIC_ASSERT_EQ(m.end, (size_t) 9);
    KRITIC_ASSERT_EQ_INT(strncmp(text + m.start, "HeLLo", m.end - m.start), 0);

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, is_match, KRITIC_DEPENDS_ON(regex, build_matcher)) {

    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, "h([a-z]+)o"), OK
    );

    const char* text = "123 HeLLo 456";

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(matcher, text, strlen(text)), MATCH
    );

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, no_match, KRITIC_DEPENDS_ON(regex, build_matcher)) {

    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, "h([a-z]+)o"), OK
    );

    const char* text = "nothing here";

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_is_match(matcher, text, strlen(text)), NO_MATCH
    );

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, shortest_match, KRITIC_DEPENDS_ON(regex, build_matcher)) {

    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, "h([a-z]+)o"), OK
    );

    const char* text = "123 HeLLo 456";
    size_t shortest = 0;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_shortest_match(matcher, text, strlen(text), &shortest),
        MATCH
    );

    KRITIC_ASSERT_EQ(shortest, (size_t) 9);

    rg_regex_matcher_free(matcher);
}

KRITIC_TEST(regex, captures, KRITIC_DEPENDS_ON(regex, build_matcher)) {

    rg_regex_matcher_opts_t opts;
    make_default_opts(&opts);

    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_build(&matcher, &opts, "h([a-z]+)o"), OK
    );

    const char* text = "123 HeLLo 456";

    rg_regex_captures_t* caps = NULL;

    KRITIC_ASSERT_EQ_INT(rg_regex_matcher_new_captures(matcher, &caps), OK);

    KRITIC_ASSERT(caps != NULL);

    KRITIC_ASSERT_EQ_INT(
        rg_regex_matcher_captures_at(
            matcher, (const uint8_t*) text, strlen(text), 0, caps
        ),
        MATCH
    );

    size_t count = rg_regex_matcher_capture_count(matcher);

    KRITIC_ASSERT_EQ(count, (size_t) 2);

    rg_regex_match_t whole = {0};
    rg_regex_match_t group = {0};

    KRITIC_ASSERT_EQ_INT(rg_regex_captures_get(caps, 0, &whole), MATCH);
    KRITIC_ASSERT_EQ_INT(rg_regex_captures_get(caps, 1, &group), MATCH);

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

    rg_matcher_t* matcher = NULL;

    KRITIC_ASSERT_NE_INT(rg_regex_matcher_build(&matcher, &opts, "("), OK);

    KRITIC_ASSERT(matcher == NULL);
}
