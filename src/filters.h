/*
 * This file is part of udisks-glue.
 *
 * © 2010-2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef FILTERS_H
#define FILTERS_H

#include <confuse.h>

#include "filter.h"

int filters_init(cfg_t *cfg);
void filters_free(void);

cfg_opt_t *filters_get_cfg_opts(void);
void filters_free_cfg_opts(cfg_opt_t *opts);

filter *filters_find_filter_by_name(const char *name);

#endif
