/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <confuse.h>
#include <glib.h>

#include "match.h"

static GHashTable *matches;
static match *default_match = NULL;

int matches_init(cfg_t *cfg)
{
    matches = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, (GDestroyNotify)&match_free);

    int index = cfg_size(cfg, "match");
    while (index--) {
        cfg_t *sec = cfg_getnsec(cfg, "match", index);
        g_hash_table_insert(matches, g_strdup(cfg_title(sec)), match_create(sec));
    }

    if (cfg_size(cfg, "default"))
        default_match = match_create(cfg_getsec(cfg, "default"));

    return 1;
}

void matches_free()
{
    g_hash_table_destroy(matches);
    if (default_match)
        match_free(default_match);
}

match *matches_find_match(const char *name)
{
    match *m = g_hash_table_lookup(matches, name);
    return m ? m : default_match;
}
