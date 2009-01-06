#include "local.h"

/*
 *	This file contains the definitions of all the filters we use in wavelet
 *	analysis, reconstruction, and interpolation.  Sources are cited.  One very
 *	common source is Daubechies's book "Ten Lectures on Wavelets" (hereafter
 *	referred to as "TLoW").
 *
 *	Note that orthonormal wavelet filters have identical forward (H) and
 *	inverse (Htilde) smoothing components.
 */

#if !defined(M_SQRT2)
#define M_SQRT2     1.41421356237309504880168872420969808
#endif

/*
 *	-----------------------------------------------------------------
 *	Battle-Lemarie filter
 *
 *	Source: Mallat, "A Theory for Multiresolution Signal Decomposition: The
 *	  Wavelet Representation", IEEE PAMI, v. 11, no. 7, 674-693, Table 1
 */

static double cHBattleLemarie[] = {
	M_SQRT2 * -0.002,
	M_SQRT2 * -0.003,
	M_SQRT2 *  0.006,
	M_SQRT2 *  0.006,
	M_SQRT2 * -0.013,
	M_SQRT2 * -0.012,
	M_SQRT2 *  0.030,  /*  5 and 6 sign change from Mallat's paper */
	M_SQRT2 *  0.023,
	M_SQRT2 * -0.078,
	M_SQRT2 * -0.035,
	M_SQRT2 *  0.307,
	M_SQRT2 *  0.542,
	M_SQRT2 *  0.307,
	M_SQRT2 * -0.035,
	M_SQRT2 * -0.078,
	M_SQRT2 *  0.023,
	M_SQRT2 *  0.030,
	M_SQRT2 * -0.012,
	M_SQRT2 * -0.013,
	M_SQRT2 *  0.006,
	M_SQRT2 *  0.006,
	M_SQRT2 * -0.003,
	M_SQRT2 * -0.002,
	  0.0
};

waveletfilter wfltrBattleLemarie = {
	cHBattleLemarie,
	N_ELEM(cHBattleLemarie),
	cHBattleLemarie,
	N_ELEM(cHBattleLemarie),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	11, 11, 11, 11
};

/*
 *	-----------------------------------------------------------------
 *	Burt-Adelson filter
 *
 *	Source: TLoW, Table 8.4
 */
static double cHBurtAdelson[] = {
	M_SQRT2 * -1.0 / 20.0,
	M_SQRT2 *  5.0 / 20.0,
	M_SQRT2 * 12.0 / 20.0,
	M_SQRT2 *  5.0 / 20.0,
	M_SQRT2 * -1.0 / 20.0,
	  0.0
};
static double cHtildeBurtAdelson[] = {
	  0.0,
	M_SQRT2 *  -3.0 / 280.0,
	M_SQRT2 * -15.0 / 280.0,
	M_SQRT2 *  73.0 / 280.0,
	M_SQRT2 * 170.0 / 280.0,
	M_SQRT2 *  73.0 / 280.0,
	M_SQRT2 * -15.0 / 280.0,
	M_SQRT2 *  -3.0 / 280.0
};
waveletfilter wfltrBurtAdelson = {
	cHBurtAdelson,
	N_ELEM(cHBurtAdelson),
	cHtildeBurtAdelson,
	N_ELEM(cHtildeBurtAdelson),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	2, 2, 4, 2
};

/*
 *	-----------------------------------------------------------------
 *	Coiflet filters
 *
 *	Source: Beylkin, Coifman, and Rokhlin "Fast Wavelet Transforms and
 *	  Numerical Algorithms I", Comm. Pure Appl. Math, v. 44, Appendix A
 */

#ifndef SQRT15
#define SQRT15 3.87298334620741688517927
#endif

static double cHCoiflet_2[] = {
	M_SQRT2 * (SQRT15 - 3) / 32.0,
	M_SQRT2 * (1 - SQRT15) / 32.0,
	M_SQRT2 * (6 - 2 * SQRT15) / 32.0,
	M_SQRT2 * (2 * SQRT15 + 6) / 32.0,
	M_SQRT2 * (SQRT15 + 13) / 32.0,
	M_SQRT2 * (9 - SQRT15) / 32.0
};
waveletfilter wfltrCoiflet_2 = {
	cHCoiflet_2,
	N_ELEM(cHCoiflet_2),
	cHCoiflet_2,
	N_ELEM(cHCoiflet_2),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	3, 1, 3, 1
};

