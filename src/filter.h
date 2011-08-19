/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef FILTER_H
#define FILTER_H

#include <dbus/dbus-glib.h>

#include "property_cache.h"

typedef struct filter_ filter;
typedef int (*custom_filter)(DBusGProxy *, property_cache *, void *);

filter *filter_create();
void filter_free(filter *f);

void filter_add_restriction_bool(filter *f, const char *property, int value);
void filter_add_restriction_string(filter *f, const char *property, const char *value);
void filter_add_restriction_custom(filter *f, custom_filter match_func, GDestroyNotify free_func, void *cookie, int value);

int filter_matches(filter *f, DBusGProxy *proxy, property_cache *cache);

#endif
