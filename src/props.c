/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <glib.h>

#include "props.h"

#define GET_PROPERTY_PREAMBLE(error_val, interface) \
    GValue value = {0, }; \
    do { \
        GError *error = NULL; \
        if (!dbus_g_proxy_call(proxy, "Get", &error, \
                    G_TYPE_STRING, interface, \
                    G_TYPE_STRING, name, \
                    G_TYPE_INVALID, \
                    G_TYPE_VALUE, &value, \
                    G_TYPE_INVALID)) { \
            g_printerr("Unable to get property \"%s\": %s\n", name, error->message); \
            g_error_free(error); \
            return error_val; \
        } \
    } while (0)

int get_bool_property(DBusGProxy *proxy, const char *name, const char *interface)
{
    GET_PROPERTY_PREAMBLE(BOOL_PROP_ERROR, interface);
    int res = g_value_get_boolean(&value) ? BOOL_PROP_TRUE : BOOL_PROP_FALSE;
    g_value_unset(&value);
    return res;
}

gchar *get_string_property(DBusGProxy *proxy, const char *name, const char *interface)
{
    GET_PROPERTY_PREAMBLE(NULL, interface);
    gchar *res = g_strdup(g_value_get_string(&value));
    g_value_unset(&value);
    return res;
}

gchar **get_stringv_property(DBusGProxy *proxy, const char *name, const char *interface)
{
    GET_PROPERTY_PREAMBLE(NULL, interface);
    gchar **res = g_strdupv(g_value_get_boxed(&value));
    g_value_unset(&value);
    return res;
}
