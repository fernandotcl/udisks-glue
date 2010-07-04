/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <confuse.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "globals.h"

#define GET_PROPERTY_PREAMBLE(error_val) \
    GValue value = {0, }; \
    do { \
        GError *error = NULL; \
        if (!dbus_g_proxy_call(props_proxy, "Get", &error, \
                    G_TYPE_STRING, "org.freedesktop.UDisks.Device", \
                    G_TYPE_STRING, name, \
                    G_TYPE_INVALID, \
                    G_TYPE_VALUE, &value, \
                    G_TYPE_INVALID)) { \
            g_printerr("Unable to get property \"%s\": %s\n", name, error->message); \
            g_error_free(error); \
            return error_val; \
        } \
    } while (0)

#define RETURN_IF_PROPERTY(name) \
    do { \
        int res = get_bool_property(props_proxy, name); \
        if (res != 0) { \
            g_object_unref(props_proxy); \
            return; \
        } \
    } while (0)

#define RETURN_IF_NOT_PROPERTY(name) \
    do { \
        int res = get_bool_property(props_proxy, name); \
        if (res != 1) { \
            g_object_unref(props_proxy); \
            return; \
        } \
    } while (0)

static GSList *pending_mount_devices = NULL;
static GHashTable *mounted_devices = NULL;

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

static gchar *replace_strings(gchar *string, gchar *search, gchar *replacement)
{
    gchar *str;
    gchar **arr = g_strsplit(string, search, -1);
    if (arr != NULL && arr[0] != NULL)
        str = g_strjoinv(replacement, arr);
    else
        str = g_strdup(string);

    g_strfreev(arr);
    return str;
}

static void run_command(const char *config_key, const char *device_file, const char *mount_point)
{
    const char *command = cfg_getstr(cfg_disks, config_key);
    if (!command) return;

    gchar *expanded = replace_strings((gchar *)command, "%device_file", (gchar *)device_file);
    if (mount_point) {
        gchar *tmp = replace_strings((gchar *)expanded, "%mount_point", (gchar *)mount_point);
        g_free(expanded);
        expanded = tmp;
    }

    g_print("Running command: %s\n", expanded);

    if (fork() == 0) {
        if (fork() == 0) {
            static const char *shell = NULL;
            if (!shell) {
                shell = getenv("SHELL");
                if (!shell)
                    shell = "/bin/sh";
            }
            execl(shell, shell, "-c", expanded, NULL);
        }
        exit(0);
    }
    wait(NULL);
}

static int get_bool_property(DBusGProxy *props_proxy, const char *name)
{
    GET_PROPERTY_PREAMBLE(-1);
    int res = g_value_get_boolean(&value) ? 1 : 0;
    g_value_unset(&value);
    return res;
}

static gchar *get_string_property(DBusGProxy *props_proxy, const char *name)
{
    GET_PROPERTY_PREAMBLE(NULL);
    gchar *res = g_strdup(g_value_get_string(&value));
    g_value_unset(&value);
    return res;
}

static gchar **get_stringv_property(DBusGProxy *props_proxy, const char *name)
{
    GET_PROPERTY_PREAMBLE(NULL);
    gchar **res = g_strdupv(g_value_get_boxed(&value));
    g_value_unset(&value);
    return res;
}

void device_added_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, "org.freedesktop.UDisks", object_path, "org.freedesktop.DBus.Properties");
    RETURN_IF_PROPERTY("DeviceIsSystemInternal");
    RETURN_IF_PROPERTY("DeviceIsMounted");
    RETURN_IF_PROPERTY("DeviceIsOpticalDisc");

    gchar *device_file = get_string_property(props_proxy, "DeviceFile");
    if (!device_file) return;

    g_hash_table_remove(mounted_devices, object_path);
    GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
    if (!entry)
        pending_mount_devices = g_slist_prepend(pending_mount_devices, g_strdup(object_path));

    g_print("Device added: %s\n", device_file);
    run_command("post_insertion_command", device_file, NULL);

    g_free(device_file);
    g_object_unref(props_proxy);
}

void device_changed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    GSList *entry = g_slist_find_custom(pending_mount_devices, object_path, (GCompareFunc)strcmp);
    if (!entry) return;

    if (g_hash_table_lookup(mounted_devices, object_path)) return;

    DBusGProxy *props_proxy = dbus_g_proxy_new_for_name(dbus_conn, "org.freedesktop.UDisks", object_path, "org.freedesktop.DBus.Properties");
    RETURN_IF_NOT_PROPERTY("DeviceIsMounted");

    gchar *device_file = get_string_property(props_proxy, "DeviceFile");
    if (!device_file) return;

    gchar **mount_paths = get_stringv_property(props_proxy, "DeviceMountPaths");
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
    run_command("post_mount_command", device_file, *mount_paths);

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
    run_command("post_removal_command", device_file, NULL);

    g_hash_table_remove(mounted_devices, object_path);
}
