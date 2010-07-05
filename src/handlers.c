/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <confuse.h>
#include <glib.h>
#include <string.h>

#include "dbus_constants.h"
#include "globals.h"
#include "props.h"
#include "util.h"

#define RETURN_IF_DEVICE_PROPERTY(name) \
    do { \
        int res = get_bool_property(props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE); \
        if (res != 0) { \
            g_object_unref(props_proxy); \
            return; \
        } \
    } while (0)

#define RETURN_IF_NOT_DEVICE_PROPERTY(name) \
    do { \
        int res = get_bool_property(props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE); \
        if (res != 1) { \
            g_object_unref(props_proxy); \
            return; \
        } \
    } while (0)

static GSList *pending_mount_devices = NULL;
static GHashTable *mounted_devices = NULL;

static void run_device_command(const char *config_key, const char *device_file, const char *mount_point)
{
    const char *command = cfg_getstr(cfg_disks, config_key);
    if (!command) return;

    gchar *expanded = str_replace((gchar *)command, "%device_file", (gchar *)device_file);
    if (mount_point) {
        gchar *tmp = str_replace((gchar *)expanded, "%mount_point", (gchar *)mount_point);
        g_free(expanded);
        expanded = tmp;
    }

    g_print("Running command: %s\n", expanded);
    run_command((const char *)expanded);
    g_free(expanded);
}

void handlers_init()
{
    mounted_devices = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, &g_free);
}

void handlers_free()
{
    g_hash_table_destroy(mounted_devices);
    g_slist_foreach(pending_mount_devices, (GFunc)g_free, NULL);
    g_slist_free(pending_mount_devices);
}

void device_added_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_DBUS_PROPERTIES);
    RETURN_IF_DEVICE_PROPERTY("DeviceIsSystemInternal");
    RETURN_IF_DEVICE_PROPERTY("DeviceIsMounted");
    RETURN_IF_DEVICE_PROPERTY("DeviceIsOpticalDisc");

    gchar *device_file = get_string_property(props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!device_file) return;

    g_hash_table_remove(mounted_devices, object_path);
    GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
    if (!entry)
        pending_mount_devices = g_slist_prepend(pending_mount_devices, g_strdup(object_path));

    g_print("Device added: %s\n", device_file);
    run_device_command("post_insertion_command", device_file, NULL);

    g_free(device_file);
    g_object_unref(props_proxy);
}

void device_changed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
    if (!entry) return;

    if (g_hash_table_lookup(mounted_devices, object_path)) return;

    DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_DBUS_PROPERTIES);
    RETURN_IF_NOT_DEVICE_PROPERTY("DeviceIsMounted");

    gchar *device_file = get_string_property(props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!device_file) return;

    gchar **mount_paths = get_stringv_property(props_proxy, "DeviceMountPaths", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!mount_paths) return;
    if (!*mount_paths) {
        g_debug("Device %s was mounted, but not anymore", device_file);
        g_strfreev(mount_paths);
        g_free(device_file);
        return;
    }

    g_free(entry->data);
    pending_mount_devices = g_slist_delete_link(pending_mount_devices, entry);
    g_hash_table_insert(mounted_devices, g_strdup(object_path), g_strdup(device_file));

    g_print("Device mounted: %s on %s\n", device_file, *mount_paths);
    run_device_command("post_mount_command", device_file, *mount_paths);

    g_strfreev(mount_paths);
    g_free(device_file);
}

void device_removed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
    if (entry) {
        g_free(entry->data);
        pending_mount_devices = g_slist_delete_link(pending_mount_devices, entry);
        return;
    }

    gchar *device_file = g_hash_table_lookup(mounted_devices, object_path);
    if (!device_file) return;

    g_print("Device removed: %s\n", device_file);
    run_device_command("post_removal_command", device_file, NULL);

    g_hash_table_remove(mounted_devices, object_path);
}
