/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <confuse.h>
#include <glib.h>

#include "filter.h"
#include "filters.h"
#include "match.h"
#include "property_cache.h"

static GSList *matches = NULL;
static match *default_match = NULL;

int matches_init(cfg_t *cfg)
{
    int index = cfg_size(cfg, "match");
    while (index--) {
        cfg_t *sec = cfg_getnsec(cfg, "match", index);

        filter *f = filters_find_filter_by_name(cfg_title(sec));
        if (!f) {
            g_printerr("Unknown filter: %s", cfg_title(sec));
            return 0;
        }

        match *m = match_create(sec, f);
        matches = g_slist_prepend(matches, m);
    }

    if (cfg_size(cfg, "default"))
        default_match = match_create(cfg_getsec(cfg, "default"), NULL);

    return 1;
}

void matches_free()
{
    g_slist_foreach(matches, (GFunc)match_free, NULL);
    if (default_match)
        match_free(default_match);
}

match *matches_find_match(DBusGProxy *proxy, property_cache *cache)
{
    for (GSList *entry = matches; entry; entry = g_slist_next(entry)) {
        match *m = (match *)entry->data;
        if (match_matches(m, proxy, cache))
            return m;
    }
    return default_match;
}
