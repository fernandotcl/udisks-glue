#ifndef GLOBALS_H
#define GLOBALS_H

#include <dbus/dbus-glib.h>
#include <confuse.h>

extern DBusGConnection *dbus_conn;
extern cfg_t *cfg, *cfg_disks;

#endif
