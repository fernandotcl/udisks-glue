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
#include <stdint.h>

#define BOOL_PROP_TRUE 1
#define BOOL_PROP_FALSE 0
#define BOOL_PROP_ERROR -1

int16_t get_int16_property(DBusGProxy *proxy, const char *name, const char *interface, int *success);
int32_t get_int32_property(DBusGProxy *proxy, const char *name, const char *interface, int *success);
int64_t get_int64_property(DBusGProxy *proxy, const char *name, const char *interface, int *success);
uint16_t get_uint16_property(DBusGProxy *proxy, const char *name, const char *interface, int *success);
uint32_t get_uint32_property(DBusGProxy *proxy, const char *name, const char *interface, int *success);
uint64_t get_uint64_property(DBusGProxy *proxy, const char *name, const char *interface, int *success);
int get_bool_property(DBusGProxy *proxy, const char *name, const char *interface);
gchar *get_string_property(DBusGProxy *proxy, const char *name, const char *interface);
gchar **get_stringv_property(DBusGProxy *proxy, const char *name, const char *interface);

#endif
