/*
 *	This is a prototype file, not meant to be compiled directly.
 *	The including source file must define the following:
 *
 *		TYPE_ARRAY -- the base (scalar) type of all argument arrays
 *		FUNC_1D -- the name of the 1-dimensional transform function
 *		FUNC_ND -- the name of the N-dimensional transform function
 */

/* this is the minimum-order wavelet filter allowed */
#define MIN_ORDER 2

/* multidimensional transforms support a maximum of this many dimensions */
#define MXN_D 32

static void wfltr_convolve _PROTO((TYPE_ARRAY* aTmp1D, waveletfilter *wfltr, bool isFwd,
		TYPE_ARRAY *aIn, int incA, int n, TYPE_ARRAY *aXf));
static void wxfrm_1d_varstep _PROTO((TYPE_ARRAY* aTmp1D, TYPE_ARRAY *a, int incA, int nA, bool isFwd,
		waveletfilter *wfltr, TYPE_ARRAY *aXf));
static void wxfrm_nd_nonstd _PROTO((TYPE_ARRAY* aTmp1D, TYPE_ARRAY *a, int nA[], int nD,
		bool isFwd, waveletfilter *wfltr));
static void wxfrm_nd_std _PROTO((TYPE_ARRAY* aTmp1D, TYPE_ARRAY *a, int nA[], int nD,
		bool isFwd, waveletfilter *wfltr));

/*
 *	To avoid lots of unnecessary malloc/free calls, especially in
 *	multidimensional transforms, we use a "static global" buffer that, at the
 *	highest level, will be allocated once to the maximum possible size and
 *	freed when done.
 */
//static TYPE_ARRAY *aTmp1D = NULL;

/* wfltr_convolve -- perform one convolution of a (general) wavelet transform */
static void wfltr_convolve(aTmp1D, wfltr, isFwd, aIn, incA, n, aXf)
	TYPE_ARRAY *aTmp1D;
          waveletfilter *wfltr;	/* in: convolving filter */
	bool isFwd;			/* in: TRUE <=> forward transform */
	TYPE_ARRAY *aIn;	/* in: input data */
	int incA;			/* in: spacing of elements in aIn[] and aXf[] */
	int n;				/* in: size of aIn and aXf */
	TYPE_ARRAY *aXf;	/* out: output data (OK if == aIn) */
{
	int nDiv2 = n / 2;
	int iA, iHtilde, iGtilde, j, jH, jG, i;
	double sum;
	bool flip;

	/*
	 *	According to Daubechies:
	 *
	 *	H is the analyzing smoothing filter.
	 *	G is the analyzing detail filter.
	 *	Htilde is the reconstruction smoothing filter.
	 *	Gtilde is the reconstruction detail filter.
	 */

	/* the reconstruction detail filter is the mirror of the analysis smoothing filter */
	int nGtilde = wfltr->nH;

	/* the analysis detail filter is the mirror of the reconstruction smoothing filter */
	int nG = wfltr->nHtilde;

	if (isFwd) {
		/*
		 *	A single step of the analysis is summarized by:
		 *		aTmp1D[0..n/2-1] = H * a (smooth component)
		 *		aTmp1D[n/2..n-1] = G * a (detail component)
		 */
		for (i = 0; i < nDiv2; i++) {
			/*
			 *	aTmp1D[0..nDiv2-1] contains the smooth components.
			 */
			sum = 0.0;
			for (jH = 0; jH < wfltr->nH; jH++) {
				/*
				 *	Each row of H is offset by 2 from the previous one.
				 *
				 *	We assume our data is periodic, so we wrap the aIn[] values
				 *	if necessary.  If we have more samples than we have H
				 *	coefficients, we also wrap the H coefficients.
				 */
				iA = MOD(2 * i + jH - wfltr->offH, n);
				sum += wfltr->cH[jH] * aIn[incA * iA];
			}
			aTmp1D[i] = (TYPE_ARRAY)sum;

			/*
			 *	aTmp1D[nDiv2..n-1] contains the detail components.
			 */
			sum = 0.0;
			flip = TRUE;
			for (jG = 0; jG < nG; jG++) {
				/*
				 *	We construct the G coefficients on-the-fly from the
				 *	Htilde coefficients.
				 *
				 *	Like H, each row of G is offset by 2 from the previous
				 *	one.  As with H, we also allow the coefficents of G to
				 *	wrap.
				 *
				 *	Again as before, the aIn[] values may wrap.
				 */
				iA = MOD(2 * i + jG - wfltr->offG, n);
				if (flip)
					sum -= wfltr->cHtilde[nG - 1 - jG] * aIn[incA * iA];
				else
					sum += wfltr->cHtilde[nG - 1 - jG] * aIn[incA * iA];
				flip = !flip;
			}
			aTmp1D[nDiv2 + i] = (TYPE_ARRAY)sum;
		}
	} else {
		/*
		 *	The inverse transform is a little trickier to do efficiently.
		 *	A single step of the reconstruction is summarized by:
		 *
		 *		aTmp1D = Htilde^t * aIn[incA * (0..n/2-1)]
		 *				+ Gtilde^t * aIn[incA * (n/2..n-1)]
		 *
		 *	where x^t is the transpose of x.
		 */
		for (i = 0; i < n; i++)
			aTmp1D[i] = 0.0;	/* necessary */
		for (j = 0; j < nDiv2; j++) {
			for (iHtilde = 0; iHtilde < wfltr->nHtilde; iHtilde++) {
				/*
				 *	Each row of Htilde is offset by 2 from the previous one.
				 */
				iA = MOD(2 * j + iHtilde - wfltr->offHtilde, n);
				aTmp1D[iA] += (TYPE_ARRAY)(wfltr->cHtilde[iHtilde] * aIn[incA * j]);
			}
			flip = TRUE;
			for (iGtilde = 0; iGtilde < nGtilde; iGtilde++) {
				/*
				 *	As with Htilde, we also allow the coefficents of Gtilde,
				 *	which is the mirror of H, to wrap.
				 *
				 *	We assume our data is periodic, so we wrap the aIn[] values
				 *	if necessary.  If we have more samples than we have Gtilde
				 *	coefficients, we also wrap the Gtilde coefficients.
				 */
				iA = MOD(2 * j + iGtilde - wfltr->offGtilde, n);
				if (flip) {
					aTmp1D[iA] -= (TYPE_ARRAY)(wfltr->cH[nGtilde - 1 - iGtilde]
							* aIn[incA * (j + nDiv2)]);
				} else {
					aTmp1D[iA] += (TYPE_ARRAY)(wfltr->cH[nGtilde - 1 - iGtilde]
							* aIn[incA * (j + nDiv2)]);
				}
				flip = !flip;
			}
		}
	}
	for (i = 0; i < n; i++)
		aXf[incA * i] = aTmp1D[i];

	return;
}

