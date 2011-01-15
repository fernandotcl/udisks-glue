/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <assert.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "dbus_constants.h"
#include "filter.h"
#include "property_cache.h"
#include "props.h"

typedef struct {
    enum {
        RESTRICTION_TYPE_BOOL,
        RESTRICTION_TYPE_STRING
    } type;
    union {
        int bool_value;
        gchar *string_value;
    } values;
    char *property;
} restriction;

struct filter_ {
    gchar *match_name;
    GSList *restrictions;
};

static restriction *restriction_create_bool(const char *property, int value)
{
    restriction *r = g_malloc(sizeof(restriction));
    r->type = RESTRICTION_TYPE_BOOL;
    r->values.bool_value = value ? BOOL_PROP_TRUE : BOOL_PROP_FALSE;
    r->property = strdup(property);
    return r;
}

static restriction *restriction_create_string(const char *property, const char *value)
{
    restriction *r = g_malloc(sizeof(restriction));
    r->type = RESTRICTION_TYPE_STRING;
    r->values.string_value = g_strdup(value);
    r->property = strdup(property);
    return r;
}

static void restriction_free(restriction *r)
{
    switch (r->type) {
        case RESTRICTION_TYPE_BOOL: break;
        case RESTRICTION_TYPE_STRING: g_free(r->values.string_value); break;
        default: assert(0); break;
    }
    free(r->property);
    g_free(r);
}

static int restriction_matches(restriction *r, DBusGProxy *proxy, property_cache *cache)
{
    switch (r->type) {
        case RESTRICTION_TYPE_BOOL: {
            int value = get_bool_property_cached(cache, proxy, r->property, DBUS_INTERFACE_UDISKS_DEVICE);
            return value == r->values.bool_value;
        }
        case RESTRICTION_TYPE_STRING: {
            gchar *value = get_string_property_cached(cache, proxy, r->property, DBUS_INTERFACE_UDISKS_DEVICE);
            return value ? !strcmp(value, r->values.string_value) : 0;
        }
        default:
            assert(0);
            return 0;
    }
}

filter *filter_create(const char *match_name)
{
    filter *f = g_malloc0(sizeof(filter));
    f->match_name = g_strdup(match_name);
    return f;
}

void filter_free(filter *f)
{
    g_slist_foreach(f->restrictions, (GFunc)restriction_free, NULL);
    g_free(f->match_name);
    g_free(f);
}

void filter_add_restriction_bool(filter *f, const char *property, int value)
{
    f->restrictions = g_slist_prepend(f->restrictions, restriction_create_bool(property, value));
}

void filter_add_restriction_string(filter *f, const char *property, const char *value)
{
    f->restrictions = g_slist_prepend(f->restrictions, restriction_create_string(property, value));
}

const char *filter_matches(filter *f, DBusGProxy *proxy, property_cache *cache)
{
    for (GSList *entry = f->restrictions; entry; entry = g_slist_next(entry)) {
        restriction *r = (restriction *)entry->data;
        if (!restriction_matches(r, proxy, cache))
            return f->match_name;
    }
    return NULL;
}
