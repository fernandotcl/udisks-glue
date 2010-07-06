/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <glib.h>
#include <string.h>

#include "dbus_constants.h"
#include "filters.h"
#include "globals.h"
#include "props.h"
#include "util.h"

#define RETURN_IF_DEVICE_PROPERTY(name) \
    do { \
        int res = get_bool_property(props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE); \
        if (res != BOOL_PROP_FALSE) { \
            g_object_unref(props_proxy); \
            return; \
        } \
    } while (0)

#define RETURN_IF_NOT_DEVICE_PROPERTY(name) \
    do { \
        int res = get_bool_property(props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE); \
        if (res != BOOL_PROP_TRUE) { \
            g_object_unref(props_proxy); \
            return; \
        } \
    } while (0)

#define GET_BOOL_DEVICE_PROPERTY(name, var) \
    int var; \
    do { \
        var = get_bool_property(props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE); \
        if (var == BOOL_PROP_ERROR) { \
            g_object_unref(props_proxy); \
            return; \
        } \
    } while (0)

typedef struct {
    gchar *device_file;
    int is_optical_disc;
    const candidate *post_removal_candidate;
} mounted_device;

static GSList *empty_optical_drives = NULL;
static GSList *pending_mount_devices = NULL;
static GHashTable *mounted_devices = NULL;

static void free_mounted_device(mounted_device *md)
{
    g_free(md->device_file);
    g_free(md);
}

static void run_device_command(const char *command, const char *device_file, const char *mount_point)
{
    // Expand the %device_file and %mount_point tokens
    gchar *expanded = str_replace((gchar *)command, "%device_file", (gchar *)device_file);
    if (mount_point) {
        gchar *tmp = str_replace((gchar *)expanded, "%mount_point", (gchar *)mount_point);
        g_free(expanded);
        expanded = tmp;
    }

    // Run the command
    g_print("Running command: %s\n", expanded);
    run_command((const char *)expanded);
    g_free(expanded);
}

static void try_post_mount_command(DBusGProxy *props_proxy, const char *object_path, int optical)
{
    // Get the device file
    gchar *device_file = get_string_property(props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!device_file) {
        g_object_unref(props_proxy);
        return;
    }

    // Get the mount path
    gchar **mount_paths = get_stringv_property(props_proxy, "DeviceMountPaths", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!mount_paths) return;
    if (!*mount_paths) {
        g_debug("%s %s was mounted, but not anymore", optical ? "Disc" : "Device", device_file);
        g_strfreev(mount_paths);
        g_free(device_file);
        return;
    }
    g_print("%s mounted: %s on %s\n", optical ? "Disc" : "Device", device_file, *mount_paths);

    // Run the post-mount command
    filter_info info = { props_proxy, optical, FILTER_COMMAND_POST_MOUNT };
    const char *command = filters_get_command(&info);
    if (command) {
        mounted_device *md = g_malloc(sizeof(mounted_device));
        md->device_file = g_strdup(device_file);
        md->is_optical_disc = optical;
        info.requested_command = FILTER_COMMAND_POST_REMOVAL;
        md->post_removal_candidate = filters_get_candidate(&info);
        g_hash_table_insert(mounted_devices, g_strdup(object_path), md);
        run_device_command(command, device_file, *mount_paths);
    }

    g_strfreev(mount_paths);
    g_free(device_file);
}

static void optical_drive_media_inserted(DBusGProxy *props_proxy, const char *object_path)
{
    // If for some reason the device is mounted, check if we need to try to run the post-mount command
    GET_BOOL_DEVICE_PROPERTY("DeviceIsMounted", device_is_mounted);
    if (device_is_mounted) {
        GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
        if (entry) {
            g_free(entry->data);
            pending_mount_devices = g_slist_delete_link(pending_mount_devices, entry);
            try_post_mount_command(props_proxy, object_path, 1);
        }
        return;
    }

    // Get the device file
    gchar *device_file = get_string_property(props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!device_file) {
        g_object_unref(props_proxy);
        return;
    }
    g_print("Disc inserted: %s\n", device_file);

    // Otherwise try to run the post-insertion command
    filter_info info = { props_proxy, 1, FILTER_COMMAND_POST_INSERTION };
    const char *command = filters_get_command(&info);
    if (command) {
        pending_mount_devices = g_slist_prepend(pending_mount_devices, g_strdup(object_path));
        run_device_command(command, device_file, NULL);
    }

    g_free(device_file);
}

static void optical_drive_media_removed(DBusGProxy *props_proxy, const char *object_path)
{
    // If we had run the mount command, forget this device
    GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
    if (entry) {
        g_free(entry->data);
        pending_mount_devices = g_slist_delete_link(pending_mount_devices, entry);
    }

    // If we didn't run the insertion command on it, there's nothing to do
    if (!g_hash_table_lookup(mounted_devices, object_path))
        return;

    // Get the device file
    gchar *device_file = get_string_property(props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!device_file) {
        g_object_unref(props_proxy);
        return;
    }
    g_print("Drive removed: %s\n", device_file);

    // Run the post-removal command
    g_hash_table_remove(mounted_devices, object_path);
    filter_info info = { props_proxy, 1, FILTER_COMMAND_POST_REMOVAL };
    const char *command = filters_get_command(&info);
    if (command)
        run_device_command(command, device_file, NULL);

    g_free(device_file);
}

