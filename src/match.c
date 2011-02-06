/*
 * This file is part of udisks-glue.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <confuse.h>
#include <glib.h>

#include "filter.h"
#include "match.h"

struct match_ {
    filter *filter_obj;
    int automount;
    gchar *automount_filesystem;
    gchar **automount_options;
    char *post_insertion_command;
    char *post_mount_command;
    char *post_unmount_command;
    char *post_removal_command;
};

match *match_create(cfg_t *sec, filter *f)
{
    match *m = g_malloc0(sizeof(match));
    m->filter_obj = f;

    m->automount = cfg_getbool(sec, "automount") ? 1 : 0;
    if (cfg_size(sec, "automount_filesystem"))
        m->automount_filesystem = g_strdup(cfg_getstr(sec, "automount_filesystem"));
    int num_automount_options = cfg_size(sec, "automount_options");
    if (num_automount_options) {
        m->automount_options = g_malloc0(sizeof(gchar *) * (num_automount_options + 1));
        for (int i = 0; i < num_automount_options; ++i)
            m->automount_options[i] = g_strdup(cfg_getnstr(sec, "automount_options", i));
    }

    if (cfg_size(sec, "post_insertion_command"))
        m->post_insertion_command = cfg_getstr(sec, "post_insertion_command");
    if (cfg_size(sec, "post_mount_command"))
        m->post_mount_command = cfg_getstr(sec, "post_mount_command");
    if (cfg_size(sec, "post_unmount_command"))
        m->post_unmount_command = cfg_getstr(sec, "post_unmount_command");
    if (cfg_size(sec, "post_removal_command"))
        m->post_removal_command = cfg_getstr(sec, "post_removal_command");

    return m;
}

void match_free(match *m)
{
    if (m->automount_options)
        g_strfreev(m->automount_options);
    if (m->automount_filesystem)
        g_free(m->automount_filesystem);
    g_free(m);
}

cfg_opt_t *match_get_cfg_opts()
{
    static cfg_opt_t opts[] = {
        CFG_BOOL("automount", cfg_false, CFGF_NONE),
        CFG_STR("automount_filesystem", NULL, CFGF_NODEFAULT),
        CFG_STR_LIST("automount_options", NULL, CFGF_NODEFAULT),
        CFG_STR("post_insertion_command", NULL, CFGF_NODEFAULT),
        CFG_STR("post_mount_command", NULL, CFGF_NODEFAULT),
        CFG_STR("post_unmount_command", NULL, CFGF_NODEFAULT),
        CFG_STR("post_removal_command", NULL, CFGF_NODEFAULT),
        CFG_END()
    };
    return opts;
}

void match_free_cfg_opts(cfg_opt_t *opts)
{
}

int match_matches(match *m, DBusGProxy *proxy, property_cache *cache)
{
    return m->filter_obj ? filter_matches(m->filter_obj, proxy, cache) : 1;
}

const char *match_get_post_insertion_command(match *m)
{
    return m->post_insertion_command;
}

const char *match_get_post_mount_command(match *m)
{
    return m->post_mount_command;
}

const char *match_get_post_unmount_command(match *m)
{
    return m->post_unmount_command;
}

const char *match_get_post_removal_command(match *m)
{
    return m->post_removal_command;
}

int match_get_automount(match *m)
{
    return m->automount;
}

gchar *match_get_automount_filesystem(match *m)
{
    return m->automount_filesystem;
}

gchar **match_get_automount_options(match *m)
{
    return m->automount_options;
}
