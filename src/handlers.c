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

#define GET_BOOL_PROPERTY_RETURN(name, var) \
    int var; \
    do { \
        var = get_bool_property(props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE); \
        if (var == BOOL_PROP_ERROR) { \
            g_object_unref(props_proxy); \
            return; \
        } \
    } while (0)

#define GET_BOOL_PROPERTY_CONTINUE(name, var) \
    int var; \
    do { \
        var = get_bool_property(props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE); \
        if (var == BOOL_PROP_ERROR) { \
            g_object_unref(props_proxy); \
            continue; \
        } \
    } while (0)

typedef struct {
    enum {
        TRACKED_OBJECT_STATUS_NO_MEDIA,
        TRACKED_OBJECT_STATUS_INSERTED,
        TRACKED_OBJECT_STATUS_MOUNTED
    } status;
    gchar *device_file;
    gchar *mount_point;
    const char *post_mount_command;
    const char *post_removal_command;
} tracked_object;

static GHashTable *tracked_objects;

static void free_tracked_object(tracked_object *tobj)
{
    g_free(tobj->device_file);
    if (tobj->mount_point)
        g_free(tobj->mount_point);
    g_free(tobj);
}

static void post_insertion_procedure(tracked_object *tobj, DBusGProxy *props_proxy)
{
    g_print("Device file %s inserted\n", tobj->device_file);

    // Look for the commands
    filter_cache *cache = filter_cache_create();
    const char *command = filters_get_command(props_proxy, FILTER_COMMAND_POST_INSERTION, cache);
    tobj->post_mount_command = filters_get_command(props_proxy, FILTER_COMMAND_POST_MOUNT, cache);
    tobj->post_removal_command = filters_get_command(props_proxy, FILTER_COMMAND_POST_REMOVAL, cache);
    filter_cache_free(cache);

    // Run the post-insertion command
    if (command && command[0]) {
        gchar *expanded = str_replace((gchar *)command, "%device_file", tobj->device_file);
        run_command(expanded);
        g_free(expanded);
    }
}

static void post_mount_procedure(tracked_object *tobj)
{
    g_print("Device file %s mounted at %s\n", tobj->device_file, tobj->mount_point);

    // Run the post-mount command
    if (tobj->post_mount_command && tobj->post_mount_command[0]) {
        gchar *expanded_tmp = str_replace((gchar *)tobj->post_mount_command, "%device_file", tobj->device_file);
        gchar *expanded = str_replace(expanded_tmp, "%mount_point", tobj->mount_point);
        g_free(expanded_tmp);
        run_command(expanded);
        g_free(expanded);
    }
}

static void post_removal_procedure(tracked_object *tobj)
{
    g_print("Device file %s removed\n", tobj->device_file);

    // Run the post-removal command
    if (tobj->post_removal_command && tobj->post_removal_command[0]) {
        gchar *expanded = str_replace((gchar *)tobj->post_removal_command, "%device_file", tobj->device_file);
        run_command(expanded);
        g_free(expanded);
    }
}

static gchar *get_mount_point(DBusGProxy *props_proxy)
{
    gchar **mount_paths = get_stringv_property(props_proxy, "DeviceMountPaths", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!mount_paths)
        return NULL;

    if (!*mount_paths) {
        g_strfreev(mount_paths);
        return NULL;
    }

    return g_strdup(*mount_paths);
}

static void load_devices(DBusGProxy *proxy)
{
    // Get a list of devices
    GError *error;
    GPtrArray *devices;
    gboolean res = dbus_g_proxy_call(proxy, "EnumerateDevices", &error,
            G_TYPE_INVALID,
            dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH),
            &devices,
            G_TYPE_INVALID);
    if (!res) {
        g_printerr("Unable to enumerate the devices: %s\n", error->message);
        g_error_free(error);
        return;
    }

    for (int i = 0; i < devices->len; ++i) {
        // Get the properties proxy
        char *object_path = devices->pdata[i];
        DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_DBUS_PROPERTIES);

        // Skip system internal devices
        int is_system_internal = get_bool_property(props_proxy, "DeviceIsSystemInternal", DBUS_INTERFACE_UDISKS_DEVICE);
        if (is_system_internal != BOOL_PROP_FALSE) {
            g_object_unref(props_proxy);
            continue;
        }

        // Get some properties
        GET_BOOL_PROPERTY_CONTINUE("DeviceIsMediaAvailable", is_media_available);
        GET_BOOL_PROPERTY_CONTINUE("DeviceIsMounted", is_mounted);

        // Get the device file
        gchar *device_file = get_string_property(props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
        if (!device_file) {
            g_object_unref(props_proxy);
            continue;
        }

        // Create and insert the new tracked object
        tracked_object *tobj = g_malloc0(sizeof(tracked_object));
        if (!tobj) {
            g_printerr("g_malloc failed\n");
            g_object_unref(props_proxy);
            g_free(device_file);
            return;
        }
        g_hash_table_insert(tracked_objects, g_strdup(object_path), tobj);

        // Configure the tracked object
        tobj->device_file = device_file;
        if (is_mounted)
            tobj->status = TRACKED_OBJECT_STATUS_MOUNTED;
        else if (is_media_available)
            tobj->status = TRACKED_OBJECT_STATUS_INSERTED;
        else
            tobj->status = TRACKED_OBJECT_STATUS_NO_MEDIA;
    }

    g_ptr_array_foreach(devices, (GFunc)g_free, NULL);
    g_ptr_array_free(devices, TRUE);
}

