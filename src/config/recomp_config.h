/*
 * recomp_config.h -- the settings a player chooses, and where they live.
 *
 * Everything the runtime can be told is an environment variable, which is
 * right for a developer and no use to someone who just wants the game in
 * widescreen. This is the other half: a small file the launcher writes and
 * the runtime reads, so a setting survives being chosen once.
 *
 * The environment still wins. A variable set on the command line, in a
 * .bat, or by CI overrides the file, so every existing way of driving the
 * runtime keeps working and the file is what happens when nothing said
 * otherwise. Callers say both names at the point of use:
 *
 *     v = recomp_config_lookup("RECOMP_RES_SCALE", "resolution_scale");
 *
 * which keeps the precedence visible where the setting is read rather
 * than buried here.
 *
 * The file is per title, keyed by the title id out of the XBE
 * certificate: a resolution scale is a judgement about one game on one
 * machine, and the Hor+ register is meaningless to any other game. Input
 * bindings are deliberately not here -- those belong to the person, not
 * the title, and they already have a file of their own.
 *
 * It is an INI rather than JSON because the settings are flat and because
 * a generated INI can explain itself. The file a player opens says what
 * each line does; JSON has nowhere to put that.
 */
#ifndef XBOXRECOMP_RECOMP_CONFIG_H
#define XBOXRECOMP_RECOMP_CONFIG_H

#include <stdint.h>

/* The running title, from its XBE certificate, which decides the file's
 * name. Called during start-up before anything reads a setting; without
 * it the lookups below still work and simply find no file. */
void recomp_config_set_title(uint32_t title_id);

/* The value for a setting: the environment variable if it is set and not
 * empty, then the config file, then NULL. Either name may be NULL to skip
 * that source. The returned string is owned here and valid for the life
 * of the process. */
const char *recomp_config_lookup(const char *env_name, const char *key);

/* The same, parsed, with a fallback for absent or unreadable values.
 * Booleans accept 1/0, on/off, yes/no, true/false, and an empty
 * environment variable counts as on -- the rule xbox_EnvSwitch already
 * uses, so a variable behaves the same whether or not this file exists. */
int    recomp_config_int(const char *env_name, const char *key, int fallback);
int    recomp_config_bool(const char *env_name, const char *key, int fallback);
double recomp_config_float(const char *env_name, const char *key, double fallback);

/* The file the lookups read, for a log line or a launcher; NULL when no
 * file was found. */
const char *recomp_config_path(void);

#endif /* XBOXRECOMP_RECOMP_CONFIG_H */
