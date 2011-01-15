/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef MATCHES_H
#define MATCHES_H

#include <dbus/dbus-glib.h>

#include "property_cache.h"

int matches_init(cfg_t *cfg);
void matches_free();

match *matches_find_match(DBusGProxy *proxy, property_cache *cache);

#endif
