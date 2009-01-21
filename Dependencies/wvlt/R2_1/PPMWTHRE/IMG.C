#include "local.h"

/* PPM likes no more than about 70 characters per line */
#define MXN_PXL_PER_ROW 5

#if defined(ARCH_SUN4) && OSREL_MAJOR < 5
/* Why, oh, why aren't these declarations in stdio on SunOS 4 SPARCs????? */
extern rewind _PROTO((FILE *fp));
extern ungetc _PROTO((int ch, FILE *fp));
#endif

static int decimal_read _PROTO((FILE *fp));
static pixel pxlPpmAscii_read _PROTO((FILE *fp, int rgbMaxPpm));

/* decimal_read -- read an ASCII decimal number from a file */
static int decimal_read(fp)
	FILE *fp;
{
	int ch, val, state;

	state = 1;
	val = 0;
	for (;;) {
		ch = getc(fp);
		switch (state) {

		case 1:	/* normal */
			if (ch == '#')
				state = 2;
			else if (isascii(ch) && isdigit(ch)) {
				val = (ch - '0');
				state = 3;
			} else if (ch == EOF || !isascii(ch) || !isspace(ch))
				return EOF;
			break;

		case 2:	/* skip to end of line */
			if (ch == '\n')
				state = 1;
			else if (ch == EOF)
				return EOF;
			break;

		case 3:	/* accumulate and look for end of number */
			if (isascii(ch) && isdigit(ch))
				val = 10 * val + (ch - '0');
			else if (ch == EOF || (isascii(ch) && isspace(ch)))
				return val;
			else if (ch == '#')
				state = 4;
			break;

		case 4:	/* return value when end of line or file is reached */
			if (ch == EOF || ch == '\n')
				return val;
			break;

		default:
			NOT_REACHED;
		}
	}
}

/* img_delete -- delete a PPM image object */
void img_delete(img)
	image *img;
{
	assert(img != NULL);
	FREE_LINTOK(img->pxl[0]);
	FREE_LINTOK(img->pxl);
	img->pxl = NULL;
	return;
}

/* img_new -- create an image object */
image *img_new(npx, npy)
	int npx, npy;
{
	image *img;
	pixel *pxl;
	int py;

	/*
	 *	For convenience's sake, we adopt the pixel ordering used by SGI:
	 *	pxl[y][x].
	 */
	(void) MALLOC_LINTOK(img, 1, image);
	if (img == NULL)
		return NULL;
	img->npx = npx;
	img->npy = npy;
	if (MALLOC_LINTOK(img->pxl, npy, pixel *) == NULL) {
		FREE_LINTOK(img);
		return NULL;
	}
	if (MALLOC_LINTOK(pxl, npx * npy, pixel) == NULL) {
		FREE_LINTOK(img->pxl);
		FREE_LINTOK(img);
		return NULL;
	}
	for (py = 0; py < npy; py++)
		img->pxl[py] = &pxl[py * npx];
	return img;
}

