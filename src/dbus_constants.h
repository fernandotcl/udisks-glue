/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 * © 2011 Jan Palus
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef DBUS_CONSTANTS_H
#define DBUS_CONSTANTS_H

#define DBUS_INTERFACE_UDISKS "org.freedesktop.UDisks"
#define DBUS_INTERFACE_UDISKS_DEVICE "org.freedesktop.UDisks.Device"
#define DBUS_INTERFACE_DBUS_PROPERTIES "org.freedesktop.DBus.Properties"
#define DBUS_INTERFACE_CK "org.freedesktop.ConsoleKit"
#define DBUS_INTERFACE_CK_MANAGER "org.freedesktop.ConsoleKit.Manager"
#define DBUS_INTERFACE_CK_SEAT "org.freedesktop.ConsoleKit.Seat"
#define DBUS_INTERFACE_CK_SESSION "org.freedesktop.ConsoleKit.Session"

#define DBUS_COMMON_NAME_UDISKS "org.freedesktop.UDisks"
#define DBUS_COMMON_NAME_CK "org.freedesktop.ConsoleKit"

#define DBUS_OBJECT_PATH_UDISKS_ROOT "/org/freedesktop/UDisks"
#define DBUS_OBJECT_PATH_CK_ROOT "/org/freedesktop/ConsoleKit"
#define DBUS_OBJECT_PATH_CK_MANAGER DBUS_OBJECT_PATH_CK_ROOT "/Manager"

#endif
