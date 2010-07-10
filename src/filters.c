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
    get_##type##_property(proxy, name, DBUS_INTERFACE_UDISKS_DEVICE)

#define DECLARE_BOOLEAN_FILTER_CACHE(identifier) \
    int identifier; \
    int identifier##_is_hot;

#define DECLARE_BOOLEAN_FILTER_OPTION(config_key) \
    CFG_BOOL(#config_key, 0, CFGF_NODEFAULT)

#define IMPLEMENT_BOOLEAN_FILTER(identifier, property) \
    static int filter_function_##identifier(DBusGProxy *proxy, void *value, filter_cache *cache) { \
        if (!cache->boolean_values.identifier##_is_hot) { \
            cache->boolean_values.identifier = DEVICE_PROPERTY(bool, property); \
            cache->boolean_values.identifier##_is_hot = 1; \
        } \
        int wanted_value = (cfg_bool_t)value ? BOOL_PROP_TRUE : BOOL_PROP_FALSE; \
        return cache->boolean_values.identifier == wanted_value; \
    }

#define LINK_BOOLEAN_FILTER(identifier, config_key) \
    if (cfg_size(sec, config_key)) { \
        filter_parameters *fp = g_malloc(sizeof(filter_parameters)); \
        fp->func = &filter_function_##identifier; \
        fp->arg = (void *)cfg_getbool(sec, config_key); \
        f->parameters = g_slist_prepend(f->parameters, fp); \
    }

typedef int (*filter_function)(DBusGProxy *, void *, filter_cache *);

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

struct filter_cache_ {
    struct {
        DECLARE_BOOLEAN_FILTER_CACHE(removable)
        DECLARE_BOOLEAN_FILTER_CACHE(read_only)
        DECLARE_BOOLEAN_FILTER_CACHE(optical)
        DECLARE_BOOLEAN_FILTER_CACHE(optical_disc_closed)
    } boolean_values;
};

static cfg_opt_t filter_opts[] = {
    DECLARE_BOOLEAN_FILTER_OPTION(removable),
    DECLARE_BOOLEAN_FILTER_OPTION(read_only),
    DECLARE_BOOLEAN_FILTER_OPTION(optical),
    DECLARE_BOOLEAN_FILTER_OPTION(disc_closed),
    CFG_END()
};

static candidate *default_candidate = NULL;
static GSList *candidates = NULL;
static GSList *filters = NULL;

IMPLEMENT_BOOLEAN_FILTER(removable,           "DeviceIsRemovable")
IMPLEMENT_BOOLEAN_FILTER(read_only,           "DeviceIsReadOnly")
IMPLEMENT_BOOLEAN_FILTER(optical,             "DeviceIsOpticalDisc")
IMPLEMENT_BOOLEAN_FILTER(optical_disc_closed, "OpticalDiscIsClosed")

static void add_filter_parameters(filter *f, cfg_t *sec)
{
    LINK_BOOLEAN_FILTER(removable,           "removable")
    LINK_BOOLEAN_FILTER(read_only,           "read_only")
    LINK_BOOLEAN_FILTER(optical,             "optical")
    LINK_BOOLEAN_FILTER(optical_disc_closed, "disc_closed")
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

filter_cache *filter_cache_create()
{
    return g_malloc0(sizeof(filter_cache));
}

void filter_cache_free(filter_cache *cache)
{
    g_free(cache);
}

static const candidate *filters_get_candidate(DBusGProxy *proxy, filter_command command, filter_cache *cache)
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

const char *filters_get_command(DBusGProxy *proxy, filter_command command, filter_cache *cache)
{
    const candidate *c = filters_get_candidate(proxy, command, cache);
    return c ? c->commands[command] : NULL;
}
