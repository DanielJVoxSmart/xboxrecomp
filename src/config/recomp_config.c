/*
 * recomp_config.c -- see recomp_config.h for what this is and why.
 *
 * The parser is deliberately small. Blank lines and lines starting with
 * '#' or ';' are comments, everything else is `key = value` with the
 * spaces optional, and a key it does not recognise is ignored rather than
 * refused -- a config written by a newer launcher must not stop an older
 * build from running.
 */
#include "recomp_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define RECOMP_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define RECOMP_MKDIR(p) mkdir((p), 0755)
#endif

#define MAX_ENTRIES 64
#define MAX_KEY     64
#define MAX_VALUE   512

typedef struct {
    char key[MAX_KEY];
    char value[MAX_VALUE];
} Entry;

static Entry   g_entries[MAX_ENTRIES];
static int     g_entry_count;
static int     g_loaded;
static uint32_t g_title_id;
static char    g_path[1024];
static int     g_have_path;

void recomp_config_set_title(uint32_t title_id)
{
    g_title_id = title_id;
}

/* ----------------------------------------------------------------- text */

/* Case-insensitive compare, spelled out rather than borrowed: stricmp is
 * MSVC's and strcasecmp is POSIX's, and this file is built for both. */
static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return *a == *b;
}

/* The words that mean off, everywhere in the runtime. Anything else,
 * including an empty string, means on. */
static int off_word(const char *v)
{
    return strcmp(v, "0") == 0 || ieq(v, "off") || ieq(v, "no") || ieq(v, "false");
}

static char *trim(char *s)
{
    char *end;

    while (*s && isspace((unsigned char)*s))
        s++;
    if (!*s)
        return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        *end-- = '\0';
    return s;
}

static void store(const char *key, const char *value)
{
    int i;

    for (i = 0; i < g_entry_count; i++)
        if (strcmp(g_entries[i].key, key) == 0) {
            snprintf(g_entries[i].value, sizeof g_entries[i].value, "%s", value);
            return;
        }
    if (g_entry_count >= MAX_ENTRIES)
        return;
    snprintf(g_entries[g_entry_count].key, MAX_KEY, "%s", key);
    snprintf(g_entries[g_entry_count].value, MAX_VALUE, "%s", value);
    g_entry_count++;
}

static void parse(FILE *f)
{
    char line[MAX_VALUE + MAX_KEY + 8];

    while (fgets(line, sizeof line, f)) {
        char *p = trim(line), *eq;

        if (!*p || *p == '#' || *p == ';')
            continue;
        eq = strchr(p, '=');
        if (!eq)
            continue;                    /* not a setting; say nothing */
        *eq = '\0';
        store(trim(p), trim(eq + 1));
    }
}

/* ----------------------------------------------------------------- paths */

/* Per-user, beside the input bindings, because a setting is a judgement
 * about this machine rather than about this copy of the game. */
static int user_dir(char *out, size_t n)
{
#if defined(_WIN32)
    const char *home = getenv("APPDATA");
    const char *tail = "\\xboxrecomp";
#else
    const char *home = getenv("XDG_CONFIG_HOME");
    const char *tail = "/xboxrecomp";
    char buf[512];

    if (!home || !*home) {
        const char *h = getenv("HOME");

        if (!h)
            return 0;
        snprintf(buf, sizeof buf, "%s/.config", h);
        home = buf;
    }
#endif
    if (!home || !*home)
        return 0;
    snprintf(out, n, "%s%s", home, tail);
    return 1;
}

static void title_file(char *out, size_t n, const char *dir)
{
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    if (g_title_id)
        snprintf(out, n, "%s%ctitles%c%08X.conf", dir, sep, sep, g_title_id);
    else
        snprintf(out, n, "%s%ctitles%cdefault.conf", dir, sep, sep);
}

static int readable(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;
    fclose(f);
    return 1;
}

/* A file that says what it is, written once if nothing is there. A player
 * who never opens the launcher still gets something they can read and
 * edit, and it documents the names the launcher writes. Best effort: a
 * read-only or missing directory is not an error, it just means there is
 * no file and the defaults stand. */
