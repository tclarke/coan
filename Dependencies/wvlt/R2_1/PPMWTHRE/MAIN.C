#include "local.h"

static char *progname = NULL;

static enum {
	FTYP_BATTLE,
	FTYP_BURT,
	FTYP_COIFLET,
	FTYP_DAUB,
	FTYP_HAAR,
	FTYP_PSCOIFLET,
	FTYP_SPLINE
} ftyp = FTYP_HAAR;

static int orderCoif = 2;
static int orderDaub = 4;
static char orderSpline[2] = "";
static bool overrideOffsets = FALSE;
static int offH = 0;
static int offG = 0;
static int offHtilde = 0;
static int offGtilde = 0;
static bool exchangeCoeffs = FALSE;
static bool isStd = TRUE;

static double thresh = 0.0;
static double fComp = 0.0;
static bool isVerbose = FALSE;

static void ppmwthresh _PROTO((char *fnImg, FILE *fpImg));
static double thresh_apply _PROTO((double thresh, float *y, int ny,
		int *nBelow));
static void usage _PROTO((void));
static waveletfilter *wfltr_select _PROTO((void));

void main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	extern char *optarg;
	extern int optind;
	bool ok = TRUE;
	char *pOptarg;
	FILE *fpImg;
	char *fnImg;
	double rComp;

	progname = argv[0];
	while ((ch = getopt(argc, argv, "ABC:D:ef:Hno:Pr:S:t:v?")) != -1) {
		switch (ch) {

		case 'A':
			ftyp = FTYP_BURT;
			break;

		case 'B':
			ftyp = FTYP_BATTLE;
			break;

		case 'C':
			ftyp = FTYP_COIFLET;
			orderCoif = atoi(optarg);
			if (orderCoif != 2 && orderCoif != 4 && orderCoif != 6)
				ok = FALSE;
			break;

		case 'D':
			ftyp = FTYP_DAUB;
			orderDaub = atoi(optarg);
			if (orderDaub != 4 && orderDaub != 6 && orderDaub != 8
					&& orderDaub != 10 && orderDaub != 12 && orderDaub != 20)
				ok = FALSE;
			break;

		case 'e':
			exchangeCoeffs = TRUE;
			break;

		case 'f':
			fComp = atof(optarg);
			if (fComp < 0.0 || 1.0 < fComp)
				ok = FALSE;
			break;

		case 'H':
			ftyp = FTYP_HAAR;
			break;

		case 'n':
			isStd = FALSE;
			break;

		case 'o':
			pOptarg = optarg;
			overrideOffsets = TRUE;
			offH = strtol(pOptarg, &pOptarg, 10);
			if (pOptarg == optarg)
				ok = FALSE;
			else {
				/* mirrored coefficients default to the same offsets */
				if (*pOptarg == ',') {
					pOptarg++;
					offG = strtol(pOptarg, &pOptarg, 10);
					if (*pOptarg == ',') {
						pOptarg++;
						offHtilde = strtol(pOptarg, &pOptarg, 10);
						if (*pOptarg == ',') {
							pOptarg++;
							offGtilde = strtol(pOptarg, &pOptarg, 10);
						} else
							offGtilde = offH;
					} else {
						offHtilde = offG;
						offGtilde = offH;
					}
				} else
					offGtilde = offHtilde = offG = offH;
				if (*pOptarg != '\0')
					ok = FALSE;
			}
			break;

		case 'P':
			ftyp = FTYP_PSCOIFLET;
			break;

		case 'r':
			rComp = atof(optarg);
			if (rComp < 1.0)
				ok = FALSE;
			else {
				fComp = 1.0 - 1/rComp;
			}
			break;

		case 'S':
			ftyp = FTYP_SPLINE;
			if (strlen(optarg) != 3)
				ok = FALSE;
			else {
				strcpy(orderSpline, optarg);
				if (!STR_EQ(orderSpline, "2,2")
						&& !STR_EQ(orderSpline, "2,4")
						&& !STR_EQ(orderSpline, "3,3")
						&& !STR_EQ(orderSpline, "3,7"))
					ok = FALSE;
			}
			break;

		case 't':
			thresh = atof(optarg);
			if (thresh < 0.0)
				ok = FALSE;
			break;

		case 'v':
			isVerbose = TRUE;
			break;

		case '?':
			usage();
			exit(0);
		}
	}

	if (!ok) {
		usage();
		exit(1);
	}
	if (optind == argc - 1) {
		fnImg = argv[optind];
		fpImg = fopen(fnImg, "r");
		if (fpImg == NULL) {
			(void) fprintf(stderr, "can\'t open \"%s\" for reading\n", fnImg);
			exit(1);
		}
		ppmwthresh(fnImg, fpImg);
		fclose(fpImg);
	} else if (optind == argc) {
		ppmwthresh("standard input", stdin);
	} else {
		usage();
		exit(1);
	}
	exit(0);
}

