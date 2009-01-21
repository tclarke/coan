#ifndef _INCLUDED_WVLT
#define _INCLUDED_WVLT

/*
 *	We need to define some structures for quadratic spline wavelets.  For these
 *	biorthogonal filters, we require the analysis and reconstruction smoothing
 *	filters (H and Htilde, respectively).  The analyzing and reconstruction
 *	detail filters (G and Gtilde) are constructed from the smoothing filters.
 */
typedef struct {
	double *cH;
	int nH;
	double *cHtilde;
	int nHtilde;
	int offH, offG, offHtilde, offGtilde;
} waveletfilter;

/* in "wfltr" */
extern waveletfilter wfltrBattleLemarie;
extern waveletfilter wfltrBurtAdelson;
extern waveletfilter wfltrCoiflet_2;
extern waveletfilter wfltrCoiflet_4;
extern waveletfilter wfltrCoiflet_6;
extern waveletfilter wfltrDaubechies_4;
extern waveletfilter wfltrDaubechies_6;
extern waveletfilter wfltrDaubechies_8;
extern waveletfilter wfltrDaubechies_10;
extern waveletfilter wfltrDaubechies_12;
extern waveletfilter wfltrDaubechies_20;
extern waveletfilter wfltrHaar;
extern waveletfilter wfltrPseudocoiflet_4_4;
extern waveletfilter wfltrSpline_2_2;
extern waveletfilter wfltrSpline_2_4;
extern waveletfilter wfltrSpline_3_3;
extern waveletfilter wfltrSpline_3_7;
extern void wfltr_exchange _PROTO((waveletfilter *wfltrNorm,
		waveletfilter *wfltrRev));

/* in "wrefine" */
extern void wrefine_da1d _PROTO((double *a, int nA, int nNew,
		waveletfilter *wfltr, double *aRefined));

/* in "wxfrmf" */
extern void wxfrm_fa1d _PROTO((float *a, int nA, bool isFwd,
		waveletfilter *wfltr, float *aXf));
extern void wxfrm_fand _PROTO((float *a, int nAOfIDim[],
		int nD, bool isFwd, bool isStd, waveletfilter *wfltr, float *aXf));

/* in "wxfrmd" */
extern void wxfrm_da1d _PROTO((double *a, int nA, bool isFwd,
		waveletfilter *wfltr, double *aXf));
extern void wxfrm_dand _PROTO((double *a, int nA[],
		int nD, bool isFwd, bool isStd, waveletfilter *wfltr, double *aXf));

#endif /* _INCLUDED_WVLT */
