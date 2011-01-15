/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef PROPS_CACHE_H
#define PROPS_CACHE_H

#include <dbus/dbus-glib.h>
#include <glib.h>

typedef struct property_cache_ property_cache;

property_cache *property_cache_create();
void property_cache_free(property_cache *property_cache);

int get_bool_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface);
gchar *get_string_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface);
gchar **get_stringv_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface);

#endif
