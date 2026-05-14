#define _XOPEN_SOURCE 700

#include "kritic.h"
#include "rg_walker.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static void make_default_opts(rg_walker_opts_t* opts) {
    rg_walker_default_opts(opts);
}

typedef struct walk_seen {
    size_t count;
    size_t dirs;
    size_t files;
    size_t symlinks;

    bool saw_root;
    bool saw_a_txt;
    bool saw_b_rs;
    bool saw_sub;
    bool saw_c_txt;
    bool saw_hidden;
    bool saw_ignored;
    bool saw_link;

    size_t max_depth_seen;
} walk_seen_t;

static bool streq(const char* a, const char* b) {
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static void update_seen(walk_seen_t* seen, const rg_walker_entry_t* entry) {
    seen->count++;

    if (entry->kind == RG_WALKER_DIR) {
        seen->dirs++;
    } else if (entry->kind == RG_WALKER_FILE) {
        seen->files++;
    } else if (entry->kind == RG_WALKER_SYMLINK) {
        seen->symlinks++;
    }

    if (entry->depth > seen->max_depth_seen) {
        seen->max_depth_seen = entry->depth;
    }

    if (entry->depth == 0) {
        seen->saw_root = true;
    }

    if (streq(entry->filename, "a.txt")) {
        seen->saw_a_txt = true;
    } else if (streq(entry->filename, "b.rs")) {
        seen->saw_b_rs = true;
    } else if (streq(entry->filename, "sub")) {
        seen->saw_sub = true;
    } else if (streq(entry->filename, "c.txt")) {
        seen->saw_c_txt = true;
    } else if (streq(entry->filename, ".hidden")) {
        seen->saw_hidden = true;
    } else if (streq(entry->filename, "ignored.txt")) {
        seen->saw_ignored = true;
    } else if (streq(entry->filename, "alink")) {
        seen->saw_link = true;
    }
}

static rg_walker_walkstate_t collect_cb(
    const rg_walker_entry_t* entry,
    void* payload
) {
    KRITIC_ASSERT(entry != NULL);
    KRITIC_ASSERT(entry->path != NULL);
    KRITIC_ASSERT(entry->filename != NULL);

    walk_seen_t* seen = payload;
    update_seen(seen, entry);

    return RG_WALKER_CONTINUE;
}

static rg_walker_walkstate_t skip_sub_cb(
    const rg_walker_entry_t* entry,
    void* payload
) {
    KRITIC_ASSERT(entry != NULL);

    walk_seen_t* seen = payload;
    update_seen(seen, entry);

    if (entry->kind == RG_WALKER_DIR && streq(entry->filename, "sub")) {
        return RG_WALKER_SKIP;
    }

    return RG_WALKER_CONTINUE;
}

static rg_walker_walkstate_t quit_after_first_cb(
    const rg_walker_entry_t* entry,
    void* payload
) {
    KRITIC_ASSERT(entry != NULL);

    walk_seen_t* seen = payload;
    update_seen(seen, entry);

    return RG_WALKER_QUIT;
}

static rg_walker_walkstate_t validate_entry_cb(
    const rg_walker_entry_t* entry,
    void* payload
) {
    (void) payload;

    KRITIC_ASSERT(entry != NULL);
    KRITIC_ASSERT(entry->path != NULL);
    KRITIC_ASSERT(entry->filename != NULL);

    KRITIC_ASSERT(
        entry->kind == RG_WALKER_UNKNOWN ||
        entry->kind == RG_WALKER_FILE ||
        entry->kind == RG_WALKER_DIR ||
        entry->kind == RG_WALKER_SYMLINK
    );

    if (entry->kind == RG_WALKER_SYMLINK) {
        KRITIC_ASSERT(entry->flags & RG_WALKER_FLAG_SYMLINK);
    }

    return RG_WALKER_CONTINUE;
}

#ifndef _WIN32

static void make_temp_dir(char* out, size_t out_len) {
    char templ[512];

    snprintf(
        templ,
        sizeof(templ),
        "/tmp/rg_walker_test_%ld_XXXXXX",
        (long) getpid()
    );

    char* made = mkdtemp(templ);

    KRITIC_ASSERT(made != NULL);
    KRITIC_ASSERT(strlen(made) + 1 <= out_len);

    strcpy(out, made);
}

static void join_path(
    char* out,
    size_t out_len,
    const char* root,
    const char* name
) {
    snprintf(out, out_len, "%s/%s", root, name);
}

static void write_text_file(
    const char* root,
    const char* name,
    const char* text
) {
    char path[1024];
    join_path(path, sizeof(path), root, name);

    FILE* file = fopen(path, "wb");

    KRITIC_ASSERT(file != NULL);
    KRITIC_ASSERT(fwrite(text, 1, strlen(text), file) == strlen(text));
    KRITIC_ASSERT_EQ_INT(fclose(file), 0);
}

static void make_dir(const char* root, const char* name) {
    char path[1024];
    join_path(path, sizeof(path), root, name);

    KRITIC_ASSERT_EQ_INT(mkdir(path, 0700), 0);
}

static void cleanup_temp_dir(const char* root) {
    char cmd[1200];

    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
    (void) system(cmd);
}

static void make_fixture(char* root, size_t root_len) {
    make_temp_dir(root, root_len);

    write_text_file(root, "a.txt", "aaa\n");
    write_text_file(root, "b.rs", "bbb\n");
    write_text_file(root, ".hidden", "hidden\n");
    write_text_file(root, "ignored.txt", "ignored\n");

    make_dir(root, "sub");
    write_text_file(root, "sub/c.txt", "ccc\n");
}

#endif

KRITIC_TEST(walker, version) {
    const char* version = rg_walker_version();

    KRITIC_ASSERT(version != NULL);
    KRITIC_ASSERT(strlen(version) > 0);
}

KRITIC_TEST(walker, default_opts) {
    rg_walker_opts_t opts;

    KRITIC_ASSERT_EQ_INT(rg_walker_default_opts(&opts), RG_WALKER_OK);

    KRITIC_ASSERT_EQ(opts.min_depth, (size_t) -1);
    KRITIC_ASSERT_EQ(opts.max_depth, (size_t) -1);
    KRITIC_ASSERT_EQ(opts.max_filesize, (size_t) -1);
    KRITIC_ASSERT_EQ(opts.threads, (size_t) 0);

    KRITIC_ASSERT(opts.ignore_file == NULL);

    KRITIC_ASSERT_NOT(opts.follow_links);
    KRITIC_ASSERT(opts.hidden);
    KRITIC_ASSERT(opts.parents);
    KRITIC_ASSERT(opts.ignore);
    KRITIC_ASSERT(opts.git_global);
    KRITIC_ASSERT(opts.git_ignore);
    KRITIC_ASSERT(opts.git_exclude);
    KRITIC_ASSERT_NOT(opts.require_git);
    KRITIC_ASSERT_NOT(opts.ignore_case_insensitive);
    KRITIC_ASSERT_NOT(opts.same_file_system);
    KRITIC_ASSERT_NOT(opts.skip_stdout);
}

KRITIC_TEST(walker, default_opts_null_fails) {
    KRITIC_ASSERT_EQ_INT(rg_walker_default_opts(NULL), RG_WALKER_NULL_ERR);
}

KRITIC_TEST(walker, build_null_out_fails) {
    rg_walker_opts_t opts;
    make_default_opts(&opts);

    const char* paths[] = {
        ".",
    };

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(NULL, &opts, NULL, paths, 1),
        RG_WALKER_NULL_ERR
    );
}

