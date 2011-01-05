/*
 * This file is part of udisks-glue.
 *
 * © 2011 Jan Palus
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <glib.h>
#include <string.h>

#include "dbus_constants.h"
#include "globals.h"

static char *session_obj_path = NULL;
static DBusGProxy *proxy;

static char* get_session_obj_path(void);
static char* get_session_seat_obj_path(const char *session_id);
static void session_removed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data);

int session_init(void)
{
    char *seat_obj_path;

    session_obj_path = get_session_obj_path();

    if (!session_obj_path)
        return 0;

    seat_obj_path = get_session_seat_obj_path(session_obj_path);

    if (!seat_obj_path)
        return 0;

    g_print("Running within session context: %s\n", session_obj_path);

    proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_CK, seat_obj_path, DBUS_INTERFACE_CK_SEAT);
    dbus_g_proxy_add_signal(proxy, "SessionRemoved", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(proxy, "SessionRemoved", G_CALLBACK(session_removed_signal_handler), NULL, NULL);

    g_free(seat_obj_path);

    return 1;
}

void session_free(void)
{
    g_free(session_obj_path);
    g_object_unref(proxy);
}

static char* get_session_obj_path(void)
{
    GError *error = NULL;
    char *session_id;
    DBusGProxy *proxy;
    gboolean res; 
    
    proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_CK, DBUS_OBJECT_PATH_CK_MANAGER, DBUS_INTERFACE_CK_MANAGER);
    res = dbus_g_proxy_call(proxy, "GetCurrentSession", &error,
            G_TYPE_INVALID,
            DBUS_TYPE_G_OBJECT_PATH,
            &session_id,
            G_TYPE_INVALID);
    if (!res) {
        g_printerr("Unable to get current session: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }

    return session_id;
}

static char* get_session_seat_obj_path(const char *session_id)
{
    GError *error = NULL;
    char *seat_id;
    DBusGProxy *proxy;
    gboolean res;

    proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_CK, session_id, DBUS_INTERFACE_CK_SESSION);
    res = dbus_g_proxy_call(proxy, "GetSeatId", &error,
            G_TYPE_INVALID,
            DBUS_TYPE_G_OBJECT_PATH,
            &seat_id,
            G_TYPE_INVALID);
    if (!res) {
        g_printerr("Unable to get session seat: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }

    return seat_id;
}

static void session_removed_signal_handler(DBusGProxy *proxy, const char *object_path, gpointer user_data)
{
    if (!strcmp(session_obj_path, object_path)) {
        g_print("Session ended, exiting...\n");
        g_main_loop_quit(loop);
    }
}
