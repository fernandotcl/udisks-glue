/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef MATCH_H
#define MATCH_H

#include <dbus/dbus-glib.h>
#include <confuse.h>

#include "filter.h"
#include "property_cache.h"

typedef struct match_ match;

match *match_create(cfg_t *sec, filter *f);
void match_free(match *m);

cfg_opt_t *match_get_cfg_opts();
void match_free_cfg_opts(cfg_opt_t *opts);

int match_matches(match *m, DBusGProxy *proxy, property_cache *cache);

const char *match_get_post_insertion_command(match *m);
const char *match_get_post_mount_command(match *m);
const char *match_get_post_unmount_command(match *m);
const char *match_get_post_removal_command(match *m);

#endif