KRITIC_TEST(walker, build_null_opts_fails) {
    rg_walker_t* walker = NULL;

    const char* paths[] = {
        ".",
    };

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, NULL, NULL, paths, 1),
        RG_WALKER_NULL_ERR
    );

    KRITIC_ASSERT(walker == NULL);
}

KRITIC_TEST(walker, build_null_paths_fails) {
    rg_walker_opts_t opts;
    make_default_opts(&opts);

    rg_walker_t* walker = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, NULL, 1),
        RG_WALKER_NULL_ERR
    );

    KRITIC_ASSERT(walker == NULL);
}

KRITIC_TEST(walker, build_zero_paths_fails) {
    rg_walker_opts_t opts;
    make_default_opts(&opts);

    rg_walker_t* walker = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, NULL, 0),
        RG_WALKER_NULL_ERR
    );

    KRITIC_ASSERT(walker == NULL);
}

KRITIC_TEST(walker, build_null_path_entry_fails) {
    rg_walker_opts_t opts;
    make_default_opts(&opts);

    const char* paths[] = {
        ".",
        NULL,
    };

    rg_walker_t* walker = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 2),
        RG_WALKER_NULL_ERR
    );

    KRITIC_ASSERT(walker == NULL);
}

KRITIC_TEST(walker, run_null_fails) {
    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(NULL, collect_cb, NULL),
        RG_WALKER_NULL_ERR
    );
}

