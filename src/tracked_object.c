/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <glib.h>

#include "dbus_constants.h"
#include "globals.h"
#include "match.h"
#include "matches.h"
#include "property_cache.h"
#include "props.h"
#include "tracked_object.h"

struct tracked_object_ {
    tracked_object_status status;
    DBusGProxy *device_proxy;
    DBusGProxy *props_proxy;
    property_cache *props_cache;
    gchar *device_file;
    gchar *mount_point;
    match *match_obj;
    int match_obj_loaded;
};

tracked_object *tracked_object_create(const char *object_path)
{
    // Allocate the memory
    tracked_object *tobj = g_malloc0(sizeof(tracked_object));

    // Create the proxies
    tobj->device_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_UDISKS_DEVICE);
    tobj->props_proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, object_path, DBUS_INTERFACE_DBUS_PROPERTIES);

    // Get the device file
    tobj->device_file = get_string_property(tobj->props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!tobj->device_file) {
        g_object_unref(tobj->device_proxy);
        g_object_unref(tobj->props_proxy);
        g_free(tobj);
        return NULL;
    }

    // Create a new cache
    tobj->props_cache = property_cache_create();

    // Get a weak reference to the match object
    tobj->match_obj = matches_find_match(tobj->props_proxy, tobj->props_cache);
    tobj->match_obj_loaded = 1;

    return tobj;
}

void tracked_object_free(tracked_object *tobj)
{
    // Free the proxies
    g_object_unref(tobj->device_proxy);
    g_object_unref(tobj->props_proxy);

    // Free the properties cache
    property_cache_free(tobj->props_cache);

    // Free the device file
    g_free(tobj->device_file);

    // Free the mount point
    if (tobj->mount_point)
        g_free(tobj->mount_point);

    g_free(tobj);
}

void tracked_object_purge_cache(tracked_object *tobj)
{
    // Purge the properties cache
    property_cache_purge(tobj->props_cache);

    // Get rid of the mount point
    if (tobj->mount_point) {
        g_free(tobj->mount_point);
        tobj->mount_point = NULL;
    }

    // Unload the match object
    tobj->match_obj_loaded = 0;
    tobj->match_obj = NULL;
}

tracked_object_status tracked_object_get_status(tracked_object *tobj)
{
    return tobj->status;
}

void tracked_object_set_status(tracked_object *tobj, tracked_object_status status)
{
    tobj->status = status;
}

gchar *tracked_object_get_device_file(tracked_object *tobj)
{
    return tobj->device_file;
}

gchar *tracked_object_get_mount_point(tracked_object *tobj)
{
    if (tobj->mount_point)
        return tobj->mount_point;

    gchar **mount_paths = get_stringv_property(tobj->props_proxy, "DeviceMountPaths", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!mount_paths)
        return NULL;

    if (!*mount_paths) {
        g_strfreev(mount_paths);
        return NULL;
    }

    tobj->mount_point = g_strdup(*mount_paths);
    return tobj->mount_point;
}

int tracked_object_get_bool_property(tracked_object *tobj, const char *name, int cached)
{
    if (cached)
        return get_bool_property_cached(tobj->props_cache, tobj->props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE);
    else
        return get_bool_property(tobj->props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE);
}

gchar *tracked_object_get_string_property(tracked_object *tobj, const char *name, int cached)
{
    if (cached)
        return get_string_property_cached(tobj->props_cache, tobj->props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE);
    else
        return get_string_property(tobj->props_proxy, name, DBUS_INTERFACE_UDISKS_DEVICE);
}

#define LOAD_MATCH_OBJ \
    do { \
        if (!tobj->match_obj_loaded) { \
            tobj->match_obj = matches_find_match(tobj->props_proxy, tobj->props_cache); \
            tobj->match_obj_loaded = 1; \
        } \
        if (!tobj->match_obj) \
            return NULL; \
    } while (0)

const char *tracked_object_get_post_insertion_command(tracked_object *tobj)
{
    LOAD_MATCH_OBJ;
    return match_get_post_insertion_command(tobj->match_obj);
}

const char *tracked_object_get_post_mount_command(tracked_object *tobj)
{
    LOAD_MATCH_OBJ;
    return match_get_post_mount_command(tobj->match_obj);
}

const char *tracked_object_get_post_unmount_command(tracked_object *tobj)
{
    LOAD_MATCH_OBJ;
    return match_get_post_unmount_command(tobj->match_obj);
}

const char *tracked_object_get_post_removal_command(tracked_object *tobj)
{
    LOAD_MATCH_OBJ;
    return match_get_post_removal_command(tobj->match_obj);
}

void tracked_object_automount_if_needed(tracked_object *tobj)
{
    if (!tobj->match_obj_loaded) {
        tobj->match_obj = matches_find_match(tobj->props_proxy, tobj->props_cache);
        tobj->match_obj_loaded = 1;
    }
    if (!tobj->match_obj || !match_get_automount(tobj->match_obj))
        return;

    g_print("Trying to automount %s...\n", tobj->device_file);

    GError *error = NULL;
    gchar *mount_point = NULL;
    gboolean res = dbus_g_proxy_call(tobj->device_proxy, "FilesystemMount", &error,
            G_TYPE_STRING, match_get_automount_filesystem(tobj->match_obj),
            G_TYPE_STRV, match_get_automount_options(tobj->match_obj),
            G_TYPE_INVALID,
            G_TYPE_STRING, &mount_point,
            G_TYPE_INVALID);

    if (res) {
        if (mount_point) {
            if (tobj->mount_point)
                g_free(tobj->mount_point);
            tobj->mount_point = mount_point;
            g_print("Successfully automounted %s at %s\n", tobj->device_file, mount_point);
        }
        else {
            g_print("Successfully automounted %s\n", tobj->device_file);
        }
    }
    else {
        g_printerr("Failed to automount %s: %s\n", tobj->device_file, error->message);
        g_error_free(error);
    }
}
