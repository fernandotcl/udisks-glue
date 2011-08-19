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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dbus_constants.h"
#include "filter.h"
#include "props.h"

typedef struct {
    enum {
        FILTER_OPTION_TYPE_BOOL,
        FILTER_OPTION_TYPE_STRING,
        FILTER_OPTION_TYPE_CUSTOM
    } type;
    union {
        const char *property_name;
        struct {
            custom_filter match_func;
            GDestroyNotify free_func;
            void *cookie;
        } custom;
    };
    const char *config_name;
    cfg_opt_t confuse_opt;
} filter_option;

static int custom_optical_disc_has_audio_tracks(DBusGProxy *proxy, property_cache *cache, void *cookie)
{
    int success;
    uint32_t num_audio_tracks = get_uint32_property_cached(cache, proxy, "OpticalDiscNumAudioTracks", DBUS_INTERFACE_UDISKS_DEVICE, &success);
    if (success)
        return num_audio_tracks > 0 ? BOOL_PROP_TRUE : BOOL_PROP_FALSE;
    else
        return BOOL_PROP_ERROR;
}

static int custom_optical_disc_has_audio_tracks_only(DBusGProxy *proxy, property_cache *cache, void *cookie)
{
    int success;
    uint32_t num_tracks = get_uint32_property_cached(cache, proxy, "OpticalDiscNumTracks", DBUS_INTERFACE_UDISKS_DEVICE, &success);
    if (!success)
        return BOOL_PROP_ERROR;
    uint32_t num_audio_tracks = get_uint32_property_cached(cache, proxy, "OpticalDiscNumAudioTracks", DBUS_INTERFACE_UDISKS_DEVICE, &success);
    if (!success)
        return BOOL_PROP_ERROR;
    return num_tracks > 0 && num_tracks == num_audio_tracks ? BOOL_PROP_TRUE : BOOL_PROP_FALSE;
}

#define FILTER_OPTION_BOOL(property, config) \
    { FILTER_OPTION_TYPE_BOOL, { property }, config, CFG_BOOL(config, cfg_false, CFGF_NODEFAULT) }

#define FILTER_OPTION_STRING(property, config) \
    { FILTER_OPTION_TYPE_STRING, { property }, config, CFG_STR(config, NULL, CFGF_NODEFAULT) }

#define FILTER_OPTION_CUSTOM(match_func, free_func, cookie, config) \
    { FILTER_OPTION_TYPE_CUSTOM, custom: { match_func, free_func, cookie }, config, CFG_BOOL(config, cfg_false, CFGF_NODEFAULT) }

#define NUM_FILTER_OPTIONS 12
static filter_option filter_options[NUM_FILTER_OPTIONS] = {
    FILTER_OPTION_BOOL("DeviceIsRemovable", "removable"),
    FILTER_OPTION_BOOL("DeviceIsReadOnly", "read_only"),
    FILTER_OPTION_BOOL("DeviceIsPartition", "partition"),
    FILTER_OPTION_BOOL("DeviceIsPartitionTable", "partition_table"),
    FILTER_OPTION_BOOL("DeviceIsOpticalDisc", "optical"),
    FILTER_OPTION_BOOL("OpticalDiscIsClosed", "optical_disc_closed"),
    FILTER_OPTION_STRING("IdUsage", "usage"),
    FILTER_OPTION_STRING("IdType", "type"),
    FILTER_OPTION_STRING("IdUuid", "uuid"),
    FILTER_OPTION_STRING("IdLabel", "label"),
    FILTER_OPTION_CUSTOM(&custom_optical_disc_has_audio_tracks, NULL, NULL, "optical_disc_has_audio_tracks"),
    FILTER_OPTION_CUSTOM(&custom_optical_disc_has_audio_tracks_only, NULL, NULL, "optical_disc_has_audio_tracks_only")
};

#undef FILTER_OPTION_BOOL
#undef FILTER_OPTION_STRING

static GHashTable *filters;

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
                case FILTER_OPTION_TYPE_CUSTOM: {
                    intptr_t value = cfg_getbool(sec, opt->config_name) == cfg_true ? 1 : 0;
                    filter_add_restriction_custom(f, opt->custom.match_func, opt->custom.free_func, opt->custom.cookie, value);
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
    filters = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, (GDestroyNotify)&filter_free);

    int index = cfg_size(cfg, "filter");
    while (index--) {
        cfg_t *sec = cfg_getnsec(cfg, "filter", index);
        filter *f = filter_create();
        add_filter_restrictions(f, sec);
        g_hash_table_insert(filters, g_strdup(cfg_title(sec)), f);
    }
    return 1;
}

void filters_free()
{
    if (filters)
        g_hash_table_destroy(filters);
}

filter *filters_find_filter_by_name(const char *name)
{
    return g_hash_table_lookup(filters, name);
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