#ifndef _WIN32

KRITIC_TEST(walker, build_and_free) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT(walker != NULL);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, run_null_callback_fails) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, NULL, NULL),
        RG_WALKER_NULL_ERR
    );

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, walk_basic_entries) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;
    walk_seen_t seen = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, collect_cb, &seen),
        RG_WALKER_OK
    );

    KRITIC_ASSERT(seen.saw_root);
    KRITIC_ASSERT(seen.saw_a_txt);
    KRITIC_ASSERT(seen.saw_b_rs);
    KRITIC_ASSERT(seen.saw_sub);
    KRITIC_ASSERT(seen.saw_c_txt);

    KRITIC_ASSERT_NOT(seen.saw_hidden);

    KRITIC_ASSERT(seen.count >= 5);
    KRITIC_ASSERT(seen.files >= 3);
    KRITIC_ASSERT(seen.dirs >= 2);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, entry_fields_are_valid) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, validate_entry_cb, NULL),
        RG_WALKER_OK
    );

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, hidden_files_skipped_by_default) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;
    walk_seen_t seen = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, collect_cb, &seen),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_NOT(seen.saw_hidden);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, hidden_files_can_be_included) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;
    opts.hidden = false;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;
    walk_seen_t seen = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, collect_cb, &seen),
        RG_WALKER_OK
    );

    KRITIC_ASSERT(seen.saw_hidden);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, max_depth_limits_recursion) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;
    opts.max_depth = 1;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;
    walk_seen_t seen = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, collect_cb, &seen),
        RG_WALKER_OK
    );

    KRITIC_ASSERT(seen.saw_a_txt);
    KRITIC_ASSERT(seen.saw_b_rs);
    KRITIC_ASSERT(seen.saw_sub);
    KRITIC_ASSERT_NOT(seen.saw_c_txt);
    KRITIC_ASSERT(seen.max_depth_seen <= 1);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, min_depth_skips_root) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;
    opts.min_depth = 1;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;
    walk_seen_t seen = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, collect_cb, &seen),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_NOT(seen.saw_root);
    KRITIC_ASSERT(seen.saw_a_txt);
    KRITIC_ASSERT(seen.saw_b_rs);
    KRITIC_ASSERT(seen.saw_sub);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, callback_skip_skips_directory_children) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;
    walk_seen_t seen = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, skip_sub_cb, &seen),
        RG_WALKER_OK
    );

    KRITIC_ASSERT(seen.saw_sub);
    KRITIC_ASSERT_NOT(seen.saw_c_txt);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, callback_quit_stops_walk) {
    char root[512];
    make_fixture(root, sizeof(root));

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;
    walk_seen_t seen = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, quit_after_first_cb, &seen),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ(seen.count, (size_t) 1);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, custom_ignore_file_skips_matching_file) {
    char root[512];
    make_fixture(root, sizeof(root));

    write_text_file(root, "ignore.rules", "ignored.txt\n");

    char ignore_path[1024];
    join_path(ignore_path, sizeof(ignore_path), root, "ignore.rules");

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;
    opts.ignore_file = ignore_path;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;
    walk_seen_t seen = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, collect_cb, &seen),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_NOT(seen.saw_ignored);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

KRITIC_TEST(walker, symlink_sets_flag) {
    char root[512];
    make_fixture(root, sizeof(root));

    char target[1024];
    char link_path[1024];

    join_path(target, sizeof(target), root, "a.txt");
    join_path(link_path, sizeof(link_path), root, "alink");

    KRITIC_ASSERT_EQ_INT(symlink(target, link_path), 0);

    rg_walker_opts_t opts;
    make_default_opts(&opts);
    opts.threads = 1;

    const char* paths[] = {
        root,
    };

    rg_walker_t* walker = NULL;
    walk_seen_t seen = {0};

    KRITIC_ASSERT_EQ_INT(
        rg_walker_build(&walker, &opts, NULL, paths, 1),
        RG_WALKER_OK
    );

    KRITIC_ASSERT_EQ_INT(
        rg_walker_run(walker, collect_cb, &seen),
        RG_WALKER_OK
    );

    KRITIC_ASSERT(seen.saw_link);

    rg_walker_free(walker);
    cleanup_temp_dir(root);
}

#endif
