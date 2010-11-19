/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef FILTERS_H
#define FILTERS_H

#include <dbus/dbus-glib.h>
#include <confuse.h>

typedef enum {
    FILTER_COMMAND_POST_INSERTION = 0,
    FILTER_COMMAND_POST_MOUNT,
    FILTER_COMMAND_POST_UNMOUNT,
    FILTER_COMMAND_POST_REMOVAL,
    FILTER_COMMAND_LAST
} filter_command;

typedef struct filter_cache_ filter_cache;

cfg_opt_t *filters_get_cfg_opts();

int filters_init(cfg_t *cfg);
void filters_free();

filter_cache *filter_cache_create();
void filter_cache_free(filter_cache *cache);
const char *filters_get_command(DBusGProxy *proxy, filter_command command, filter_cache *cache);

#endif
