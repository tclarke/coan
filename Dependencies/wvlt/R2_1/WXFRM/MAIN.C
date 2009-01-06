#include "local.h"

#if defined(ARCH_SUN4) && OSREL_MAJOR >= 5
static int dbl_revCmp _PROTO((const void *d1, const void *d2));
#else
static int dbl_revCmp _PROTO((genericptr d1, genericptr d2));
#endif
static void doit _PROTO((void));
static void usage _PROTO((void));

static char *progname = NULL;

static FILE *fpIn = NULL;
static bool isFwd = TRUE;
static bool magsort = FALSE;
static bool oneColumn = FALSE;
static int n = -1;
static int ndim = -1;

static enum {
	OFMT_GNUPLOT, OFMT_NORMAL, OFMT_XGRAPH
} ofmt = OFMT_NORMAL;

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

#define MXN_DIM 10	/* maximum dimensionality of data */
static int nOfDim[MXN_DIM];

void main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	extern char *optarg;
	extern int optind;
	bool ok = TRUE;
	char *pOptarg;

	progname = argv[0];
	while ((ch = getopt(argc, argv, "1ABC:D:egHim:no:PsS:x?")) != -1) {
		switch (ch) {

		case '1':
			oneColumn = TRUE;
			break;

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

		case 'g':
			ofmt = OFMT_GNUPLOT;
			break;

		case 'H':
			ftyp = FTYP_HAAR;
			break;

		case 'i':
			isFwd = FALSE;
			break;

		case 'm':
			ndim = 0;
			pOptarg = optarg;
			n = 1;
			while (*pOptarg != '\0') {
				if (ndim >= MXN_DIM) {
					ok = FALSE;
					break;
				}
				nOfDim[ndim] = strtol(pOptarg, &pOptarg, 10);
				if (*pOptarg == ',')
					pOptarg++;
				else if (*pOptarg != '\0') {
					ok = FALSE;
					break;
				}
				n *= nOfDim[ndim];
				ndim++;
			}
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

		case 's':
			magsort = TRUE;
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

		case 'x':
			ofmt = OFMT_XGRAPH;
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
	if (optind == argc) {
		fpIn = stdin;
		doit();
	} else {
		for (; optind < argc; optind++) {
			fpIn = fopen(argv[optind], "r");
			if (fpIn == NULL) {
				(void) fprintf(stderr, "can\'t open \"%s\" for reading\n",
						argv[optind]);
				exit(1);
			} else {
				doit();
				fclose(fpIn);
			}
		}
	}
	exit(0);
}

/* dbl_revCmp -- compare two doubles in decreasing order */
static int dbl_revCmp(d1, d2)
#if defined(ARCH_SUN4) && OSREL_MAJOR >= 5
	const void *d1, *d2;
#else
	genericptr d1, d2;
#endif
{
	double diff = *((double *) d1) - *((double *) d2);

	if (diff < 0.0)
		return 1;
	else if (diff > 0.0)
		return -1;
	else
		return 0;
}

static void doit()
{
	int i, lgN;
	double *x, *y, xNew, yMax;
	double yPrev = 0.0;
	char type[20];
	char chComment;
	waveletfilter *wfltr = NULL;
	double tolGraph;

	if (n <= 1) {
		x = NULL;
		n = 0;
		while (fscanf(fpIn, "%lf", &xNew) == 1) {
			VOID_MR_ALLOC_LINTOK(x, n+1, double);
			x[n++] = xNew;
		}
	} else {
		/* this is in the multidimensional case */
		(void) MALLOC_LINTOK(x, n, double);
		for (i = 0; i < n; i++) {
			if (fscanf(fpIn, "%lf", &x[i]) != 1) {
				(void) fprintf(stderr, "unexpected EOF -- exiting\n");
				exit(1);
			}
		}
	}	/* count values */

	for (lgN = 0; (1 << lgN) < n; lgN++)
		continue;
	if ((1 << lgN) != n) {
		(void) fprintf(stderr, "# of points must be a power of 2 -- exiting\n");
		exit(1);
	}
		

	switch (ftyp) {

	case FTYP_BATTLE:
		wfltr = &wfltrBattleLemarie;
		(void) strcpy(type, "BATTLE-LEMARIE");
		break;

	case FTYP_BURT:
		wfltr = &wfltrBurtAdelson;
		(void) strcpy(type, "BURT-ADELSON");
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
		(void) sprintf(type, "COIFLET%d", orderCoif);
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
		(void) sprintf(type, "DAUB%d", orderDaub);
		break;

	case FTYP_HAAR:
		wfltr = &wfltrHaar;
		(void) strcpy(type, "HAAR");
		break;

	case FTYP_PSCOIFLET:
		wfltr = &wfltrPseudocoiflet_4_4;
		(void) strcpy(type, "PSEUDOCOIFLET");
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
		(void) sprintf(type, "Spline %s", orderSpline);
		break;

	default:
		NOT_REACHED;
	}

	if (wfltr == NULL) {
		(void) fprintf(stderr,
				"requested filter \"%s\" not yet supported -- exiting\n",
				type);
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

	y = da1d_new(n);
	if (ndim <= 0)
		wxfrm_da1d(x, n, isFwd, wfltr, y);
	else
		wxfrm_dand(x, nOfDim, ndim, isFwd, isStd, wfltr, y);

	if (magsort) {
		yMax = 0.0;
		for (i = 0; i < n; i++) {
			y[i] = fabs(y[i]);
			if (y[i] > yMax)
				yMax = y[i];
		}
		if (yMax > 0.0) {
			for (i = 0; i < n; i++)
				y[i] /= yMax;
		}
		qsort((char *) y, n, sizeof(y[0]), dbl_revCmp);
	}

	switch (ofmt) {

	case OFMT_GNUPLOT:
	case OFMT_XGRAPH:
		chComment = ( ofmt == OFMT_XGRAPH ? '"' : '#' );
		if (isFwd)
			(void) printf("%c wavelet (%s)\n", chComment, type);
		else
			(void) printf("%c inverse wavelet (%s)\n", chComment, type);
		if (magsort) {
			/*
			 *	The 'magsort' option is usually used in connection with a
			 *	log plot, so we use the graphics tolerance in a relative
			 *	sense.
			 */
			tolGraph = 1.0e-3;	/* relative tolerance */
			for (i = 0; i < n; i++) {
				if (i == 0 || i == n-1
						|| (yPrev != 0.0 && fabs((y[i] - yPrev) / yPrev) >= tolGraph)) {
					if (i != 0)
						(void) printf("%10.6f %10.4g\n", RATIO(i,n), yPrev);
					(void) printf("%10.6f %10.3g\n", RATIO(i,n), y[i]);
					(void) printf("%10.6f %10.3g\n", RATIO(i+1,n), y[i]);
				}
				yPrev = y[i];
			}
		} else {
			tolGraph = 1.0e-4;	/* absolute tolerance */
			for (i = 0; i < n; i++) {
				if (i == 0 || i == n-1 || fabs(y[i] - yPrev) >= tolGraph) {
					if (i != 0)
						(void) printf("%d %10.4f\n", i, yPrev);
					(void) printf("%d %10.4f\n", i, y[i]);
					(void) printf("%d %10.4f\n", i+1, y[i]);
				}
				yPrev = y[i];
			}
		}
		break;

	case OFMT_NORMAL:
		for (i = 0; i < n; i++) {
			(void) printf("%f", y[i]);
			if (ndim >= 1 && !oneColumn) {
				if (((i + 1) % nOfDim[ndim - 1]) == 0)
					putchar('\n');
				else
					putchar(' ');
			} else
				putchar('\n');
		}
		break;

	default:
		NOT_REACHED;
	}

	da1d_delete(y);
	FREE_LINTOK(x);

	return;
}

/* usage -- issue a usage error message */
static void usage()
{
	(void) fprintf(stderr,
			"usage: %s [{args}] [{data file name}]\n", progname);
	(void) fprintf(stderr, "%s\n",
			" {args} are:");
	(void) fprintf(stderr, "%s\n",
			"  -1       force one-column output (only relevant with -m)");
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
			"  -g       output data in a format suitable for \"gnuplot\" input");
	(void) fprintf(stderr, "%s\n",
			"  -H       use Haar basis (default)");
	(void) fprintf(stderr, "%s\n",
			"  -i       perform inverse transform");
	(void) fprintf(stderr, "%s\n",
			"  -m {#}[,{#}]*  interpret data as multidimensional with {#}[,{#}]* extents");
	(void) fprintf(stderr, "%s\n",
			"  -n       use non-standard multidimensional basis");
	(void) fprintf(stderr, "%s\n",
			"  -P       use Pseudocoiflet (4, 4) basis");
	(void) fprintf(stderr, "%s\n",
			"  -s       output coefficient magnitudes sorted in decreasing order");
	(void) fprintf(stderr, "%s\n",
			"  -S {#}   use Spline order {#} (= \"2,2\", \"2,4\", \"3,3\", or \"3,7\")");
	(void) fprintf(stderr, "%s\n",
			"  -x       output data in a format suitable for \"xgraph\" input");
	(void) fprintf(stderr, "%s\n",
			"  -?       this message");
	return;
}
