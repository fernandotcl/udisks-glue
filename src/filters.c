/*
 * This file is part of udisks-glue.
 *
 * © 2010-2011 Fernando Tarlá Cardoso Lemos
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
#include "property_cache.h"
#include "props.h"

#define DEVICE_PROPERTY(type, name) \
    get_##type##_property(proxy, name, DBUS_INTERFACE_UDISKS_DEVICE)

#define DECLARE_BOOLEAN_FILTER_OPTION(config_key) \
    CFG_BOOL(#config_key, 0, CFGF_NODEFAULT)

#define DECLARE_STRING_FILTER_OPTION(config_key) \
    CFG_STR(#config_key, NULL, CFGF_NODEFAULT)

#define IMPLEMENT_BOOLEAN_FILTER(identifier, property) \
    static int filter_function_##identifier(DBusGProxy *proxy, void *value, property_cache *cache) { \
        int wanted_value = (cfg_bool_t)value ? BOOL_PROP_TRUE : BOOL_PROP_FALSE; \
        return get_bool_property_cached(cache, proxy, property, DBUS_INTERFACE_UDISKS_DEVICE) == wanted_value; \
    }

#define IMPLEMENT_STRING_FILTER(identifier, property) \
    static int filter_function_##identifier(DBusGProxy *proxy, void *value, property_cache *cache) { \
        gchar *prop = get_string_property_cached(cache, proxy, property, DBUS_INTERFACE_UDISKS_DEVICE); \
        return value ? !strcmp((const char *)prop, (const char *)value) : 0; \
    }

#define LINK_BOOLEAN_FILTER(identifier, config_key) \
    if (cfg_size(sec, config_key)) { \
        filter_parameters *fp = g_malloc(sizeof(filter_parameters)); \
        fp->func = &filter_function_##identifier; \
        fp->arg = (void *)cfg_getbool(sec, config_key); \
        f->parameters = g_slist_prepend(f->parameters, fp); \
    }

#define LINK_STRING_FILTER(identifier, config_key) \
    if (cfg_size(sec, config_key)) { \
        filter_parameters *fp = g_malloc(sizeof(filter_parameters)); \
        fp->func = &filter_function_##identifier; \
        fp->arg = (void *)cfg_getstr(sec, config_key); \
        f->parameters = g_slist_prepend(f->parameters, fp); \
    }

typedef int (*filter_function)(DBusGProxy *, void *, property_cache *);

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

static cfg_opt_t filter_opts[] = {
    DECLARE_BOOLEAN_FILTER_OPTION(removable),
    DECLARE_BOOLEAN_FILTER_OPTION(read_only),
    DECLARE_BOOLEAN_FILTER_OPTION(optical),
    DECLARE_BOOLEAN_FILTER_OPTION(disc_closed),

    DECLARE_STRING_FILTER_OPTION(usage),
    DECLARE_STRING_FILTER_OPTION(type),
    DECLARE_STRING_FILTER_OPTION(uuid),
    DECLARE_STRING_FILTER_OPTION(label),

    CFG_END()
};

static candidate *default_candidate = NULL;
static GSList *candidates = NULL;
static GSList *filters = NULL;

IMPLEMENT_BOOLEAN_FILTER(removable,           "DeviceIsRemovable")
IMPLEMENT_BOOLEAN_FILTER(read_only,           "DeviceIsReadOnly")
IMPLEMENT_BOOLEAN_FILTER(optical,             "DeviceIsOpticalDisc")
IMPLEMENT_BOOLEAN_FILTER(optical_disc_closed, "OpticalDiscIsClosed")

IMPLEMENT_STRING_FILTER(usage, "IdUsage")
IMPLEMENT_STRING_FILTER(type,  "IdType")
IMPLEMENT_STRING_FILTER(uuid,  "IdUuid")
IMPLEMENT_STRING_FILTER(label, "IdLabel")

static void add_filter_parameters(filter *f, cfg_t *sec)
{
    LINK_BOOLEAN_FILTER(removable,           "removable")
    LINK_BOOLEAN_FILTER(read_only,           "read_only")
    LINK_BOOLEAN_FILTER(optical,             "optical")
    LINK_BOOLEAN_FILTER(optical_disc_closed, "disc_closed")

    LINK_STRING_FILTER(usage, "usage")
    LINK_STRING_FILTER(type,  "type")
    LINK_STRING_FILTER(uuid,  "uuid")
    LINK_STRING_FILTER(label, "label")
}

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

static void candidate_free(candidate *c)
{
    g_free(c);
}

static void candidate_read_funcs(candidate *c, cfg_t *sec)
{
    if (cfg_size(sec, "post_insertion_command"))
        c->commands[FILTER_COMMAND_POST_INSERTION] = cfg_getstr(sec, "post_insertion_command");
    if (cfg_size(sec, "post_mount_command"))
        c->commands[FILTER_COMMAND_POST_MOUNT] = cfg_getstr(sec, "post_mount_command");
    if (cfg_size(sec, "post_unmount_command"))
        c->commands[FILTER_COMMAND_POST_UNMOUNT] = cfg_getstr(sec, "post_unmount_command");
    if (cfg_size(sec, "post_removal_command"))
        c->commands[FILTER_COMMAND_POST_REMOVAL] = cfg_getstr(sec, "post_removal_command");
}

cfg_opt_t *filters_get_cfg_opts()
{
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
        add_filter_parameters(f, sec);
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

static const candidate *filters_get_candidate(DBusGProxy *proxy, property_cache *cache, filter_command command)
{
    for (GSList *entry = candidates; entry; entry = g_slist_next(entry)) {
        candidate *c = (candidate *)entry->data;
        int matched = 1;
        for (GSList *entry2 = c->f->parameters; entry2; entry2 = g_slist_next(entry2)) {
            filter_parameters *fp = (filter_parameters *)entry2->data;
            if (!fp->func(proxy, fp->arg, cache)) {
                matched = 0;
                break;
            }
        }
        if (matched && c->commands[command]) return c;
    }

    return default_candidate;
}

const char *filters_get_command(DBusGProxy *proxy, filter_command command, property_cache *cache)
{
    const candidate *c = filters_get_candidate(proxy, cache, command);
    return c ? c->commands[command] : NULL;
}
