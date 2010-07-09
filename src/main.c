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
#include <getopt.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "dbus_constants.h"
#include "filters.h"
#include "handlers.h"
#include "util.h"

DBusGConnection *dbus_conn = NULL;

static GMainLoop *loop = NULL;
static cfg_t *cfg = NULL;

static void signal_handler(int sig)
{
    if (loop)
        g_main_loop_quit(loop);
}

static void print_usage(FILE *out)
{
    fprintf(out, "\
Usage: \n\
    udisks-glue [--foreground] [--config file]\n\
    udisks-glue [--help]\n");
}

static int parse_config(int argc, char **argv, int *rc)
{
    struct option long_options[] = {
        { "config", required_argument, 0, 'c' },
        { "foreground", no_argument, 0, 'f' },
        { "help", no_argument, 0, 'h' },
        { NULL, 0, 0, 0 }
    };

    int do_daemonize = 1;
    const char *config_file = NULL;

    int opt, option_index = 0;
    while ((opt = getopt_long(argc, argv, "c:fh", long_options, &option_index)) != EOF) {
        switch ((char)opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'f':
                do_daemonize = 0;
                break;
            case 'h':
                print_usage(stdout);
                *rc = EXIT_SUCCESS;
                return 1;
            default:
                print_usage(stderr);
                *rc = EXIT_FAILURE;
                return 1;
        }
    }
 
    if (config_file == NULL) {
        if (file_exists("~/.config/udisks-glue/udisks-glue.conf")) {
            config_file = "~/.config/udisks-glue/udisks-glue.conf";
        }
        else if (file_exists("~/.udisks-glue.conf")) {
            config_file = "~/.udisks-glue.conf";
        }
        else if (file_exists(SYSCONFDIR "/udisks-glue.conf")) {
            config_file = SYSCONFDIR "/udisks-glue.conf";
        }
        else {
            fprintf(stderr, "Unable to find the configuration file\n");
            *rc = EXIT_FAILURE;
            return 1;
        }
    }

    cfg_opt_t match_opts[] = {
        CFG_STR("post_insertion_command", NULL, CFGF_NONE),
        CFG_STR("post_mount_command", NULL, CFGF_NONE),
        CFG_STR("post_removal_command", NULL, CFGF_NONE),
        CFG_END()
    };

    cfg_opt_t opts[] = {
        CFG_SEC("filter", filters_get_cfg_opts(), CFGF_MULTI | CFGF_TITLE),
        CFG_SEC("match", match_opts, CFGF_MULTI | CFGF_TITLE),
        CFG_SEC("default", match_opts, CFGF_NONE),
        CFG_END()
    };

    cfg = cfg_init(opts, CFGF_NONE);
    if (cfg_parse(cfg, config_file) == CFG_PARSE_ERROR) {
        *rc = EXIT_FAILURE;
        return 1;
    }

    if (do_daemonize)
        daemonize();

    return 0;
}

int main(int argc, char **argv)
{
    int rc;
    if (parse_config(argc, argv, &rc))
        return rc;

    if (!filters_init(cfg))
        return EXIT_FAILURE;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);

    rc = EXIT_FAILURE;
    GError *error = NULL;
    DBusGProxy *proxy = NULL;

    g_type_init();
    loop = g_main_loop_new(NULL, FALSE);

    dbus_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
    if (!dbus_conn) {
        g_printerr("Unable to connect to the system bus: %s\n", error->message);
        goto cleanup;
    }

    proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, DBUS_OBJECT_PATH_UDISKS_ROOT, DBUS_INTERFACE_UDISKS);
    handlers_init(proxy);

    dbus_g_proxy_add_signal(proxy, "DeviceAdded", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
    dbus_g_proxy_add_signal(proxy, "DeviceChanged", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
    dbus_g_proxy_add_signal(proxy, "DeviceRemoved", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(proxy, "DeviceAdded", G_CALLBACK(device_added_signal_handler), NULL, NULL);
    dbus_g_proxy_connect_signal(proxy, "DeviceChanged", G_CALLBACK(device_changed_signal_handler), NULL, NULL);
    dbus_g_proxy_connect_signal(proxy, "DeviceRemoved", G_CALLBACK(device_removed_signal_handler), NULL, NULL);

    g_main_loop_run(loop);
    rc = EXIT_SUCCESS;

cleanup:
    if (cfg) cfg_free(cfg);
    if (error) g_error_free(error);
    if (proxy) g_object_unref(proxy);
    if (dbus_conn) dbus_g_connection_unref(dbus_conn);
    g_main_loop_unref(loop);
    handlers_free();
    filters_free();
    return rc;
}
