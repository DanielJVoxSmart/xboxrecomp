/*
 * test_config -- the settings file, parsed without a game.
 *
 * Covers the things that decide whether a player's choice actually
 * reaches the runtime: that a value is found, that the environment beats
 * the file, that a boolean reads the way the rest of the runtime reads
 * booleans, and that rubbish falls back instead of failing.
 */
#include "recomp_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void check(int ok, const char *what)
{
    if (!ok) {
        printf("  FAIL: %s\n", what);
        failures++;
    }
}

static void check_str(const char *got, const char *want, const char *what)
{
    int ok = (got && want) ? strcmp(got, want) == 0 : got == want;

    if (!ok) {
        printf("  FAIL: %s (got %s, want %s)\n", what,
               got ? got : "(null)", want ? want : "(null)");
        failures++;
    }
}

static void check_int(int got, int want, const char *what)
{
    if (got != want) {
        printf("  FAIL: %s (got %d, want %d)\n", what, got, want);
        failures++;
    }
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");

    if (!f) {
        printf("  FAIL: cannot write %s\n", path);
        failures++;
        return;
    }
    fputs(text, f);
    fclose(f);
}

int main(void)
{
    const char *path = "test_config_tmp.conf";

    write_file(path,
        "# a comment\n"
        "; another\n"
        "\n"
        "resolution_scale = 2\n"
        "widescreen = 1\n"
        "hor_plus=0.75\n"                 /* no spaces */
        "   anisotropy   =   16   \n"     /* lots of spaces */
        "frame_cap = adaptive\n"
        "empty =\n"
        "nonsense line with no equals\n"
        "unknown_key = 7\n");

    /* The named file wins outright, which is how the test reaches the
     * parser without a per-user directory or a title id. */
#if defined(_WIN32)
    _putenv_s("RECOMP_DISPLAY_CONFIG", path);
#else
    setenv("RECOMP_DISPLAY_CONFIG", path, 1);
#endif

    printf("test_config\n");

    check_str(recomp_config_lookup(NULL, "resolution_scale"), "2", "plain value");
    check_str(recomp_config_lookup(NULL, "hor_plus"), "0.75", "no spaces around =");
    check_str(recomp_config_lookup(NULL, "anisotropy"), "16", "spaces trimmed");
    check_str(recomp_config_lookup(NULL, "frame_cap"), "adaptive", "word value");
    check_str(recomp_config_lookup(NULL, "empty"), NULL, "empty value reads as absent");
    check_str(recomp_config_lookup(NULL, "missing"), NULL, "absent key");
    check(recomp_config_path() != NULL, "path reported");

    check_int(recomp_config_int(NULL, "resolution_scale", 1), 2, "int");
    check_int(recomp_config_int(NULL, "frame_cap", 99), 99, "int fallback on a word");
    check_int(recomp_config_int(NULL, "missing", 5), 5, "int fallback when absent");
    check(recomp_config_float(NULL, "hor_plus", 0.0) > 0.74 &&
          recomp_config_float(NULL, "hor_plus", 0.0) < 0.76, "float");

    /* The boolean is the one that decides widescreen, so it gets the
     * whole vocabulary the rest of the runtime accepts. */
    check_int(recomp_config_bool(NULL, "widescreen", 0), 1, "bool 1 from file");
    check_int(recomp_config_bool(NULL, "missing", 0), 0, "bool fallback off");
    check_int(recomp_config_bool(NULL, "missing", 1), 1, "bool fallback on");

    /* Environment beats the file, and an empty variable means on. */
#if defined(_WIN32)
    _putenv_s("RECOMP_TEST_SCALE", "4");
    _putenv_s("RECOMP_TEST_WIDE", "0");
    _putenv_s("RECOMP_TEST_EMPTY", "");
#else
    setenv("RECOMP_TEST_SCALE", "4", 1);
    setenv("RECOMP_TEST_WIDE", "0", 1);
    setenv("RECOMP_TEST_EMPTY", "", 1);
#endif
    check_str(recomp_config_lookup("RECOMP_TEST_SCALE", "resolution_scale"), "4",
              "environment beats the file");
    check_int(recomp_config_bool("RECOMP_TEST_WIDE", "widescreen", 0), 0,
              "environment off beats file on");
    check_int(recomp_config_bool("RECOMP_TEST_EMPTY", "widescreen", 0), 1,
              "empty environment variable means on");
    check_str(recomp_config_lookup("RECOMP_TEST_UNSET", "resolution_scale"), "2",
              "unset variable falls through to the file");

    remove(path);
    if (failures)
        printf("test_config: %d FAILURE(S)\n", failures);
    else
        printf("test_config: all checks passed\n");
    return failures ? 1 : 0;
}
