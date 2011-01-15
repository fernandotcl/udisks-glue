/*
 * This file is part of udisks-glue.
 *
 * © 2010-2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef FILTERS_H
#define FILTERS_H

#include <dbus/dbus-glib.h>
#include <confuse.h>

#include "property_cache.h"

int filters_init(cfg_t *cfg);
void filters_free();

cfg_opt_t *filters_get_cfg_opts();
void filters_free_cfg_opts(cfg_opt_t *opts);

const char *filters_find_match_name(DBusGProxy *proxy, property_cache *cache);

#endif
