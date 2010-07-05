/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef PROPS_H
#define PROPS_H

#include <dbus/dbus-glib.h>
#include <glib.h>

#define BOOL_PROP_TRUE 1
#define BOOL_PROP_FALSE 0
#define BOOL_PROP_ERROR -1

int get_bool_property(DBusGProxy *proxy, const char *name, const char *interface);
gchar *get_string_property(DBusGProxy *proxy, const char *name, const char *interface);
gchar **get_stringv_property(DBusGProxy *proxy, const char *name, const char *interface);

#endif
