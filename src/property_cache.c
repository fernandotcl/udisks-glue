/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <assert.h>
#include <glib.h>

#include "property_cache.h"
#include "props.h"

typedef struct {
    enum {
        CACHE_VALUE_TYPE_BOOL,
        CACHE_VALUE_TYPE_STRING,
        CACHE_VALUE_TYPE_STRINGV
    } type;
    union {
        int bool_value;
        gchar *string_value;
        gchar **stringv_value;
    } values;
} cache_value;

struct property_cache_ {
    GHashTable *entries;
};

static cache_value *cache_value_create_bool(int val)
{
    cache_value *value = g_malloc0(sizeof(cache_value));
    value->type = CACHE_VALUE_TYPE_BOOL;
    value->values.bool_value = val;
    return value;
}

static cache_value *cache_value_create_string(gchar *val)
{
    cache_value *value = g_malloc0(sizeof(cache_value));
    value->type = CACHE_VALUE_TYPE_STRING;
    value->values.string_value = val;
    return value;
}

static cache_value *cache_value_create_stringv(gchar **val)
{
    cache_value *value = g_malloc0(sizeof(cache_value));
    value->type = CACHE_VALUE_TYPE_STRING;
    value->values.stringv_value = val;
    return value;
}

static void cache_value_free(cache_value *value)
{
    switch (value->type) {
        case CACHE_VALUE_TYPE_BOOL: break;
        case CACHE_VALUE_TYPE_STRING: g_free(value->values.string_value); break;
        case CACHE_VALUE_TYPE_STRINGV: g_strfreev(value->values.stringv_value); break;
        default: assert(0); break;
    }
    g_free(value);
}

property_cache *property_cache_create()
{
    property_cache *cache = g_malloc(sizeof(property_cache));
    cache->entries = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, (GDestroyNotify)&cache_value_free);
    return cache;
}

void property_cache_free(property_cache *cache)
{
    g_hash_table_destroy(cache->entries);
    g_free(cache);
}

int get_bool_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface)
{
    cache_value *value = g_hash_table_lookup(cache->entries, name);
    if (value) return value->values.bool_value;

    int res = get_bool_property(proxy, name, interface);
    if (res != BOOL_PROP_ERROR)
        g_hash_table_insert(cache->entries, g_strdup(name), cache_value_create_bool(res));

    return res;
}

gchar *get_string_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface)
{
    cache_value *value = g_hash_table_lookup(cache->entries, name);
    if (value) return value->values.string_value;

    gchar *res = get_string_property(proxy, name, interface);
    if (res)
        g_hash_table_insert(cache->entries, g_strdup(name), cache_value_create_string(res));

    return res;
}

gchar **get_stringv_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface)
{
    cache_value *value = g_hash_table_lookup(cache->entries, name);
    if (value) return value->values.stringv_value;

    gchar **res = get_stringv_property(proxy, name, interface);
    if (res)
        g_hash_table_insert(cache->entries, g_strdup(name), cache_value_create_stringv(res));

    return res;
}
