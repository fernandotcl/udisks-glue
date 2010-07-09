/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <confuse.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>

#include "dbus_constants.h"
#include "filters.h"
#include "props.h"

#define DEVICE_PROPERTY(type, name) \
    get_##type##_property(info->proxy, name, DBUS_INTERFACE_UDISKS_DEVICE)

#define DEFINE_BOOL_PROPERTY(name, prop) \
    static int filter_##name(filter_info *info, void *value) {\
        int wanted_value = (cfg_bool_t)value ? BOOL_PROP_TRUE : BOOL_PROP_FALSE; \
        return DEVICE_PROPERTY(bool, prop) == wanted_value; \
    }

#define BOOL_OPTION(name) \
    CFG_BOOL(#name, 0, CFGF_NODEFAULT)

#define FILTER_LINK(cfg_name, func_name, type) \
    do { \
        if (cfg_size(sec, #cfg_name)) { \
            filter_parameters *fp = g_malloc(sizeof(filter_parameters)); \
            fp->func = &filter_##func_name; \
            fp->arg = (void *)cfg_get##type(sec, #cfg_name); \
            f->parameters = g_slist_prepend(f->parameters, fp); \
        } \
    } while (0)

typedef int (*filter_function)(filter_info *, void *value);

typedef struct {
    filter_function func;
    void *arg;
} filter_parameters;

typedef struct {
    const char *name;
    GSList *parameters;
} filter;

typedef struct {
    filter *f;
    const char *commands[FILTER_COMMAND_LAST];
} candidate;

static candidate *default_candidate = NULL;
static GSList *candidates = NULL;
static GSList *filters = NULL;

DEFINE_BOOL_PROPERTY(device_is_removable, "DeviceIsRemovable")
DEFINE_BOOL_PROPERTY(device_is_read_only, "DeviceIsReadOnly")
DEFINE_BOOL_PROPERTY(device_is_optical_disc, "DeviceIsOpticalDisc")
DEFINE_BOOL_PROPERTY(device_optical_disc_is_closed, "OpticalDiscIsClosed")

static filter *filter_create()
{
    return g_malloc0(sizeof(filter));
}

static void filter_free(filter *f)
{
    g_slist_foreach(f->parameters, (GFunc)g_free, NULL);
    g_slist_free(f->parameters);
    g_free(f);
}

static candidate *candidate_create()
{
    return g_malloc0(sizeof(candidate));
}

static void candidate_read_funcs(candidate *c, cfg_t *sec)
{
    if (cfg_size(sec, "post_insertion_command"))
        c->commands[FILTER_COMMAND_POST_INSERTION] = cfg_getstr(sec, "post_insertion_command");
    if (cfg_size(sec, "post_mount_command"))
        c->commands[FILTER_COMMAND_POST_MOUNT] = cfg_getstr(sec, "post_mount_command");
    if (cfg_size(sec, "post_removal_command"))
        c->commands[FILTER_COMMAND_POST_REMOVAL] = cfg_getstr(sec, "post_removal_command");
}

static void candidate_free(candidate *c)
{
    g_free(c);
}

cfg_opt_t *filters_get_cfg_opts()
{
    static cfg_opt_t filter_opts[] = {
        BOOL_OPTION(removable),
        BOOL_OPTION(read_only),
        BOOL_OPTION(optical),
        BOOL_OPTION(disc_closed),
        CFG_END()
    };
    return filter_opts;
}

int filters_init(cfg_t *cfg)
{
    int index = cfg_size(cfg, "filter");
    while (index--) {
        cfg_t *sec = cfg_getnsec(cfg, "filter", index);

        filter *f = filter_create();
        filters = g_slist_prepend(filters, f);

        f->name = cfg_title(sec);
        FILTER_LINK(removable, device_is_removable, bool);
        FILTER_LINK(read_only, device_is_read_only, bool);
        FILTER_LINK(optical, device_is_optical_disc, bool);
        FILTER_LINK(disc_closed, device_optical_disc_is_closed, bool);
    }

    index = cfg_size(cfg, "match");
    while (index--) {
        cfg_t *sec = cfg_getnsec(cfg, "match", index);
        const char *title = cfg_title(sec);

        filter *f = NULL;
        for (GSList *entry = filters; entry; entry = g_slist_next(entry)) {
            filter *f2 = (filter *)entry->data;
            if (!strcmp(f2->name, title)) {
                f = f2;
                break;
            }
        }
        if (!f) {
            g_printerr("Unknown filter %s\n", title);
            return 0;
        }

        candidate *c = candidate_create();
        candidates = g_slist_prepend(candidates, c);

        c->f = f;
        candidate_read_funcs(c, sec);
    }

    if (cfg_size(cfg, "default")) {
        default_candidate = candidate_create();
        candidate_read_funcs(default_candidate, cfg_getsec(cfg, "default"));
    }

    return 1;
}

void filters_free()
{
    g_slist_foreach(candidates, (GFunc)candidate_free, NULL);
    g_slist_free(candidates);

    if (default_candidate)
        candidate_free(default_candidate);

    g_slist_foreach(filters, (GFunc)filter_free, NULL);
    g_slist_free(filters);
}

static const candidate *filters_get_candidate(filter_info *info)
{
    // Look at the configured candidates for an entry that matches the filters
    // and sets the requested command
    for (GSList *entry = candidates; entry; entry = g_slist_next(entry)) {
        candidate *c = (candidate *)entry->data;
        int matched = 1;
        for (GSList *entry2 = c->f->parameters; entry2; entry2 = g_slist_next(entry2)) {
            filter_parameters *fp = (filter_parameters *)entry2->data;
            if (!fp->func(info, fp->arg)) {
                matched = 0;
                break;
            }
        }
        if (matched && c->commands[info->requested_command]) return c;
    }

    // Try the default entry
    return default_candidate;
}

const char *filters_get_command(filter_info *info)
{
    const candidate *c = filters_get_candidate(info);
    return c ? c->commands[info->requested_command] : NULL;
}
