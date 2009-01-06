#include <stdio.h>
#include <math.h>

#include "util.h"
#include "lintok.h"

#if defined(LIBARRAY_NOT_INSTALLED)
typedef double *doublearray1d;
#define da1d_new(n) ((double *) malloc((n) * sizeof(double)))
#define da1d_delete(a) free(a)
#else
#include "array.h"
#endif

#include "wvlt.h"