/* img_read -- read an image file */
image *img_read(fp, whynot)
	FILE *fp;	/* in: open file (must be repositionable) */
	char **whynot;	/* out: (if not NULL) in case of error, set to reason why read failed */
{
	unsigned char *scnl;
	int px, npx, py, npy, zRes, rgbMaxPpm, iScnl, iRgb;
	int ch1, ch2, chR, chG, chB;
	int pxlR, pxlG, pxlB;
	image *img;
	register pixel pxl;
	bool isAscii;
	int _pxlval;
#define PXL_PPM_ASCII_READ(fp) \
	( (_pxlval = decimal_read(fp)) != EOF \
			&& 0 <= _pxlval && _pxlval <= rgbMaxPpm \
			? (_pxlval * PXL_RGBMAX) / rgbMaxPpm \
			: EOF )

	/*
	 *	Read the first two characters for magic cookies.
	 */
	if (fp == NULL) {
		if (whynot != NULL)
			*whynot = "NULL FILE pointer passed";
		return FALSE;
	}
	ch1 = getc(fp);
	ch2 = getc(fp);

	if (ch1 == 'P' && ch2 == '3')
		isAscii = TRUE;
	else if (ch1 == 'P' && ch2 == '6')
		isAscii = FALSE;
	else {
		if (whynot != NULL)
			*whynot = "unrecognized or unsupported file format";
		return NULL;
	}

	npx = decimal_read(fp);
	if (npx == EOF || npx < 1) {
		if (whynot != NULL)
			*whynot = "improperly coded image width";
		return NULL;
	}
	npy = decimal_read(fp);
	if (npy == EOF || npy < 1) {
		if (whynot != NULL)
			*whynot = "improperly coded image height";
		return NULL;
	}
	rgbMaxPpm = decimal_read(fp);
	if (rgbMaxPpm == EOF || rgbMaxPpm < 1) {
		if (whynot != NULL)
			*whynot = "improperly coded image max RGB value";
		return NULL;
	}

	img = img_new(npx, npy);
	if (img == NULL) {
		if (whynot != NULL)
			*whynot = "cannot allocate memory for image";
		return NULL;
	}

	if (isAscii) {
		for (py = npy - 1; py >= 0; py--) {
			for (px = 0; px < npx; px++) {
				pxlR = PXL_PPM_ASCII_READ(fp);
				pxlG = PXL_PPM_ASCII_READ(fp);
				pxlB = PXL_PPM_ASCII_READ(fp);
				if (pxlR == EOF || pxlG == EOF || pxlB == EOF) {
					img_delete(img);
					if (whynot != NULL)
						*whynot = "premature end-of-file reached";
					return NULL;
				}
				PXL_SET_RGB(IMG_PXL_AT_XY(img, px, py), pxlR, pxlG, pxlB);
			}
		}
	} else {
		for (py = npy - 1; py >= 0; py--) {
			for (px = 0; px < npx; px++) {
				chR = getc(fp);
				chG = getc(fp);
				chB = getc(fp);
				if (chR == EOF || chG == EOF || chB == EOF) {
					img_delete(img);
					if (whynot != NULL)
						*whynot = "premature end-of-file reached";
					return NULL;
				}
				pxlR = (chR * PXL_RGBMAX) / rgbMaxPpm;
				pxlG = (chG * PXL_RGBMAX) / rgbMaxPpm;
				pxlB = (chB * PXL_RGBMAX) / rgbMaxPpm;
				PXL_SET_RGB(IMG_PXL_AT_XY(img, px, py), pxlR, pxlG, pxlB);
			}
		}
	}
#undef PXL_PPM_ASCII_READ

	return img;
}


/* img_write -- write a img image to a file */
bool img_write(img, isAscii, fp)
	image *img;
	bool isAscii;
	FILE *fp;
{
	int px, py;
	int iScnl;
	unsigned char *scnl;


	assert(fp != NULL);

	if (isAscii) {
		(void) fprintf(fp, "P3\n");
		(void) fprintf(fp, "%d %d\n", img->npx, img->npy);
		(void) fprintf(fp, "%d\n", PXL_RGBMAX);
		for (py = img->npy - 1; py >= 0; py--) {
			(void) fprintf(fp, "# row %d:\n", py);
			for (px = 0; px < img->npx; px++) {
				(void) fprintf(fp,
						( (px + 1) % MXN_PXL_PER_ROW == 0
								|| px == img->npx - 1
							? "%d %d %d\n"
							: "%d %d %d  " ),
						R_OF_PXL(IMG_PXL_AT_XY(img, px, py)),
						G_OF_PXL(IMG_PXL_AT_XY(img, px, py)),
						B_OF_PXL(IMG_PXL_AT_XY(img, px, py)));
			}
		}
	} else {
		(void) fprintf(fp, "P6\n");
		(void) fprintf(fp, "%d %d\n", img->npx, img->npy);
		(void) fprintf(fp, "%d\n", PXL_RGBMAX);
		for (py = img->npy - 1; py >= 0; py--) {
			for (px = 0; px < img->npx; px++) {
				(void) putc(R_OF_PXL(IMG_PXL_AT_XY(img, px, py)), fp);
				(void) putc(G_OF_PXL(IMG_PXL_AT_XY(img, px, py)), fp);
				(void) putc(B_OF_PXL(IMG_PXL_AT_XY(img, px, py)), fp);
			}
		}
	}

	return TRUE;
}
