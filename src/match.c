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
    char *post_insertion_command;
    char *post_mount_command;
    char *post_unmount_command;
    char *post_removal_command;
};

match *match_create(cfg_t *sec, filter *f)
{
    match *m = g_malloc0(sizeof(match));
    m->filter_obj = f;

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
    g_free(m);
}

cfg_opt_t *match_get_cfg_opts()
{
    static cfg_opt_t opts[] = {
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
