/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 * © 2010 Jan Palus
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef UTIL_H
#define UTIL_H

#include <glib.h>

gchar *str_replace(gchar *string, gchar *search, gchar *replacement);
void run_command(const char *command);
void daemonize(void);
void close_descriptors(void);
const char *find_config_file(void);

#endif
