/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef TRACKED_OBJECT_H
#define TRACKED_OBJECT_H

#include <dbus/dbus-glib.h>
#include <glib.h>

typedef enum {
    TRACKED_OBJECT_STATUS_NO_MEDIA = 0,
    TRACKED_OBJECT_STATUS_INSERTED,
    TRACKED_OBJECT_STATUS_MOUNTED
} tracked_object_status;

typedef struct tracked_object_ tracked_object;

tracked_object *tracked_object_create(DBusGProxy *props_proxy);
void tracked_object_free();

tracked_object_status tracked_object_get_status(tracked_object *tobj);
void tracked_object_set_status(tracked_object *tobj, tracked_object_status status);

gchar *tracked_object_get_device_file(tracked_object *tobj);
gchar *tracked_object_get_mount_point(tracked_object *tobj);

int tracked_object_get_bool_property(tracked_object *tobj, const char *name, int cached);
gchar *tracked_object_get_string_property(tracked_object *tobj, const char *name, int cached);

const char *tracked_object_get_post_insertion_command(tracked_object *tobj);
const char *tracked_object_get_post_mount_command(tracked_object *tobj);
const char *tracked_object_get_post_unmount_command(tracked_object *tobj);
const char *tracked_object_get_post_removal_command(tracked_object *tobj);

#endif