static void write_default(const char *dir, const char *path)
{
    char sub[1024];
    FILE *f;
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif

    RECOMP_MKDIR(dir);
    snprintf(sub, sizeof sub, "%s%ctitles", dir, sep);
    RECOMP_MKDIR(sub);

    f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f,
        "# Settings for this title, written by the launcher and read when the\n"
        "# game starts. Every line here can be overridden by the environment\n"
        "# variable named beside it, so a .bat or a command line still wins.\n"
        "#\n"
        "# Delete this file to go back to the defaults.\n"
        "\n"
        "# How much larger than the console the game is rendered, before being\n"
        "# filtered back down: supersampling. 1 is the console's own size. 2 is\n"
        "# a good default and costs little. Up to 8.        [RECOMP_RES_SCALE]\n"
        "resolution_scale = 1\n"
        "\n"
        "# Present at 16:9 rather than 4:3, and tell the game the console is\n"
        "# widescreen. Only right for a game with a widescreen mode of its own;\n"
        "# one without draws 4:3 and will look stretched.  [RECOMP_WIDESCREEN]\n"
        "widescreen = 0\n"
        "\n"
        "# Widen the camera's horizontal field of view to match, so you see\n"
        "# more to the sides instead of the same view stretched. 0.75 is the\n"
        "# 4:3-to-16:9 figure; 0 leaves the camera alone.     [RECOMP_HOR_PLUS]\n"
        "hor_plus = 0\n"
        "\n"
        "# Which vertex constant register holds the projection, for the line\n"
        "# above. Per game; 60 for TimeSplitters 2.       [RECOMP_HOR_PLUS_REG]\n"
        "hor_plus_register = 60\n"
        "\n"
        "# Sharpen textures seen at a glancing angle, 1 to 16. The flat layer\n"
        "# is left alone, which wants no filtering.               [RECOMP_ANISO]\n"
        "anisotropy = 1\n"
        "\n"
        "# Frame pacing: adaptive, 60, 30, or 0 for uncapped.  [RECOMP_FPS_CAP]\n"
        "frame_cap = adaptive\n"
        "\n"
        "# Show the frame rate from the moment the game starts. F9 toggles it\n"
        "# while playing either way.                      [RECOMP_FPS_OVERLAY]\n"
        "fps_overlay = 0\n"
        "\n"
        "# Where the game's files are, if they are not beside the executable.\n"
        "#                                                   [RECOMP_GAME_DIR]\n"
        "# game_dir =\n");
    fclose(f);
}

static void load(void)
{
    const char *env = getenv("RECOMP_DISPLAY_CONFIG");
    char dir[1024], path[1024];
    FILE *f;

    g_loaded = 1;

    /* Named outright: use it or say so, and do not quietly fall back --
     * a mistyped path should not look like the settings were ignored. */
    if (env && *env) {
        f = fopen(env, "rb");
        if (f) {
            parse(f);
            fclose(f);
            snprintf(g_path, sizeof g_path, "%s", env);
            g_have_path = 1;
        } else {
            fprintf(stderr, "[CONFIG] RECOMP_DISPLAY_CONFIG=%s cannot be read; "
                            "using the defaults\n", env);
        }
        return;
    }

    if (user_dir(dir, sizeof dir)) {
        title_file(path, sizeof path, dir);
        if (!readable(path))
            write_default(dir, path);
        f = fopen(path, "rb");
        if (f) {
            parse(f);
            fclose(f);
            snprintf(g_path, sizeof g_path, "%s", path);
            g_have_path = 1;
            return;
        }
    }

    /* Beside the executable, which is where a portable copy would keep
     * it -- the working directory a launcher starts the game in. */
    f = fopen("recomp.conf", "rb");
    if (f) {
        parse(f);
        fclose(f);
        snprintf(g_path, sizeof g_path, "recomp.conf");
        g_have_path = 1;
    }
}

/* ---------------------------------------------------------------- lookup */

static const char *from_file(const char *key)
{
    int i;

    if (!key)
        return NULL;
    if (!g_loaded)
        load();
    for (i = 0; i < g_entry_count; i++)
        if (strcmp(g_entries[i].key, key) == 0)
            return g_entries[i].value[0] ? g_entries[i].value : NULL;
    return NULL;
}

const char *recomp_config_lookup(const char *env_name, const char *key)
{
    const char *v = env_name ? getenv(env_name) : NULL;

    if (v && *v)
        return v;
    return from_file(key);
}

const char *recomp_config_path(void)
{
    if (!g_loaded)
        load();
    return g_have_path ? g_path : NULL;
}

int recomp_config_int(const char *env_name, const char *key, int fallback)
{
    const char *v = recomp_config_lookup(env_name, key);
    char *end = NULL;
    long n;

    if (!v || !*v)
        return fallback;
    n = strtol(v, &end, 10);
    if (end == v || (end && *end))
        return fallback;
    return (int)n;
}

double recomp_config_float(const char *env_name, const char *key, double fallback)
{
    const char *v = recomp_config_lookup(env_name, key);
    char *end = NULL;
    double d;

    if (!v || !*v)
        return fallback;
    d = strtod(v, &end);
    if (end == v || (end && *end))
        return fallback;
    return d;
}

int recomp_config_bool(const char *env_name, const char *key, int fallback)
{
    const char *env = env_name ? getenv(env_name) : NULL;
    const char *v;

    /* An empty environment variable means on, as it does everywhere else
     * in the runtime: `set RECOMP_WIDESCREEN=` turns a switch on. */
    if (env)
        return !off_word(env);

    v = from_file(key);
    if (!v || !*v)
        return fallback;
    return !off_word(v);
}
