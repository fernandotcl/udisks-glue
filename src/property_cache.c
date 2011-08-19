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
        CACHE_VALUE_TYPE_INT16,
        CACHE_VALUE_TYPE_INT32,
        CACHE_VALUE_TYPE_INT64,
        CACHE_VALUE_TYPE_UINT16,
        CACHE_VALUE_TYPE_UINT32,
        CACHE_VALUE_TYPE_UINT64,
        CACHE_VALUE_TYPE_BOOL,
        CACHE_VALUE_TYPE_STRING,
        CACHE_VALUE_TYPE_STRINGV
    } type;
    union {
        int16_t int16_value;
        int32_t int32_value;
        int64_t int64_value;
        uint16_t uint16_value;
        uint32_t uint32_value;
        uint64_t uint64_value;
        int bool_value;
        gchar *string_value;
        gchar **stringv_value;
    } values;
} cache_value;

struct property_cache_ {
    GHashTable *entries;
};

#define IMPLEMENT_CACHE_VALUE_COPY(c_type, e_type, name) \
    static cache_value *cache_value_create_##name(c_type val) \
    { \
        cache_value *value = g_malloc0(sizeof(cache_value)); \
        value->type = CACHE_VALUE_TYPE_##e_type; \
        value->values.name##_value = val; \
        return value; \
    }

IMPLEMENT_CACHE_VALUE_COPY(int16_t, INT16, int16)
IMPLEMENT_CACHE_VALUE_COPY(int32_t, INT32, int32)
IMPLEMENT_CACHE_VALUE_COPY(int64_t, INT64, int64)
IMPLEMENT_CACHE_VALUE_COPY(uint16_t, UINT16, uint16)
IMPLEMENT_CACHE_VALUE_COPY(uint32_t, UINT32, uint32)
IMPLEMENT_CACHE_VALUE_COPY(uint64_t, UINT64, uint64)
IMPLEMENT_CACHE_VALUE_COPY(int, BOOL, bool)
IMPLEMENT_CACHE_VALUE_COPY(gchar *, STRING, string)
IMPLEMENT_CACHE_VALUE_COPY(gchar **, STRINGV, stringv)

static void cache_value_free(cache_value *value)
{
    switch (value->type) {
        case CACHE_VALUE_TYPE_INT16: break;
        case CACHE_VALUE_TYPE_INT32: break;
        case CACHE_VALUE_TYPE_INT64: break;
        case CACHE_VALUE_TYPE_UINT16: break;
        case CACHE_VALUE_TYPE_UINT32: break;
        case CACHE_VALUE_TYPE_UINT64: break;
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

void property_cache_purge(property_cache *cache)
{
    g_hash_table_remove_all(cache->entries);
}

#define IMPLEMENT_GET_NUMBER_PROPERTY_CACHED(c_type, name) \
    c_type get_##name##_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface, int *success) \
    { \
        cache_value *value = g_hash_table_lookup(cache->entries, name); \
        if (value) { \
            if (success) \
                *success = 1; \
            return value->values.name##_value; \
        } \
        int my_success; \
        c_type res = get_##name##_property(proxy, name, interface, &my_success); \
        if (my_success) \
            g_hash_table_insert(cache->entries, g_strdup(name), cache_value_create_##name(res)); \
        if (success) \
            *success = my_success; \
        return res; \
    }

IMPLEMENT_GET_NUMBER_PROPERTY_CACHED(int16_t, int16)
IMPLEMENT_GET_NUMBER_PROPERTY_CACHED(int32_t, int32)
IMPLEMENT_GET_NUMBER_PROPERTY_CACHED(int64_t, int64)
IMPLEMENT_GET_NUMBER_PROPERTY_CACHED(uint16_t, uint16)
IMPLEMENT_GET_NUMBER_PROPERTY_CACHED(uint32_t, uint32)
IMPLEMENT_GET_NUMBER_PROPERTY_CACHED(uint64_t, uint64)

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
