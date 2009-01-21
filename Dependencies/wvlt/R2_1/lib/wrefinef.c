#include "local.h"

/*
 *	This file instantiates the waveform refinement template to work on float
 *	arrays.
 */

#define TYPE_ARRAY float
#define FUNC_1D wrefine_fa1d
#define FUNC_ND wrefine_fand

#include "wrefine_t.c"