void handlers_init(DBusGProxy *proxy)
{
    // Create the list of tracked objects
    tracked_objects = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, (GDestroyNotify)&free_tracked_object);

    // Load it with the devices that are already present in the system
    load_devices(proxy);
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

    // Skip system internal devices
    DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_DBUS_PROPERTIES);
    GET_BOOL_PROPERTY_RETURN("DeviceIsSystemInternal", is_system_internal);
    if (is_system_internal != BOOL_PROP_FALSE) {
        g_object_unref(props_proxy);
        return;
    }

    // Get some properties
    GET_BOOL_PROPERTY_RETURN("DeviceIsRemovable", is_removable);
    GET_BOOL_PROPERTY_RETURN("DeviceIsMediaAvailable", is_media_available);

    // Get the device file
    gchar *device_file = get_string_property(props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!device_file) {
        g_object_unref(props_proxy);
        return;
    }

    // Create a new tracked object
    tracked_object *tobj = g_malloc0(sizeof(tracked_object));
    if (!tobj) {
        g_printerr("g_malloc failed\n");
        g_free(device_file);
        g_object_unref(props_proxy);
        return;
    }
    tobj->device_file = device_file;
    g_hash_table_insert(tracked_objects, g_strdup(object_path), tobj);

    // If it's not a removable device, run the post-insertion procedure directly
    if (!is_removable) {
        tobj->status = TRACKED_OBJECT_STATUS_INSERTED;
        post_insertion_procedure(tobj, props_proxy);
        g_object_unref(props_proxy);
        return;
    }

    // Run the post-insertion procedure if it has media
    if (is_media_available) {
        tobj->status = TRACKED_OBJECT_STATUS_INSERTED;
        post_insertion_procedure(tobj, props_proxy);
        g_object_unref(props_proxy);
        return;
    }

    // The object has no media, nothing to do with it yet
    tobj->status = TRACKED_OBJECT_STATUS_NO_MEDIA;
    g_object_unref(props_proxy);
}

void device_changed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    // Check if we were tracking this device
    tracked_object *tobj = g_hash_table_lookup(tracked_objects, object_path);
    if (!tobj) return;

    // Get some properties
    DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_DBUS_PROPERTIES);
    GET_BOOL_PROPERTY_RETURN("DeviceIsMounted", is_mounted);
    GET_BOOL_PROPERTY_RETURN("DeviceIsMediaAvailable", is_media_available);

    switch (tobj->status)
    {
        // If media was inserted just now, run the post-insertion procedure
        case TRACKED_OBJECT_STATUS_NO_MEDIA:
            if (is_media_available) {
                tobj->status = TRACKED_OBJECT_STATUS_INSERTED;
                post_insertion_procedure(tobj, props_proxy);
            }
            break;

        // If it has been mounted just now, run the post-mount
        case TRACKED_OBJECT_STATUS_INSERTED:
            if (is_mounted) {
                tobj->mount_point = get_mount_point(props_proxy);
                if (tobj->mount_point) {
                    tobj->status = TRACKED_OBJECT_STATUS_MOUNTED;
                    post_mount_procedure(tobj);
                }
            }
            else if (!is_media_available) {
                tobj->status = TRACKED_OBJECT_STATUS_NO_MEDIA;
                post_removal_procedure(tobj);
            }
            break;

        // If it has been unmounted just now, run the post-removal procedure
        case TRACKED_OBJECT_STATUS_MOUNTED:
            if (!is_mounted) {
                tobj->status = is_media_available ? TRACKED_OBJECT_STATUS_INSERTED : TRACKED_OBJECT_STATUS_NO_MEDIA;
                post_removal_procedure(tobj);
            }
            break;
    }

    g_object_unref(props_proxy);
}

void device_removed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    // Check if we were tracking this device
    tracked_object *tobj = g_hash_table_lookup(tracked_objects, object_path);
    if (!tobj) return;

    // If the object had no media, simply stop tracking it
    if (tobj->status == TRACKED_OBJECT_STATUS_NO_MEDIA) {
        g_hash_table_remove(tracked_objects, object_path);
        return;
    }

    // Remove the reference to the object from the table to avoid races
    g_hash_table_steal(tracked_objects, object_path);

    // Run the post-removal procedure and free the tracked object
    post_removal_procedure(tobj);
    free_tracked_object(tobj);
}