static double cHCoiflet_4[] = {
	 0.0011945726958388,
	-0.01284557955324,
	 0.024804330519353,
	 0.050023519962135,
	-0.15535722285996,
	-0.071638282295294,
	 0.57046500145033,
	 0.75033630585287,
	 0.28061165190244,
	-0.0074103835186718,
	-0.014611552521451,
	-0.0013587990591632
};
waveletfilter wfltrCoiflet_4 = {
	cHCoiflet_4,
	N_ELEM(cHCoiflet_4),
	cHCoiflet_4,
	N_ELEM(cHCoiflet_4),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	6, 4, 6, 4
};

static double cHCoiflet_6[] = {
	-0.0016918510194918,
	-0.00348787621998426,
	 0.019191160680044,
	 0.021671094636352,
	-0.098507213321468,
	-0.056997424478478,
	 0.45678712217269,
	 0.78931940900416,
	 0.38055713085151,
	-0.070438748794943,
	-0.056514193868065,
	 0.036409962612716,
	 0.0087601307091635,
	-0.011194759273835,
	-0.0019213354141368,
	 0.0020413809772660,
	 0.00044583039753204,
	-0.00021625727664696
};
waveletfilter wfltrCoiflet_6 = {
	cHCoiflet_6,
	N_ELEM(cHCoiflet_6),
	cHCoiflet_6,
	N_ELEM(cHCoiflet_6),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	6, 10, 6, 10
};

/*
 *	-----------------------------------------------------------------
 *	Daubechies filters
 *
 *	Source: TLoW, Table 6.1
 */

#ifndef SQRT3
#define SQRT3 1.73205080756887729352745
#endif

static double cHDaubechies_4[] = {
	M_SQRT2 * (1 + SQRT3) / 8.0,
	M_SQRT2 * (3 + SQRT3) / 8.0,
	M_SQRT2 * (3 - SQRT3) / 8.0,
	M_SQRT2 * (1 - SQRT3) / 8.0
};
waveletfilter wfltrDaubechies_4 = {
	cHDaubechies_4,
	N_ELEM(cHDaubechies_4),
	cHDaubechies_4,
	N_ELEM(cHDaubechies_4),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	1, 1, 1, 1
};

static double cHDaubechies_6[] = {
	 0.332670552950,
	 0.806891509311,
	 0.459877502118,
	-0.135011020010,
	-0.085441273882,
	 0.035226291882
};
waveletfilter wfltrDaubechies_6 = {
	cHDaubechies_6,
	N_ELEM(cHDaubechies_6),
	cHDaubechies_6,
	N_ELEM(cHDaubechies_6),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	1, 3, 1, 3
};

static double cHDaubechies_8[] = {
	 0.230377813309,
     0.714846570553,
	 0.6308807667930,
	-0.027983769417,
	-0.187034811719,
	 0.030841381836,
	 0.032883011667,
	-0.010597401785
};
waveletfilter wfltrDaubechies_8 = {
	cHDaubechies_8,
	N_ELEM(cHDaubechies_8),
	cHDaubechies_8,
	N_ELEM(cHDaubechies_8),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	1, 5, 1, 5
};

static double cHDaubechies_10[] = {
	 0.1601023979741929,
	 0.6038292697971895,
	 0.7243085284377726,
	 0.1384281459013203,
	-0.2422948870663823,
	-0.0322448695846381,
	 0.0775714938400459,
	-0.0062414902127983,
	-0.0125807519990820,
	 0.0033357252854738
};
waveletfilter wfltrDaubechies_10 = {
	cHDaubechies_10,
	N_ELEM(cHDaubechies_10),
	cHDaubechies_10,
	N_ELEM(cHDaubechies_10),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	1, 7, 1, 7
};

static double cHDaubechies_12[] = {
	 0.1115407433501095,
	 0.4946238903984533,
	 0.7511339080210959,
	 0.3152503517091982,
	-0.2262646939654400,
	-0.1297668675672625,
	 0.0975016055873225,
	 0.0275228655303053,
	-0.0315820393184862,
	 0.0005538422011614,
	 0.0047772575119455,
	-0.0010773010853085
};
waveletfilter wfltrDaubechies_12 = {
	cHDaubechies_12,
	N_ELEM(cHDaubechies_12),
	cHDaubechies_12,
	N_ELEM(cHDaubechies_12),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	1, 9, 1, 9
};

