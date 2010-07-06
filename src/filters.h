/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <confuse.h>

typedef struct {
    DBusGProxy *proxy;
    int device_is_optical;
    enum {
        FILTER_COMMAND_POST_INSERTION = 0,
        FILTER_COMMAND_POST_MOUNT,
        FILTER_COMMAND_POST_REMOVAL,
        FILTER_COMMAND_LAST
    } requested_command;
} filter_info;

typedef struct candidate_ candidate;

cfg_opt_t *filters_get_cfg_opts();

int filters_init(cfg_t *cfg);
void filters_free();

const candidate *filters_get_candidate(filter_info *info);
const char *filters_get_command(filter_info *info);
const char *filters_get_command_in_candidate(filter_info *info, const candidate *c);
