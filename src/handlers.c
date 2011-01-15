/*
 * This file is part of udisks-glue.
 *
 * © 2010-2011 Fernando Tarlá Cardoso Lemos
 * © 2010-2011 Jan Palus
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <glib.h>

#include "dbus_constants.h"
#include "globals.h"
#include "handlers.h"
#include "props.h"
#include "tracked_object.h"
#include "util.h"

static GHashTable *tracked_objects;

static void post_insertion_procedure(tracked_object *tobj)
{
    gchar *device_file = tracked_object_get_device_file(tobj);
    g_print("Device file %s inserted\n", device_file);

    // Get the post insertion command and run it
    const char *command = tracked_object_get_post_insertion_command(tobj);
    if (command && command[0]) {
        gchar *expanded = str_replace((gchar *)command, "%device_file", device_file);
        run_command(expanded);
        g_free(expanded);
    }
}

static void post_mount_procedure(tracked_object *tobj)
{
    gchar *device_file = tracked_object_get_device_file(tobj);
    gchar *mount_point = tracked_object_get_mount_point(tobj);
    g_print("Device file %s mounted at %s\n", device_file, mount_point);

    // Run the post-mount command
    const char *command = tracked_object_get_post_mount_command(tobj);
    if (command && command[0]) {
        gchar *expanded_tmp = str_replace((gchar *)command, "%device_file", device_file);
        gchar *expanded = str_replace(expanded_tmp, "%mount_point", mount_point);
        g_free(expanded_tmp);
        run_command(expanded);
        g_free(expanded);
    }
}

static void post_unmount_procedure(tracked_object *tobj)
{
    gchar *device_file = tracked_object_get_device_file(tobj);
    gchar *mount_point = tracked_object_get_mount_point(tobj);
    g_print("Device file %s unmounted from %s\n", device_file, mount_point);

    // Run the post-unmount command
    const char *command = tracked_object_get_post_unmount_command(tobj);
    if (command && command[0]) {
        gchar *expanded_tmp = str_replace((gchar *)command, "%device_file", device_file);
        gchar *expanded = str_replace(expanded_tmp, "%mount_point", mount_point);
        g_free(expanded_tmp);
        run_command(expanded);
        g_free(expanded);
    }
}

static void post_removal_procedure(tracked_object *tobj)
{
    gchar *device_file = tracked_object_get_device_file(tobj);
    g_print("Device file %s removed\n", device_file);

    // Run the post-removal command
    const char *command = tracked_object_get_post_removal_command(tobj);
    if (command && command[0]) {
        gchar *expanded = str_replace((gchar *)command, "%device_file", device_file);
        run_command(expanded);
        g_free(expanded);
    }
}

static int load_devices(DBusGProxy *proxy)
{
    // Get a list of devices
    GError *error = NULL;
    GPtrArray *devices;
    gboolean res = dbus_g_proxy_call(proxy, "EnumerateDevices", &error,
            G_TYPE_INVALID,
            dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH),
            &devices,
            G_TYPE_INVALID);
    if (!res) {
        g_printerr("Unable to enumerate the devices: %s\n", error->message);
        g_error_free(error);
        return 0;
    }

    // Run the post insertion procedure on these devices
    for (int i = 0; i < devices->len; ++i) {
        char *object_path = devices->pdata[i];
        device_added_signal_handler(proxy, object_path, NULL);
    }

    g_ptr_array_foreach(devices, (GFunc)g_free, NULL);
    g_ptr_array_free(devices, TRUE);
    return 1;
}

int handlers_init(DBusGProxy *proxy)
{
    // Create the list of tracked objects
    tracked_objects = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, (GDestroyNotify)&tracked_object_free);

    // Load it with the devices that are already present in the system
    return load_devices(proxy);
}

void handlers_free()
{
    if (tracked_objects)
        g_hash_table_destroy(tracked_objects);
}

void device_added_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    // Remove this object in case something funny is going on
    g_hash_table_remove(tracked_objects, object_path);

    // Create the tracked object
    DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_DBUS_PROPERTIES);
    tracked_object *tobj = tracked_object_create(props_proxy);
    g_object_unref(props_proxy);

    // Skip system internal devices
    int is_system_internal = tracked_object_get_bool_property(tobj, "DeviceIsSystemInternal", 0);
    if (is_system_internal != BOOL_PROP_FALSE) {
        tracked_object_free(tobj);
        return;
    }

    // Get some properties
    int is_removable = tracked_object_get_bool_property(tobj, "DeviceIsRemovable", 1);
    int is_mounted = tracked_object_get_bool_property(tobj, "DeviceIsMounted", 0);
    int is_media_available = tracked_object_get_bool_property(tobj, "DeviceIsMediaAvailable", 0);
    if (is_removable == BOOL_PROP_ERROR || is_mounted == BOOL_PROP_ERROR || is_media_available == BOOL_PROP_ERROR) {
        tracked_object_free(tobj);
        return;
    }

    // Add the tracked object to the list of tracked objects
    g_hash_table_insert(tracked_objects, g_strdup(object_path), tobj);

    // If loading devices on init and device is already mounted
    if (is_mounted) {
        tracked_object_set_status(tobj, TRACKED_OBJECT_STATUS_MOUNTED);
        tracked_object_get_mount_point(tobj);
    }

    // If it's not a removable device, run the post-insertion procedure directly
    else if (!is_removable) {
        tracked_object_set_status(tobj, TRACKED_OBJECT_STATUS_INSERTED);
        post_insertion_procedure(tobj);
    }

    // Run the post-insertion procedure if it has media
    else if (is_media_available) {
        tracked_object_set_status(tobj, TRACKED_OBJECT_STATUS_INSERTED);
        post_insertion_procedure(tobj);
    }
}

void device_changed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    // Check if we were tracking this device
    tracked_object *tobj = g_hash_table_lookup(tracked_objects, object_path);
    if (!tobj) return;

    // Get some properties
    int is_mounted = tracked_object_get_bool_property(tobj, "DeviceIsMounted", 0);
    int is_media_available = tracked_object_get_bool_property(tobj, "DeviceIsMediaAvailable", 0);
    if (is_mounted == BOOL_PROP_ERROR || is_media_available == BOOL_PROP_ERROR)
        return;

    switch (tracked_object_get_status(tobj))
    {
        // If media was inserted just now, run the post-insertion procedure
        case TRACKED_OBJECT_STATUS_NO_MEDIA:
            if (is_media_available) {
                tracked_object_set_status(tobj, TRACKED_OBJECT_STATUS_INSERTED);
                post_insertion_procedure(tobj);
            }
            break;

        // If it has been mounted just now, run the post-mount
        case TRACKED_OBJECT_STATUS_INSERTED:
            if (is_mounted) {
                if (tracked_object_get_mount_point(tobj)) {
                    tracked_object_set_status(tobj, TRACKED_OBJECT_STATUS_MOUNTED);
                    post_mount_procedure(tobj);
                }
            }
            else if (!is_media_available) {
                tracked_object_set_status(tobj, TRACKED_OBJECT_STATUS_NO_MEDIA);
                post_removal_procedure(tobj);
            }
            break;

        // If it has been unmounted just now, run the post-unmount procedure
        case TRACKED_OBJECT_STATUS_MOUNTED:
            if (!is_mounted) {
                tracked_object_set_status(tobj, is_media_available ? TRACKED_OBJECT_STATUS_INSERTED : TRACKED_OBJECT_STATUS_NO_MEDIA);
                post_unmount_procedure(tobj);
            }
            break;
    }
}

void device_removed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    // Check if we were tracking this device
    tracked_object *tobj = g_hash_table_lookup(tracked_objects, object_path);
    if (!tobj) return;

    // If the object had no media, simply stop tracking it
    tracked_object_status status = tracked_object_get_status(tobj);
    if (status == TRACKED_OBJECT_STATUS_NO_MEDIA) {
        g_hash_table_remove(tracked_objects, object_path);
        return;
    }

    // Remove the reference to the object from the table to avoid races
    g_hash_table_steal(tracked_objects, object_path);

    // If the device was mounted, run the post-unmount procedure
    if (status == TRACKED_OBJECT_STATUS_MOUNTED)
        post_unmount_procedure(tobj);

    // Run the post-removal procedure and free the tracked object
    post_removal_procedure(tobj);
    tracked_object_free(tobj);
}