static void ppmwthresh(fnImg, fpImg)
	char *fnImg;
	FILE *fpImg;
{
#define ABS(val) ( (val) >= 0 ? (val) : -(val) )
	char *whynot;
	int px, py;
	pixel pxl;
	image *img;
	int n[2];
	float *p;
	int ipxl, npxl;
	int lg2npx, lg2npy;
	int pxlR, pxlG, pxlB;
	int nBelow0, nAbove255, nBelowThresh;
	double err;
	waveletfilter *wfltr = NULL;
	int iq, nq, lower, upper;
	float *q, qSwap, qAbsMin;

	wfltr = wfltr_select();

	img = img_read(fpImg, &whynot);
	if (img == NULL) {
		(void) fprintf(stderr,
				"can\'t read image from \"%s\"\n  %s -- exiting\n",
				fnImg, whynot);
		exit(1);
	}

	for (lg2npx = 0; (1 << lg2npx) < img->npx; lg2npx++)
		continue;
	for (lg2npy = 0; (1 << lg2npy) < img->npy; lg2npy++)
		continue;
	if (img->npx != (1 << lg2npx) || img->npy != (1 << lg2npy)) {
		(void) fprintf(stderr,
				"dimensions of image must be powers of two -- exiting\n");
		exit(1);
	}

	n[0] = img->npy;
	n[1] = img->npx;
	npxl = img->npx * img->npy;
	MALLOC_OR_ELSE_LINTOK(p, 3 * npxl, float);

	ipxl = 0;
	for (py = 0; py < img->npy; py++) {
		for (px = 0; px < img->npx; px++) {
			pxl = IMG_PXL_AT_XY(img, px, py);
			p[ipxl] = R_OF_PXL(pxl);
			p[ipxl + npxl] = G_OF_PXL(pxl);
			p[ipxl + 2*npxl] = B_OF_PXL(pxl);
			ipxl++;
		}
	}

	err = 0.0;

	wxfrm_fand(&p[0], n, 2, TRUE, isStd, wfltr, &p[0]);
	wxfrm_fand(&p[npxl], n, 2, TRUE, isStd, wfltr, &p[npxl]);
	wxfrm_fand(&p[2*npxl], n, 2, TRUE, isStd, wfltr, &p[2*npxl]);

	/*
	 *	It may be that the user has specified *both* a threshold and a percent
	 *	compression (or compression ratio).  In such cases, we apply both
	 *	criteria.
	 */
	err = thresh_apply(thresh, &p[0], 3 * npxl, &nBelowThresh);

	/*
	 *	To apply fractional compression, we determine the equivalent threshold
	 *	by using a heap.  Then we threshold the original data with that value.
	 */
	if (fComp > 0.0) {
		nq = ceil((1 - fComp) * 3 * npxl);
		if (nq == 0)
			err += thresh_apply(MAXFLOAT, &p[0], 3 * npxl, &nBelowThresh);
		else {
			MALLOC_OR_ELSE_LINTOK(q, nq, float);
			for (ipxl = 0; ipxl < 3 * npxl; ipxl++) {
				/*
				 *	Let the heap grow until there are nq elements in it.
				 */
				if (ipxl < nq) {
					lower = ipxl;
					q[lower] = ABS(p[ipxl]);
					upper = (lower - 1) / 2;
					while (lower != 0 && q[upper] > q[lower]) {
						qSwap = q[upper]; q[upper] = q[lower]; q[lower] = qSwap;
						lower = upper;
						upper = (lower - 1) / 2;
					}
				} else if (ABS(p[ipxl]) > q[0]) {
					/*
					 *	There are now nq elements in the heap.  q[0] is the
					 *	smallest.  If the new value (ABS(p[ipxl])) is less than or
					 *	equal to q[0], we can ignore it.  Otherwise, we replace
					 *	q[0] with the new value and sift downwards.
					 */
					q[0] = ABS(p[ipxl]);
					upper = 0;
					lower = 2 * upper + 1;
					while (lower < nq) {
						if (lower + 1 < nq && q[lower+1] < q[lower])
							lower++;
						/*
						 *	If q[upper] is smaller than either of its one or two
						 *	children, swap and continue down.
						 */
						if (q[upper] < q[lower])
							break;
						qSwap = q[upper]; q[upper] = q[lower]; q[lower] = qSwap;
						upper = lower;
						lower = 2 * upper + 1;
					}
				}
			}
			/*
			 *	q[0] is now the smallest value in the heap.  We can use it as
			 *	a threshold value.
			 */
			if (q[0] > thresh) {
				thresh = q[0];
				err += thresh_apply(thresh, &p[0], 3 * npxl, &nBelowThresh);
			}
			FREE_LINTOK(q);
		}
	}

	/*
	 *	Invert the transform.
	 */
	wxfrm_fand(&p[0], n, 2, FALSE, isStd, wfltr, &p[0]);
	wxfrm_fand(&p[npxl], n, 2, FALSE, isStd, wfltr, &p[npxl]);
	wxfrm_fand(&p[2*npxl], n, 2, FALSE, isStd, wfltr, &p[2*npxl]);

	/*
	 *	Put the results back in the image.
	 */
	ipxl = 0;
	nBelow0 = 0;
	nAbove255 = 0;
	for (py = 0; py < img->npy; py++) {
		for (px = 0; px < img->npx; px++) {
			pxlR = floor(p[ipxl] + 0.5);
			if (pxlR < 0) {
				nBelow0++;
				pxlR = 0;
			} else if (pxlR > 255) {
				nAbove255++;
				pxlR = 255;
			}
			pxlG = floor(p[ipxl + npxl] + 0.5);
			if (pxlG < 0) {
				nBelow0++;
				pxlG = 0;
			} else if (pxlG > 255) {
				nAbove255++;
				pxlG = 255;
			}
			pxlB = floor(p[ipxl + 2*npxl] + 0.5);
			if (pxlB < 0) {
				nBelow0++;
				pxlB = 0;
			} else if (pxlB > 255) {
				nAbove255++;
				pxlB = 255;
			}
			PXL_SET_RGB(IMG_PXL_AT_XY(img, px, py), pxlR, pxlG, pxlB);
			ipxl++;
		}
	}

	img_write(img, FALSE, stdout);

	if (isVerbose) {
		(void) fprintf(stderr, "%10.1f = threshold\n",
				thresh);
		(void) fprintf(stderr, "%10d pixel values below threshold\n",
				nBelowThresh);
		fComp = RATIO(nBelowThresh, 3 * npxl);
		(void) fprintf(stderr, "%10.6f compression (0 = no compression)\n",
				fComp);
		if (fComp < 1.0) {
			(void) fprintf(stderr, "%8.1f:1 compression ratio\n",
					1.0 / (1.0 - fComp));
		} else
			(void) fprintf(stderr, "     INF:1 compression ratio\n");
		(void) fprintf(stderr, "%10.2f = RMS error\n",
				sqrt(err / (3 * npxl)));
		(void) fprintf(stderr, "%10d reconstructed pixel values below 0\n",
				nBelow0);
		(void) fprintf(stderr, "%10d reconstructed pixel values above 255\n",
				nAbove255);
	}

	FREE_LINTOK(p);
	img_delete(img);

	return;
}

