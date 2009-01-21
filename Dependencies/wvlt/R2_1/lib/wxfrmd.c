#include "local.h"

/*
 *	This file instantiates the waveform transform template to work on double
 *	arrays.
 */

#define TYPE_ARRAY double
#define FUNC_1D wxfrm_da1d
#define FUNC_ND wxfrm_dand

#include "wxfrm_t.c"
