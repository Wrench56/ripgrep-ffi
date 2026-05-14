#define CF_DISABLE_FILE_HASH

#include "cforge.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TEST_DIR "tests"
#define TEST_BUILD_DIR "build/tests"
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

    CF_SET_ENV(ldflags, "-ldl -lpthread -lm");
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
    CF_NOP();
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

    for
        CF_GLOBS_EACH(TEST_DIR "/*.c", test_src) {
            char* bin = CF_MAP(
                test_src, CF_MAP_DIRS(TEST_BUILD_DIR "/"), CF_MAP_EXT("")
            );

            CF_BANNER(CC_TAG "Compiling tests...");
            printf(CC_TAG "  %s -> %s\n", test_src, bin);

            CF_RUNP(
                "cc %s %s %s " KRITIC_LIB " %s -o %s",
                CF_ENV(cflags),
                test_src,
                rg_lib(),
                CF_ENV(ldflags),
                bin
            );
        }
}

CF_TARGET(test, CF_DEPENDS(tests), CF_HELP_STRING("Run all compiled tests")) {
    for
        CF_GLOBS_EACH(TEST_BUILD_DIR "/*", bin) {
            printf(RN_TAG "Running %s...\n", bin);
            CF_RUN("./%s", bin);
        }
}

CF_TARGET(clean, CF_HELP_STRING("Clean build outputs")) {
    CF_RUNP("%s", "cargo clean");
    CF_RUNP("%s", "make -C " KRITIC_DIR " clean");
    CF_REMOVE(TEST_BUILD_DIR);
    CF_REMOVE(".cforge.db");
}
