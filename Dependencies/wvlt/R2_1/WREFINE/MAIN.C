#include "local.h"

static void doit();
static void usage();

static char *progname = NULL;

static FILE *fpIn = NULL;
static int n = -1;
static int fInterp = 2;
static int ndim = -1;

static enum {
	OFMT_GNUPLOT, OFMT_NORMAL, OFMT_XGRAPH, OFMT_TWOCOLUMN
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

#define MXN_DIM 10	/* maximum dimensionality of data */
#ifdef ND_REFINEMENT_SUPPORTED
static unsigned long nOfDim[MXN_DIM];
#endif

void main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	extern char *optarg;
	extern int optind;
	bool ok = TRUE;
	char *pOptarg;
	int lgFInterp;

	progname = argv[0];
	while ((ch = getopt(argc, argv, "2ABC:D:ef:gHm:n:o:PS:x?")) != -1) {
		switch (ch) {

		case '2':
			ofmt = OFMT_TWOCOLUMN;
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

		case 'f':
			fInterp = atoi(optarg);
			for (lgFInterp = 0; (1 << lgFInterp) < fInterp; lgFInterp++)
				continue;
			if ((1 << lgFInterp) != fInterp)
				ok = FALSE;
			break;

		case 'g':
			ofmt = OFMT_GNUPLOT;
			break;

		case 'H':
			ftyp = FTYP_HAAR;
			break;

		case 'm':
#ifdef ND_REFINEMENT_SUPPORTED
			ndim = 0;
			n = 1;
			pOptarg = optarg;
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
#else
			(void) fprintf(stderr,
					"multidimensional refinement not yet supported -- exiting\n");
			exit(1);
#endif
			break;

		case 'n':
			n = atoi(optarg);
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

		default:
			ok = FALSE;
			break;
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

static void doit()
{
	int i, lgN, nInterp;
	double x, *y, yNew;
	doublearray1d yRefined;
	char type[20];
	waveletfilter *wfltr = NULL;

	if (n <= 1) {
		/* count values */
		y = NULL;
		n = 0;
		while (fscanf(fpIn, "%lf", &yNew) == 1) {
			VOID_MR_ALLOC_LINTOK(y, n+1, double);
			y[n++] = yNew;
		}
	} else {
		(void) MALLOC_LINTOK(y, n, double);
		for (i = 0; i < n; i++) {
			if (fscanf(fpIn, "%lf", &y[i]) != 1) {
				(void) fprintf(stderr, "unexpected EOF -- exiting\n");
				exit(1);
			}
		}
	}
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
		(void) sprintf(type, "SPLINE%d", orderSpline);
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

	if (exchangeCoeffs)
		wfltr_exchange(wfltr, wfltr);

	/* override offsets, if requested */
	if (overrideOffsets) {
		wfltr->offH = offH;
		wfltr->offG = offG;
		wfltr->offHtilde = offHtilde;
		wfltr->offGtilde = offGtilde;
	}

	nInterp = fInterp * n;
	if (ndim <= 0) {
		yRefined = da1d_new(nInterp);
		wrefine_da1d(y, n, nInterp, wfltr, yRefined);
	} else {
#ifdef ND_REFINEMENT_SUPPORTED
		wrefine_dand(y, nOfDim, ndim, nOfDimNew, wfltr, yRefined);
#else
		yRefined = NULL;	/* shut lint up */
#endif
	}

	switch (ofmt) {

	case OFMT_GNUPLOT:
		(void) printf("# wavelet (%s) interpolation\n", type);
		(void) printf("%10.6f %10.6f\n", 0.0, yRefined[0]);
		for (i = 1; i < nInterp; i++) {
			if (yRefined[i-1] != yRefined[i]) {
				x = RATIO(i, fInterp);
				(void) printf("%10.6f %10.6f\n", x, yRefined[i-1]);
				(void) printf("%10.6f %10.6f\n", x, yRefined[i]);
			}
		}
		(void) printf("%10.6f %10.6f\n", (double) n, yRefined[nInterp - 1]);
		break;

	case OFMT_NORMAL:
		for (i = 0; i < nInterp; i++)
			(void) printf("%f\n", yRefined[i]);
		break;

	case OFMT_TWOCOLUMN:
		for (i = 0; i < nInterp; i++)
			(void) printf("%f %f\n", RATIO(i, fInterp), yRefined[i]);
		break;

	case OFMT_XGRAPH:
		(void) printf("\"wavelet (%s) interpolation\n", type);
		(void) printf("%10.6f %10.6f\n", 0.0, yRefined[0]);
		for (i = 1; i < nInterp; i++) {
			if (yRefined[i-1] != yRefined[i]) {
				x = RATIO(i, fInterp);
				(void) printf("%10.6f %10.6f\n", x, yRefined[i-1]);
				(void) printf("%10.6f %10.6f\n", x, yRefined[i]);
			}
		}
		(void) printf("%10.6f %10.6f\n", (double) n, yRefined[nInterp - 1]);
		break;

	default:
		NOT_REACHED;
	}

	da1d_delete(yRefined);
	FREE_LINTOK(y);

	return;
}

/* usage: issue a usage error message */
static void usage()
{
	(void) fprintf(stderr,
			"usage: %s [{args}] [{data file name}]\n", progname);
	(void) fprintf(stderr, "%s\n",
			" {args} are:");
	(void) fprintf(stderr, "%s\n",
			"  -A {#}         use Burt-Adelson basis");
	(void) fprintf(stderr, "%s\n",
			"  -B {#}         use Battle-Lemarie basis");
	(void) fprintf(stderr, "%s\n",
			"  -D {#}         use Daubechies order {#} (= 4, 6, 8, 10, 12, or 20) basis");
	(void) fprintf(stderr, "%s\n",
			"  -e             exchange normal and tilde components");
	(void) fprintf(stderr, "%s\n",
			"  -f {#}         interpolate by a factor of {#} (must be power of 2)");
	(void) fprintf(stderr, "%s\n",
			"  -g             output data in a format suitable for \"gnuplot\" input");
	(void) fprintf(stderr, "%s\n",
			"  -H {#}         use Haar basis (default)");
#ifdef ND_REFINEMENT_SUPPORTED
	(void) fprintf(stderr, "%s\n",
			"  -m {#}[,{#}]*  interpret data as multidimensional with {#}[,{#}]* extents");
#endif
	(void) fprintf(stderr, "%s\n",
			"  -n {#}         there are {#} points in (1-D) input");
	(void) fprintf(stderr, "%s\n",
			"  -o {#}[,{#}[,{#}[,{#}]]]   override intrinsic H, G, Htilde, and Gtilde");
	(void) fprintf(stderr, "%s\n",
			"                   filter offsets to {#}, {#}, {#}, and {#}");
	(void) fprintf(stderr, "%s\n",
			"                   (with reasonable defaults)");
	(void) fprintf(stderr, "%s\n",
			"  -S {#}         use Spline order {#} (= \"2,2\", \"2,4\", \"3,3\", or \"3,7\")");
	(void) fprintf(stderr, "%s\n",
			"  -x             output data in a format suitable for \"xgraph\" input");
	(void) fprintf(stderr, "%s\n",
			"  -?             this message");
	return;
}
