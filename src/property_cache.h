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
#include <stdint.h>

typedef struct property_cache_ property_cache;

property_cache *property_cache_create(void);
void property_cache_free(property_cache *property_cache);

void property_cache_purge(property_cache *cache);

int16_t get_int16_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface, int *success);
int32_t get_int32_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface, int *success);
int64_t get_int64_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface, int *success);
uint16_t get_uint16_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface, int *success);
uint32_t get_uint32_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface, int *success);
uint64_t get_uint64_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface, int *success);
int get_bool_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface);
gchar *get_string_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface);
gchar **get_stringv_property_cached(property_cache *cache, DBusGProxy *proxy, const char *name, const char *interface);

#endif
