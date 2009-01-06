#include "local.h"

/*
 *	This file instantiates the waveform transform template to work on float
 *	arrays.
 */

#define TYPE_ARRAY float
#define FUNC_1D wxfrm_fa1d
#define FUNC_ND wxfrm_fand

#include "wxfrm_t.c"
