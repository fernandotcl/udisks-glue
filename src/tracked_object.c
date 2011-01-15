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
#include "match.h"
#include "matches.h"
#include "property_cache.h"
#include "props.h"
#include "tracked_object.h"

struct tracked_object_ {
    tracked_object_status status;
    DBusGProxy *props_proxy;
    property_cache *props_cache;
    gchar *device_file;
    gchar *mount_point;
    match *match_obj;
};

tracked_object *tracked_object_create(DBusGProxy *props_proxy)
{
    // Allocate the memory
    tracked_object *tobj = g_malloc0(sizeof(tracked_object));

    // Get the device file
    tobj->device_file = get_string_property(props_proxy, "DeviceFile", DBUS_INTERFACE_UDISKS_DEVICE);
    if (!tobj->device_file) {
        g_free(tobj);
        return NULL;
    }

    // Reference the properties proxy
    tobj->props_proxy = props_proxy;
    g_object_ref(props_proxy);

    // Create a new cache
    tobj->props_cache = property_cache_create();

    // Get a weak reference to the match object
    tobj->match_obj = matches_find_match(props_proxy, tobj->props_cache);

    return tobj;
}

void tracked_object_free(tracked_object *tobj)
{
    // Free the properties proxy and the cache
    g_object_unref(tobj->props_proxy);
    property_cache_free(tobj->props_cache);

    // Free the device file
    g_free(tobj->device_file);

    // Free the mount point
    if (tobj->mount_point)
        g_free(tobj->mount_point);

    g_free(tobj);
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

const char *tracked_object_get_post_insertion_command(tracked_object *tobj)
{
    return tobj->match_obj ? match_get_post_insertion_command(tobj->match_obj) : NULL;
}

const char *tracked_object_get_post_mount_command(tracked_object *tobj)
{
    return tobj->match_obj ? match_get_post_mount_command(tobj->match_obj) : NULL;
}

const char *tracked_object_get_post_unmount_command(tracked_object *tobj)
{
    return tobj->match_obj ? match_get_post_unmount_command(tobj->match_obj) : NULL;
}

const char *tracked_object_get_post_removal_command(tracked_object *tobj)
{
    return tobj->match_obj ? match_get_post_removal_command(tobj->match_obj) : NULL;
}
