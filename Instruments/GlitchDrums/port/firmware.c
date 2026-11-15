/* SPDX-License-Identifier: GPL-3.0-only */
#include "names.h"
#include "../upstream/src/main.c"

int gdp_request_save(void){pending_save=1;return gd_flush_pending_save();}
