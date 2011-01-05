/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 * © 2011 Jan Palus
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <dbus/dbus-glib.h>
#include <glib.h>

extern DBusGConnection *dbus_conn;
extern GMainLoop *loop;

#endif