/* thresh_apply -- apply a magnitude threshold to an array of values */
static double thresh_apply(thresh, y, n, nBelow)
	double thresh;
	float *y;
	int n;
	int (*nBelow);
{
	double err = 0.0;
	int i;
	double yval;

	(*nBelow) = 0;
	if (thresh > 0.0) {
		for (i = 0; i < n; i++) {
			yval = y[i];
			if (-thresh < yval && yval < thresh) {
				err += yval*yval;
				y[i] = 0.0;
				(*nBelow)++;
			}
		}
	}
	return err;
}

/* usage -- issue a usage error message */
static void usage()
{
	(void) fprintf(stderr,
			"usage: %s [{args}] [{PPM image file}]\n", progname);
	(void) fprintf(stderr, "%s\n",
			" {args} are:");
	(void) fprintf(stderr, "%s\n",
			"  -A       use Burt-Adelson basis");
	(void) fprintf(stderr, "%s\n",
			"  -B       use Battle-Lemarie basis");
	(void) fprintf(stderr, "%s\n",
			"  -C {#}   use Coiflet order {#} (= 2, 4, or 6) basis");
	(void) fprintf(stderr, "%s\n",
			"  -D {#}   use Daubechies order {#} (= 4, 6, 8, 10, 12, or 20) basis");
	(void) fprintf(stderr, "%s\n",
			"  -e       exchange normal and tilde components");
	(void) fprintf(stderr, "%s\n",
			"  -f {#}   compress to fraction {#} of original data size (default: 1.0)");
	(void) fprintf(stderr, "%s\n",
			"  -H       use Haar basis (default)");
	(void) fprintf(stderr, "%s\n",
			"  -n       use non-standard multidimensional basis");
	(void) fprintf(stderr, "%s\n",
			"  -o {#,#[,#,#]} override H, G, and (for biorthogonal wavelets)");
	(void) fprintf(stderr, "%s\n",
			"             H~ and G~ offsets with {#}'s");
	(void) fprintf(stderr, "%s\n",
			"  -P       use Pseudocoiflet (4, 4) basis");
	(void) fprintf(stderr, "%s\n",
			"  -r {#}   apply compression ratio {#}:1 to wavelet coefficients (default: none)");
	(void) fprintf(stderr, "%s\n",
			"  -S {#}   use Spline order {#} (= \"2,2\", \"2,4\", \"3,3\", or \"3,7\")");
	(void) fprintf(stderr, "%s\n",
			"  -t {#}   apply threshold {#} to wavelet coefficients (default: no threshold)");
	(void) fprintf(stderr, "%s\n",
			"  -v       verbosely display compression statistics on stderr");
	(void) fprintf(stderr, "%s\n",
			"  -?       this message");
	return;
}

