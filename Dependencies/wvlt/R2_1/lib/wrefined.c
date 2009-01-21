#include "local.h"

/*
 *	This file instantiates the waveform refinement template to work on double
 *	arrays.
 */

#define TYPE_ARRAY double
#define FUNC_1D wrefine_da1d
#define FUNC_ND wrefine_dand

#include "wrefine_t.c"
