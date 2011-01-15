/*
 * This file is part of udisks-glue.
 *
 * © 2010-2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <assert.h>
#include <confuse.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>

#include "filter.h"

typedef struct {
    enum {
        FILTER_OPTION_TYPE_BOOL,
        FILTER_OPTION_TYPE_STRING
    } type;
    const char *property_name;
    const char *config_name;
    cfg_opt_t confuse_opt;
} filter_option;

#define FILTER_OPTION_BOOL(property, config) \
    { FILTER_OPTION_TYPE_BOOL, property, config, CFG_BOOL(config, cfg_false, CFGF_NODEFAULT) }

#define FILTER_OPTION_STRING(property, config) \
    { FILTER_OPTION_TYPE_STRING, property, config, CFG_STR(config, NULL, CFGF_NODEFAULT) }

#define NUM_FILTER_OPTIONS 8
static filter_option filter_options[NUM_FILTER_OPTIONS] = {
    FILTER_OPTION_BOOL("DeviceIsRemovable", "removable"),
    FILTER_OPTION_BOOL("DeviceIsReadOnly", "read_only"),
    FILTER_OPTION_BOOL("DeviceIsOpticalDisc", "optical"),
    FILTER_OPTION_BOOL("OpticalDiscIsClosed", "disc_closed"),
    FILTER_OPTION_STRING("IdUsage", "usage"),
    FILTER_OPTION_STRING("IdType", "type"),
    FILTER_OPTION_STRING("IdUuid", "uuid"),
    FILTER_OPTION_STRING("IdLabel", "label")
};

#undef FILTER_OPTION_BOOL
#undef FILTER_OPTION_STRING

static GSList *filters = NULL;

static void add_filter_restrictions(filter *f, cfg_t *sec)
{
    for (int i = 0; i < NUM_FILTER_OPTIONS; ++i) {
        filter_option *opt = &filter_options[i];
        if (cfg_size(sec, opt->config_name)) {
            switch (opt->type) {
                case FILTER_OPTION_TYPE_BOOL: {
                    int value = cfg_getbool(sec, opt->config_name) == cfg_true ? 1 : 0;
                    filter_add_restriction_bool(f, opt->property_name, value);
                    break;
                }
                case FILTER_OPTION_TYPE_STRING: {
                    const char *value = cfg_getstr(sec, opt->config_name);
                    filter_add_restriction_string(f, opt->property_name, value);
                    break;
                }
                default:
                    assert(0);
                    break;
            }
        }
    }
}

int filters_init(cfg_t *cfg)
{
    int index = cfg_size(cfg, "filter");
    while (index--) {
        cfg_t *sec = cfg_getnsec(cfg, "filter", index);
        filter *f = filter_create(cfg_title(sec));
        filters = g_slist_prepend(filters, f);
        add_filter_restrictions(f, sec);
    }
    return 1;
}

void filters_free()
{
    g_slist_foreach(filters, (GFunc)filter_free, NULL);
    g_slist_free(filters);
}

const char *filters_find_match_name(DBusGProxy *proxy, property_cache *cache)
{
    for (GSList *entry = filters; entry; entry = g_slist_next(entry)) {
        filter *f = (filter *)entry->data;
        const char *name = filter_matches(f, proxy, cache);
        if (name) return name;
    }
    return NULL;
}

cfg_opt_t *filters_get_cfg_opts()
{
    cfg_opt_t *opts = malloc(sizeof(cfg_opt_t) * (NUM_FILTER_OPTIONS + 1));
    for (int i = 0; i < NUM_FILTER_OPTIONS; ++i)
        memcpy(&opts[i], &filter_options[i].confuse_opt, sizeof(cfg_opt_t));

    cfg_opt_t end = CFG_END();
    memcpy(&opts[NUM_FILTER_OPTIONS], &end, sizeof(cfg_opt_t));

    return opts;
}

void filters_free_cfg_opts(cfg_opt_t *opts)
{
    free(opts);
}