/* wxfrm_1d_varstep -- 1-dimensional discrete wavelet transform with variable step size */
static void wxfrm_1d_varstep(aTmp1D, aIn, incA, nA, isFwd, wfltr, aXf)
          TYPE_ARRAY* aTmp1D;
	TYPE_ARRAY *aIn;	/* in: original array */
	int incA;		/* in: spacing of elements in aIn[] and aXf[] */
	int nA;			/* in: size of a (must be power of 2) */
	bool isFwd;		/* in: TRUE <=> forward transform */
	waveletfilter *wfltr;	/* in: wavelet filter to use */
	TYPE_ARRAY *aXf;	/* out: transformed array */
{
	int iA;

	if (nA < MIN_ORDER)
		return;

	if (isFwd) {
		/*
		 *	Start at largest hierarchy, work towards smallest.
		 */
		wfltr_convolve(aTmp1D, wfltr, isFwd, aIn, incA, nA, aXf);
		for (iA = nA / 2; iA >= MIN_ORDER; iA /= 2)
			wfltr_convolve(aTmp1D, wfltr, isFwd, aXf, incA, iA, aXf);
	} else {
		/*
		 *	Start at smallest hierarchy, work towards largest.
		 */
		if (aXf != aIn) {
			for (iA = 0; iA < nA; iA++)
				aXf[incA * iA] = aIn[incA * iA];	/* required for inverse */
		}
		for (iA = MIN_ORDER; iA <= nA; iA *= 2)
			wfltr_convolve(aTmp1D, wfltr, isFwd, aXf, incA, iA, aXf);
	}

	return;
}

/* wxfrm_[fd]a1d -- 1-dimensional discrete wavelet transform */
void FUNC_1D(a, nA, isFwd, wfltr, aXf)
	TYPE_ARRAY *a;		/* in: original array */
	int nA;				/* in: size of a (must be power of 2) */
	bool isFwd;			/* in: TRUE <=> forward transform */
	waveletfilter *wfltr;	/* in: wavelet filter to use */
	TYPE_ARRAY *aXf;	/* out: transformed array */
{
          TYPE_ARRAY* aTmp1D = NULL;

	(void) MALLOC_LINTOK(aTmp1D, nA, TYPE_ARRAY);
	wxfrm_1d_varstep(aTmp1D, a, 1, nA, isFwd, wfltr, aXf);
	FREE_LINTOK(aTmp1D);

	return;
}