void handlers_init()
{
    // Create the association between mounted devices and device files
    mounted_devices = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, (GDestroyNotify)&free_mounted_device);

    // Create a list of empty optical drives
    // TODO
}

void handlers_free()
{
    g_hash_table_destroy(mounted_devices);

    g_slist_foreach(pending_mount_devices, (GFunc)g_free, NULL);
    g_slist_free(pending_mount_devices);

    g_slist_foreach(empty_optical_drives, (GFunc)g_free, NULL);
    g_slist_free(empty_optical_drives);
}

void device_added_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    // Remove entries in the lists if they exist
    GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
    if (entry) {
        g_free(entry->data);
        pending_mount_devices = g_slist_delete_link(pending_mount_devices, entry);
    }
    g_hash_table_remove(mounted_devices, object_path);

    // Get the proxy and skip internal devices
    DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_DBUS_PROPERTIES);
    RETURN_IF_DEVICE_PROPERTY("DeviceIsSystemInternal");

    // If the device is an optical device, check if it's empty or not and update the list
    // of empty optical devices accordingly
    GET_BOOL_DEVICE_PROPERTY("DeviceIsOpticalDisc", device_is_optical);
    if (device_is_optical == BOOL_PROP_TRUE) {
        GET_BOOL_DEVICE_PROPERTY("DeviceIsMediaAvailable", device_has_media);
        entry = g_slist_find_custom(empty_optical_drives, object_path, (GCompareFunc)strcmp);
        if (device_has_media == BOOL_PROP_TRUE) {
            if (entry) {
                g_free(entry->data);
                empty_optical_drives = g_slist_delete_link(empty_optical_drives, entry);
            }
        }
        else if (!entry) {
            empty_optical_drives = g_slist_prepend(empty_optical_drives, g_strdup(object_path));
        }
    }

    else {
        // If the device is already mounted, there's nothing to do
        RETURN_IF_DEVICE_PROPERTY("DeviceIsMounted");

        // Identify the device file
        gchar *device_file = get_string_property(props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
        if (!device_file) {
            g_object_unref(props_proxy);
            return;
        }
        g_print("Device added: %s\n", device_file);

        // Run the post-insertion command
        filter_info info = { props_proxy, 0, FILTER_COMMAND_POST_INSERTION };
        const char *command = filters_get_command(&info);
        if (command) {
            pending_mount_devices = g_slist_prepend(pending_mount_devices, g_strdup(object_path));
            run_device_command(command, device_file, NULL);
        }

        g_free(device_file);
    }

    g_object_unref(props_proxy);
}

void device_changed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    // Get the proxy and skip internal devices
    DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_DBUS_PROPERTIES);
    RETURN_IF_DEVICE_PROPERTY("DeviceIsSystemInternal");

    // If it was an optical device, check if it was empty before and not anymore
    // (insertion) or if it is empty now but wasn't empty before (removal)
    GET_BOOL_DEVICE_PROPERTY("DeviceIsOpticalDisc", device_is_optical);
    if (device_is_optical == BOOL_PROP_TRUE) {
        GET_BOOL_DEVICE_PROPERTY("DeviceIsMediaAvailable", media_available);
        GSList *entry = g_slist_find_custom(empty_optical_drives, object_path, (GCompareFunc)strcmp);
        if (media_available && entry) {
            g_free(entry->data);
            empty_optical_drives = g_slist_delete_link(empty_optical_drives, entry);
            optical_drive_media_inserted(props_proxy, object_path);
        }
        else if (!media_available && !entry) {
            empty_optical_drives = g_slist_prepend(empty_optical_drives, g_strdup(object_path));
            optical_drive_media_removed(props_proxy, object_path);
        }
    }

    // Otherwise check if we need to run the post-mount command
    else {
        RETURN_IF_NOT_DEVICE_PROPERTY("DeviceIsMounted");
        GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
        if (entry) {
            g_free(entry->data);
            pending_mount_devices = g_slist_delete_link(pending_mount_devices, entry);
            try_post_mount_command(props_proxy, object_path, 0);
        }
    }

    g_object_unref(props_proxy);
}

void device_removed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    // Remove the entry from the pending mount devices list
    GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
    if (entry) {
        g_free(entry->data);
        pending_mount_devices = g_slist_delete_link(pending_mount_devices, entry);
    }

    // Remove the entry from the empty optical drives list
    entry = g_slist_find_custom(empty_optical_drives, object_path, (GCompareFunc)strcmp);
    if (entry) {
        g_free(entry->data);
        empty_optical_drives = g_slist_delete_link(empty_optical_drives, entry);
    }

    // If the device was never considered mounted, there's nothing to do
    mounted_device *md = g_hash_table_lookup(mounted_devices, object_path);
    if (!md) return;
    g_print("%s removed: %s\n", md->is_optical_disc ? "Disc" : "Device", md->device_file);

    // If we have a post-removal candidate, try use it
    if (md->post_removal_candidate) {
        filter_info info = { NULL, 0, FILTER_COMMAND_POST_REMOVAL };
        const char *command = filters_get_command_in_candidate(&info, md->post_removal_candidate);
        if (command)
            run_device_command(command, md->device_file, NULL);
    }

    g_hash_table_remove(mounted_devices, object_path);
}
