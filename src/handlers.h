/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef HANDLERS_H
#define HANDLERS_H

#include <dbus/dbus-glib.h>

void handlers_init();
void handlers_free();

void device_added_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data);
void device_changed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data);
void device_removed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data);

#endif