/* wxfrm_[fd]and -- n-dimensional discrete wavelet transform */
void FUNC_ND(aIn, nA, nD, isFwd, isStd, wfltr, aXf)
	TYPE_ARRAY *aIn;	/* in: original data */
	int nA[];			/* in: size of each dimension of a (must be powers of 2) */
	int nD;				/* in: number of dimensions (size of nA[]) */
	bool isFwd;			/* in: TRUE <=> forward transform */
	bool isStd;			/* in: TRUE <=> standard basis */
	waveletfilter *wfltr;	/* in: wavelet filter to use */
	TYPE_ARRAY *aXf;	/* out: transformed data (ok if == a) */
{
          TYPE_ARRAY* aTmp1D = NULL;
	int k, d, nDMax, nATot;
	int lg2nA;

	if (nD < 1 || MXN_D < nD) {
		(void) fprintf(stderr,
				"number of dimensions must be between 1 and %d -- exiting\n",
				MXN_D);
		exit(1);
	}

	for (d = 0; d < nD; d++) {
		for (lg2nA = 0; (1 << lg2nA) < nA[d]; lg2nA++)
			continue;
		if ((1 << lg2nA) != nA[d]) {
			(void) fprintf(stderr,
					"size of dimension #%d (= %d) is not a power of 2 -- exiting\n", d, nA[d]);
			exit(1);
		}
	}

	nDMax = 1;
	nATot = 1;
	for (d = 0; d < nD; d++) {
		if (nA[d] > nDMax)
			nDMax = nA[d];
		nATot *= nA[d];
	}

	/*
	 *	Make the "static global" temp array aTmp1D, big enough to hold the
	 *	longest dimension.
	 */
	(void) MALLOC_LINTOK(aTmp1D, nDMax, TYPE_ARRAY);

	/*
	 *	aXf[] starts out equal to aIn[] and gets modified in-place.
	 *	(If they're the same array, we don't need to copy aIn[] to aXf[].)
	 */
	if (aIn != aXf) {
		for (k = 0; k < nATot; k++)
			aXf[k] = aIn[k];
	}

	if (isStd)
		wxfrm_nd_std(aTmp1D, aXf, nA, nD, isFwd, wfltr);
	else
		wxfrm_nd_nonstd(aTmp1D, aXf, nA, nD, isFwd, wfltr);

	FREE_LINTOK(aTmp1D);

	return;
}