/* wfltr_select -- select the wavelet filter being used */
static waveletfilter *wfltr_select()
{
	waveletfilter *wfltr = NULL;

	switch (ftyp) {

	case FTYP_BATTLE:
		wfltr = &wfltrBattleLemarie;
		break;

	case FTYP_BURT:
		wfltr = &wfltrBurtAdelson;
		break;

	case FTYP_COIFLET:
		switch (orderCoif) {

		case 2:
			wfltr = &wfltrCoiflet_2;
			break;

		case 4:
			wfltr = &wfltrCoiflet_4;
			break;

		case 6:
			wfltr = &wfltrCoiflet_6;
			break;

		default:
			NOT_REACHED;
		}
		break;

	case FTYP_DAUB:
		switch (orderDaub) {

		case 4:
			wfltr = &wfltrDaubechies_4;
			break;

		case 6:
			wfltr = &wfltrDaubechies_6;
			break;

		case 8:
			wfltr = &wfltrDaubechies_8;
			break;

		case 10:
			wfltr = &wfltrDaubechies_10;
			break;

		case 12:
			wfltr = &wfltrDaubechies_12;
			break;

		case 20:
			wfltr = &wfltrDaubechies_20;
			break;

		default:
			NOT_REACHED;
		}
		break;

	case FTYP_HAAR:
		wfltr = &wfltrHaar;
		break;

	case FTYP_PSCOIFLET:
		wfltr = &wfltrPseudocoiflet_4_4;
		break;

	case FTYP_SPLINE:
		if (STR_EQ(orderSpline, "2,2"))
			wfltr = &wfltrSpline_2_2;
		else if (STR_EQ(orderSpline, "2,4"))
			wfltr = &wfltrSpline_2_4;
		else if (STR_EQ(orderSpline, "3,3"))
			wfltr = &wfltrSpline_3_3;
		else {
			assert(STR_EQ(orderSpline, "3,7"));
			wfltr = &wfltrSpline_3_7;
		}
		break;
	}
	if (wfltr == NULL) {
		(void) fprintf(stderr,
				"requested filter not yet supported -- exiting\n");
		exit(1);
	}

	/* exchange normal and tilde components, if requested */
	if (exchangeCoeffs)
		wfltr_exchange(wfltr, wfltr);

	/* override offsets, if requested */
	if (overrideOffsets) {
		wfltr->offH = offH;
		wfltr->offG = offG;
		wfltr->offHtilde = offHtilde;
		wfltr->offGtilde = offGtilde;
	}

	return wfltr;
}