static double cHDaubechies_20[] = {
	 0.026670057901,
	 0.188176800078,
	 0.527201188932,
	 0.688459039454,
	 0.281172343661,
	-0.249846424327,
	-0.195946274377,
	 0.127369340336,
	 0.093057364604,
	-0.071394147166,
	-0.029457536822,
	 0.033212674059,
	 0.003606553567,
	-0.010733175483,
	 0.001395351747,
	 0.001992405295,
	-0.000685856695,
	-0.000116466855,
	 0.000093588670,
	-0.000013264203
};
waveletfilter wfltrDaubechies_20 = {
	cHDaubechies_20,
	N_ELEM(cHDaubechies_20),
	cHDaubechies_20,
	N_ELEM(cHDaubechies_20),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	2, 16, 2, 16
};


/*
 *	-----------------------------------------------------------------
 *	Haar filter
 *
 *	Source: TLoW, p. 10 (and lots of other places!)
 */

static double cHHaar[] = {
	M_SQRT2 * 1.0 / 2.0,
	M_SQRT2 * 1.0 / 2.0
};

waveletfilter wfltrHaar = {
	cHHaar,
	N_ELEM(cHHaar),
	cHHaar,
	N_ELEM(cHHaar),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	0, 0, 0, 0
};

/*
 *	-----------------------------------------------------------------
 *	Pseudocoiflet filter
 *	Source: Reissell, "Multiresolution Geometric Algorithms Using Wavelets I:
 *	  Representation for Parametric Curves and Surfaces", UBC TR 93-17, p. 33
 */

static double cHPseudocoiflet_4[] = {
	M_SQRT2 *  -1.0 / 512.0,
	  0.0,
	M_SQRT2 *  18.0 / 512.0,
	M_SQRT2 * -16.0 / 512.0,
	M_SQRT2 * -63.0 / 512.0,
	M_SQRT2 * 144.0 / 512.0,
	M_SQRT2 * 348.0 / 512.0,
	M_SQRT2 * 144.0 / 512.0,
	M_SQRT2 * -63.0 / 512.0,
	M_SQRT2 * -16.0 / 512.0,
	M_SQRT2 *  18.0 / 512.0,
	  0.0,
	M_SQRT2 *  -1.0 / 512.0,
      0.0
};
static double cHtildePseudocoiflet_4[] = {
	  0.0,
	M_SQRT2 *  -1.0 / 32.0,
	  0.0,
	M_SQRT2 *   9.0 / 32.0,
	M_SQRT2 *  16.0 / 32.0,
	M_SQRT2 *   9.0 / 32.0,
	  0.0,
	M_SQRT2 *  -1.0 / 32.0
};
waveletfilter wfltrPseudocoiflet_4_4 = {
	cHPseudocoiflet_4,
	N_ELEM(cHPseudocoiflet_4),
	cHtildePseudocoiflet_4,
	N_ELEM(cHtildePseudocoiflet_4),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	6, 2, 4, 6
};

/*
 *	-----------------------------------------------------------------
 *	Spline filters
 *
 *	Source: TLoW, Table 8.2 (corrected)
 */