/* wxfrm_nd_nonstd -- perform a nonstandard n-dimensional wavelet transform */
static void wxfrm_nd_nonstd(aTmp1D, a, nA, nD, isFwd, wfltr)
          TYPE_ARRAY* aTmp1D;
	TYPE_ARRAY *a;	/* in: original/final data (modified in-place) */
	int nA[];		/* in: size of each dimension of a (must be powers of 2) */
	int nD;			/* in: number of dimensions (size of nA[]) */
	bool isFwd;		/* in: TRUE <=> forward transform */
	waveletfilter *wfltr;	/* in: wavelet filter to use */
{
	int d, d0, dMax, nConv, iConv, iConvDecoded;
	int iA, nATot, nBTot;
	int nARev[MXN_D], nB[MXN_D], incA[MXN_D];
	bool stretchOk[MXN_D];

	/*
	 *	In C, a matrix a declared
	 *
	 *		TYPE_ARRAY a[n][m];
	 *
	 *	has nA declared
	 *
	 *		int nA[] = { n, m };
	 *
	 *	but it is convenient for us to refer to nA[] in reversed form as
	 *	nARev[], so that nARev[0] is the "fastest varying" index in the
	 *	multidimensional mapping a[].
	 */
	for (d = 0; d < nD; d++)
		nARev[d] = nA[nD - 1 - d];

	/*
	 *	A[] is the original matrix.  B[] is the A[]'s submatrix whose
	 *	transform we are computing at any given time.
	 */
	nATot = 1;
	for (d = 0; d < nD; d++) {
		incA[d] = nATot;
		nATot *= nARev[d];
	}

	nBTot = nATot;
	if (isFwd) {
		/* initially, B is equivalent to A */
		for (d = 0; d < nD; d++)
			nB[d] = nARev[d];
		/*
		 *	main loop is in descending scale
		 */
		while (nBTot > 1) {
			for (d0 = 0; d0 < nD; d0++) {
				if (nB[d0] <= 1)
					continue;
				nConv = nBTot / nB[d0];	/* # of convolutions to perform */
				/*
				 *	The idea is to enumerate the nConv convolutions we need to
				 *	do in direction d0 and pull apart the enumeration index,
				 *	reforming it into the index iA of the original matrix.
				 */
				for (iConv = 0; iConv < nConv; iConv++) {
					iA = 0;
					iConvDecoded = iConv;
					for (d = 0; d < nD; d++) {
						if (d != d0) {
							iA += incA[d] * (iConvDecoded % nB[d]);
							iConvDecoded /= nB[d];
						}
					}
					wfltr_convolve(aTmp1D, wfltr, TRUE, &a[iA], incA[d0], nB[d0], &a[iA]);
				}
			}
			nBTot = 1;
			for (d = 0; d < nD; d++) {
				if (nB[d] > 1) {
					nB[d] /= 2;
					nBTot *= nB[d];
				}
			}
		}
	} else {
		/* initially, B is a 1x1x..x1 submatrix */
		for (d = 0; d < nD; d++)
			nB[d] = 1;
		nBTot = 1;
		/*
		 *	main loop is in ascending scale
		 */
		while (nBTot < nATot) {
			/*
			 *	We have to be careful about non-hypercubic grids.  In order to
			 *	duplicate what we've done above for the forward transform, if
			 *	nB[d0] is 1, we only stretch dimension d0 if
			 *
			 *		nARev[d0] / nB[d0] == max( nARev[d] / nB[d] ) for all d
			 *
			 *	The elements of stretchOk will reflect this.  dMax is the
			 *	dimension that has the largest stretch.
			 */
			dMax = 0;
			for (d = 1; d < nD; d++) {
				if (nARev[d] * nB[dMax] >= nARev[dMax] * nB[d])
					dMax = d;
			}
			for (d = 0; d < nD; d++)
				stretchOk[d] = (nB[d] > 1 || (nARev[d] * nB[dMax] >= nARev[dMax] * nB[d]));

			for (d0 = nD - 1; d0 >= 0; d0--) {
				if (!stretchOk[d0])
					continue;
				nB[d0] *= 2;
				nBTot *= 2;
			}

			for (d0 = nD - 1; d0 >= 0; d0--) {
				if (!stretchOk[d0])
					continue;
				nConv = nBTot / nB[d0];
				/*
				 *	As with the forward transform, the idea is to enumerate the
				 *	nConv convolutions we need to do in direction d0 and pull
				 *	apart the enumeration index, reforming it into the index iA
				 *	of the original matrix.
				 */
				for (iConv = 0; iConv < nConv; iConv++) {
					iA = 0;
					iConvDecoded = iConv;
					for (d = 0; d < nD; d++) {
						/*
						 *	Non-stretched directions don't count in nConv, so
						 *	they don't count in the index computation, either.
						 *	Neither does d0, obviously.
						 */
						if (d != d0 && stretchOk[d]) {
							iA += incA[d] * (iConvDecoded % nB[d]);
							iConvDecoded /= nB[d];
						}
					}
					wfltr_convolve(aTmp1D, wfltr, FALSE, &a[iA], incA[d0], nB[d0], &a[iA]);
				}
			}
		}
	}
	return;
}

/* wxfrm_nd_std -- perform a standard n-dimensional wavelet transform */
static void wxfrm_nd_std(aTmp1D, a, nA, nD, isFwd, wfltr)
          TYPE_ARRAY *aTmp1D;
	TYPE_ARRAY *a;		/* in: original/final data (modified in-place) */
	int nA[];		/* in: size of each dimension of a (must be powers of 2) */
	int nD;			/* in: number of dimensions (size of nA[]) */
	bool isFwd;			/* in: TRUE <=> forward transform */
	waveletfilter *wfltr;	/* in: wavelet filter to use */
{
	int incA = 1;
	int incNext, d, i1, i2, nATot;
	int nARev[MXN_D];

	/*
	 *	In C, a matrix "a" declared
	 *
	 *		TYPE_ARRAY a[n][m];
	 *
	 *	has "nA" declared
	 *
	 *		int nA[] = { n, m };
	 *
	 *	but it is convenient for us to refer to nA[] in reversed form as
	 *	nARev[], so that nARev[0] is the "fastest varying" index in the
	 *	multidimensional mapping a[].
	 */
	for (d = 0; d < nD; d++)
		nARev[d] = nA[nD - 1 - d];

	nATot = 1;
	for (d = 0; d < nD; d++)
		nATot *= nARev[d];

	/*
	 *	main loop is over the dimensions
	 */
	for (d = 0; d < nD; d++) {
		incNext = nARev[d] * incA;
		if (nARev[d] >= MIN_ORDER) {
			for (i2 = 0; i2 < nATot; i2 += incNext) {
				for (i1 = 0; i1 < incA; i1++) {
					/*
					 *	Use a variable step size (incA) and pointers to
					 *	perform the one-dimensional wavelet transform in-place.
					 */
					wxfrm_1d_varstep(aTmp1D, &a[i1 + i2], incA, nARev[d], isFwd, wfltr,
							&a[i1 + i2]);
				}
			}
		}
		incA = incNext;
	}
	return;
}

