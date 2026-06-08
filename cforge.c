#define CF_DISABLE_FILE_HASH

#include "cforge.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TEST_DIR "tests"
#define TEST_BUILD_DIR "build/tests"
#define TEST_BIN TEST_BUILD_DIR "/all"
#define INCLUDE_DIR "includes"
#define KRITIC_DIR "extern/kritic"
#define KRITIC_LIB KRITIC_DIR "/build/libkritic.a"

#define CC_TAG "[  " CF_BOLD CF_YELLOW "CC" CF_RESET "  ] "
#define RN_TAG "[  " CF_BOLD CF_GREEN "RN" CF_RESET "  ] "
#define IF_TAG "[  " CF_BOLD CF_CYAN "IF" CF_RESET "  ] "
#define KT_TAG "[  " CF_BOLD CF_MAGENTA "KT" CF_RESET "  ] "

#if !CF_VERSION_AT_LEAST(1, 0, 0) || !CF_VERSION_BELOW(2, 0, 0)
#error "CForge version invalid"
#endif

static inline const char* rg_lib(void) {
    const char* mode = CF_ENV(mode);

    if (mode != NULL && strcmp(mode, "release") == 0) {
        return "target/release/libripgrep_ffi.a";
    }

    return "target/debug/libripgrep_ffi.a";
}

static inline const char* cargo_flags(void) {
    const char* mode = CF_ENV(mode);

    if (mode != NULL && strcmp(mode, "release") == 0) {
        return "--release";
    }

    return "";
}

CF_CONFIG(debug) {
    CF_SET_ENV(mode, "debug");

    CF_SET_ENV(
        cflags,
        "-I" INCLUDE_DIR " "
        "-I" KRITIC_DIR " "
        "-Wall -Wextra -Wpedantic "
        "-std=c11 "
        "-g "
        "-fsanitize=undefined"
    );

    CF_SET_ENV(ldflags, "-ldl -lpthread -lm -fsanitize=undefined");
}

CF_CONFIG(release) {
    CF_SET_ENV(mode, "release");

    CF_SET_ENV(
        cflags,
        "-I" INCLUDE_DIR " "
        "-I" KRITIC_DIR " "
        "-Wall -Wextra -Wpedantic "
        "-O2 "
        "-std=c11"
    );

    CF_SET_ENV(ldflags, "-ldl -lpthread -lm");
}

CF_TARGET(
    debug,
    CF_WITH_CONFIG(debug),
    CF_DEPENDS(test),
    CF_HELP_STRING("Build and run tests in debug mode")
) {
    CF_NOP();
}

CF_TARGET(
    release,
    CF_WITH_CONFIG(release),
    CF_DEPENDS(test),
    CF_HELP_STRING("Build and run tests in release mode")
) {
    CF_RUN("%s", "strip --strip-unneeded target/release/libripgrep_ffi.a");
}

CF_TARGET(rgffi, CF_HELP_STRING("Build ripgrep FFI Rust static library")) {
    printf(IF_TAG "Building Rust library in %s mode...\n", CF_ENV(mode));
    CF_RUN("cargo build %s", cargo_flags());
}

CF_TARGET(kritic, CF_HELP_STRING("Build KritiC static library")) {
    if (CF_FILE_NOT_UTD(KRITIC_LIB)) {
        printf(KT_TAG "Building KritiC...\n");
        CF_RUN("%s", "make -C " KRITIC_DIR " static");
        CF_FILE_MARK_UTD(KRITIC_LIB);
    }
}

CF_TARGET(tests, CF_DEPENDS(rgffi), CF_DEPENDS(kritic), CF_HIDDEN) {
    CF_MKDIR(TEST_BUILD_DIR);

    cf_glob_t test_glob = CF_GLOB(TEST_DIR "/*.c");
    char* test_sources = CF_JOIN_GLOB(test_glob, " ");

    bool rebuild = CF_FILE_NOT_UTD(TEST_BIN) ||
                   CF_FILE_NOT_UTD((char*) rg_lib()) ||
                   CF_FILE_NOT_UTD(KRITIC_LIB);

    for
        CF_GLOBS_EACH(TEST_DIR "/*.c", test_src) {
            if (CF_FILE_NOT_UTD(test_src)) {
                rebuild = true;
                break;
            }
        }

    if (!rebuild) {
        return;
    }

    CF_BANNER("%s", CC_TAG "Compiling tests...");

    for (uint32_t i = 0; i < test_glob.c; i++) {
        printf(CC_TAG "  %s\n", test_glob.p[i]);
    }

    CF_RUN(
        "cc %s %s %s " KRITIC_LIB " %s -o %s",
        CF_ENV(cflags),
        test_sources,
        rg_lib(),
        CF_ENV(ldflags),
        TEST_BIN
    );

    for
        CF_GLOBS_EACH(TEST_DIR "/*.c", test_src) {
            CF_FILE_MARK_UTD(test_src);
        }

    CF_FILE_MARK_UTD((char*) rg_lib());
    CF_FILE_MARK_UTD(KRITIC_LIB);
    CF_FILE_MARK_UTD(TEST_BIN);
}

CF_TARGET(test, CF_DEPENDS(tests), CF_HELP_STRING("Run all compiled tests")) {
    printf(RN_TAG "Running %s...\n", TEST_BIN);
    CF_RUN("./%s", TEST_BIN);
}

CF_TARGET(clean, CF_HELP_STRING("Clean build outputs")) {
    CF_RUNP("%s", "cargo clean");
    CF_RUNP("%s", "make -C " KRITIC_DIR " clean");
    CF_RM(TEST_BUILD_DIR);
    CF_RM(".cforge.db");
}