static double cHSpline_2[] = {
	M_SQRT2 * -0.125,
	M_SQRT2 *  0.25,
	M_SQRT2 *  0.75,
	M_SQRT2 *  0.25,
	M_SQRT2 * -0.125,
	  0.0
};
static double cHSpline_3[] = {
	M_SQRT2 * 1.0 / 8.0,
	M_SQRT2 * 3.0 / 8.0,
	M_SQRT2 * 3.0 / 8.0,
	M_SQRT2 * 1.0 / 8.0
};
static double cHSpline_4[] = {
	M_SQRT2 *   3.0 / 128.0,
	M_SQRT2 *  -6.0 / 128.0,
	M_SQRT2 * -16.0 / 128.0,
	M_SQRT2 *  38.0 / 128.0,
	M_SQRT2 *  90.0 / 128.0,
	M_SQRT2 *  38.0 / 128.0,
	M_SQRT2 * -16.0 / 128.0,
	M_SQRT2 *  -6.0 / 128.0,
	M_SQRT2 *   3.0 / 128.0,
	   0.0
};
static double cHtildeSpline_2[] = {
	   0.0,
	M_SQRT2 * 1.0 / 4.0,
	M_SQRT2 * 2.0 / 4.0,
	M_SQRT2 * 1.0 / 4.0
};
static double cHtildeSpline_3[] = {
	M_SQRT2 *  3.0 / 64.0,
	M_SQRT2 * -9.0 / 64.0,
	M_SQRT2 * -7.0 / 64.0,
	M_SQRT2 * 45.0 / 64.0,
	M_SQRT2 * 45.0 / 64.0,
	M_SQRT2 * -7.0 / 64.0,
	M_SQRT2 * -9.0 / 64.0,
	M_SQRT2 *  3.0 / 64.0
};
static double cHtildeSpline_7[] = {
	M_SQRT2 *   -35.0 / 16384.0,
	M_SQRT2 *  -105.0 / 16384.0,
	M_SQRT2 *  -195.0 / 16384.0,
	M_SQRT2 *   865.0 / 16384.0,
	M_SQRT2 *   363.0 / 16384.0,	/* corrected ("363" was "336") in text) */
	M_SQRT2 * -3489.0 / 16384.0,
	M_SQRT2 *  -307.0 / 16384.0,
	M_SQRT2 * 11025.0 / 16384.0,
	M_SQRT2 * 11025.0 / 16384.0,
	M_SQRT2 *  -307.0 / 16384.0,
	M_SQRT2 * -3489.0 / 16384.0,
	M_SQRT2 *   363.0 / 16384.0,	/* corrected ("363" was "336") in text) */
	M_SQRT2 *   865.0 / 16384.0,
	M_SQRT2 *  -195.0 / 16384.0,
	M_SQRT2 *  -105.0 / 16384.0,
	M_SQRT2 *   -35.0 / 16384.0
};

waveletfilter wfltrSpline_2_2 = {
	cHSpline_2,
	N_ELEM(cHSpline_2),
	cHtildeSpline_2,
	N_ELEM(cHtildeSpline_2),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	2, 0, 2, 2
};
waveletfilter wfltrSpline_2_4 = {
	cHSpline_4,
	N_ELEM(cHSpline_4),
	cHtildeSpline_2,
	N_ELEM(cHtildeSpline_2),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	4, 0, 2, 4
};
waveletfilter wfltrSpline_3_3 = {
	cHSpline_3,
	N_ELEM(cHSpline_3),
	cHtildeSpline_3,
	N_ELEM(cHtildeSpline_3),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	1, 3, 3, 1
};
waveletfilter wfltrSpline_3_7 = {
	cHSpline_3,
	N_ELEM(cHSpline_3),
	cHtildeSpline_7,
	N_ELEM(cHtildeSpline_7),
	/* offsets of H, G, Htilde, and Gtilde, respectively */
	1, 7, 7, 1
};

/*
 *	-----------------------------------------------------------------
 */

/* wfltr_exchange -- exchange the normal and tilde components of a wavelet filter */
void wfltr_exchange(wfltrNorm, wfltrRev)
	waveletfilter *wfltrNorm;	/* in: original filter */
	waveletfilter *wfltrRev;	/* out: reversed filter (ok if == wfltrNorm) */
{
	int iSwap;
	double *pDSwap;

	/*
	 *	This function is only relevant for biorthogonal filters.  Orthogonal
	 *	filters are unchanged.
	 */

	iSwap = wfltrNorm->offH;
	wfltrRev->offH = wfltrNorm->offHtilde;
	wfltrRev->offHtilde = iSwap;

	iSwap = wfltrNorm->nH;
	wfltrRev->nH = wfltrNorm->nHtilde;
	wfltrRev->nHtilde = iSwap;

	iSwap = wfltrNorm->offG;
	wfltrRev->offG = wfltrNorm->offGtilde;
	wfltrRev->offGtilde = iSwap;

	pDSwap = wfltrNorm->cH;
	wfltrRev->cH = wfltrNorm->cHtilde;
	wfltrRev->cHtilde = pDSwap; 
	
	return;
}
