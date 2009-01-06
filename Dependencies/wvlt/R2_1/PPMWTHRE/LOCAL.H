#include <stdio.h>
#include <math.h>
#ifdef ARCH_DOSTC
#include <ctype.h>
#endif
#include "util.h"
#include "lintok.h"
#include "wvlt.h"

/*
 *	The definitions that follow are intended to be compatible with the SGI
 *	definitions, so change them at your peril!
 */
typedef unsigned long pixel;
#define R_OF_PXL(pxl) ((int) ((pxl) & 0xff))
#define G_OF_PXL(pxl) ((int) (((pxl) >> 8) & 0xff))
#define B_OF_PXL(pxl) ((int) (((pxl) >> 16) & 0xff))
#define A_OF_PXL(pxl) ((int) (((pxl) >> 24) & 0xff))
#define ALPHA_OPAQUE ((pixel) 0xff)
/* we generally assume pixels set this way are opaque */
#define PXL_SET_RGB(pxl, r, g, b) \
	{ (pxl) = (ALPHA_OPAQUE << 24) | (((b) & 0xff) << 16) | (((g) & 0xff) << 8) | ((r) & 0xff); }
#define PXL_RGBMAX 255

typedef struct _image {
	int npx, npy;
	pixel **pxl;	/* 2-d array of pixels representing image */
} image;

/*
 *	Although pixels are usually accessed by (x, y), for compatibility with SGI
 *	they must be stored [y][x].  For convenience, then, always access them with
 *	these macros.
 */
#define PXL_AT_XY(pxl, x, y) ((pxl)[y][x])
#define IMG_PXL_AT_XY(img, x, y) PXL_AT_XY((img)->pxl, x, y)

/* in "img" */
extern void img_delete _PROTO((image *img));
extern image *img_new _PROTO((int npx, int npy));
extern image *img_read _PROTO((FILE *fp, char **whynot));
extern bool img_write _PROTO((image *img, bool isAscii, FILE *fp));
