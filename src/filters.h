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

cfg_opt_t *filters_get_cfg_opts();

int filters_init(cfg_t *cfg);
void filters_free();

const char *filters_get_command(filter_info *info);
