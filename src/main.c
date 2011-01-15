/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 * © 2011 Jan Palus
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <dbus/dbus-glib.h>
#include <sys/types.h>
#include <confuse.h>
#include <getopt.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dbus_constants.h"
#include "filters.h"
#include "handlers.h"
#include "match.h"
#include "matches.h"
#include "session.h"
#include "util.h"

DBusGConnection *dbus_conn = NULL;
GMainLoop *loop = NULL;

static cfg_t *cfg = NULL;
static FILE *fpidfile = NULL;
static int enable_session = 0;

static void signal_handler(int sig)
{
    if (loop)
        g_main_loop_quit(loop);
}

static void print_usage(FILE *out)
{
    fprintf(out, "\
Usage: \n\
    udisks-glue [--config file] [--foreground] [--pidfile pidfile] [--session]\n\
    udisks-glue --help\n");
}

static int parse_config(int argc, char **argv, int *rc)
{
    struct option long_options[] = {
        { "config", required_argument, 0, 'c' },
        { "foreground", no_argument, 0, 'f' },
        { "help", no_argument, 0, 'h' },
        { "pidfile", required_argument, 0, 'p' },
        { "session", no_argument, 0, 's' },
        { NULL, 0, 0, 0 }
    };

    int do_daemonize = 1;
    const char *config_file = NULL;
    const char *pidfile = NULL;

    int opt, option_index = 0;
    while ((opt = getopt_long(argc, argv, "c:fhp:s", long_options, &option_index)) != EOF) {
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
            case 'p':
                pidfile = optarg;
                break;
            case 's':
                enable_session = 1;
                break;
            default:
                print_usage(stderr);
                return 1;
        }
    }

    int config_file_was_allocated = 0;
    if (config_file == NULL) {
        config_file = find_config_file();
        config_file_was_allocated = 1;
        if (!config_file) {
            fprintf(stderr, "Unable to find the configuration file\n");
            return 1;
        }
    }

    cfg_opt_t *match_opts = match_get_cfg_opts();
    cfg_opt_t *filter_opts = filters_get_cfg_opts();

    cfg_opt_t opts[] = {
        CFG_SEC("filter", filter_opts, CFGF_MULTI | CFGF_TITLE),
        CFG_SEC("match", match_opts, CFGF_MULTI | CFGF_TITLE),
        CFG_SEC("default", match_opts, CFGF_NONE),
        CFG_END()
    };

    cfg = cfg_init(opts, CFGF_NONE);
    match_free_cfg_opts(match_opts);
    filters_free_cfg_opts(filter_opts);

    int res = cfg_parse(cfg, config_file);
    if (config_file_was_allocated)
        free((char *)config_file);
    if (res == CFG_PARSE_ERROR)
        return 1;

    if (do_daemonize)
        daemonize();

    if (pidfile) {
        fpidfile = fopen(pidfile, "w");

        if (!fpidfile) {
            fprintf(stderr, "Unable to open pidfile\n");
            exit(EXIT_FAILURE);
        }

        if (lockf(fileno(fpidfile), F_TLOCK, 0) == -1) {
            fclose(fpidfile);
            fprintf(stderr, "Another udisks-glue instance is running\n");
            exit(EXIT_FAILURE);
        }

        fprintf(fpidfile, "%d\n", getpid());
    }

    if (do_daemonize)
        close_descriptors();

    return 0;
}

int main(int argc, char **argv)
{
    GError *error = NULL;
    DBusGProxy *proxy = NULL;

    int rc = EXIT_FAILURE;
    if (parse_config(argc, argv, &rc))
        goto cleanup;

    if (!filters_init(cfg))
        goto cleanup;

    if (!matches_init(cfg))
        goto cleanup;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);

    g_type_init();
    loop = g_main_loop_new(NULL, FALSE);

    dbus_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
    if (!dbus_conn) {
        g_printerr("Unable to connect to the system bus: %s\n", error->message);
        goto cleanup;
    }

    if (enable_session) {
        if (!session_init())
            goto cleanup;
    }

    proxy = dbus_g_proxy_new_for_name(dbus_conn, DBUS_COMMON_NAME_UDISKS, DBUS_OBJECT_PATH_UDISKS_ROOT, DBUS_INTERFACE_UDISKS);
    if (!handlers_init(proxy))
        goto cleanup;

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
    if (loop) g_main_loop_unref(loop);
    if (fpidfile) fclose(fpidfile);
    if (enable_session) session_free();
    handlers_free();
    matches_free();
    filters_free();
    return rc;
}
