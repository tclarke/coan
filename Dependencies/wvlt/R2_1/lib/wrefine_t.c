/*
 *	This is a prototype file, not meant to be compiled directly.
 *	The including source file must define the following:
 *
 *		TYPE_ARRAY -- the base (scalar) type of all argument arrays
 *		FUNC_1D -- the name of the 1-dimensional refinement function
 *		FUNC_ND -- the name of the N-dimensional refinement function
 */

static void wrefine_step _PROTO((waveletfilter *wfltr, TYPE_ARRAY *a, int n));

/* wrefine_[df]a1d -- 1-dimensional discrete wavelet refinement */
void FUNC_1D(a, nA, nNew, wfltr, aRefined)
	TYPE_ARRAY *a;	/* in: original data */
	int nA;				/* in: size of a (must be power of 2) */
	int nNew;			/* in: size of aRefined (must be power of 2) */
	waveletfilter *wfltr;	/* in: wavelet filter to use */
	TYPE_ARRAY *aRefined;	/* out: refined data */
{
	int iA;
	double scale;

	assert(nA <= nNew);
	for (iA = 0; iA < nA; iA++)
		aRefined[iA] = a[iA];
	for (iA = 2 * nA; iA <= nNew; iA *= 2)
		wrefine_step(wfltr, aRefined, iA);

	scale = sqrt(RATIO(nNew, nA));
	for (iA = 0; iA < nNew; iA++)
		aRefined[iA] *= (TYPE_ARRAY)scale;

	return;
}

#if 0 /* needs work */
/* wrefine_[df]and -- n-dimensional discrete wavelet transform */
void FUNC_ND(aO, nAO, nDim, isFwd, wfltr, nAR, aR)
	TYPE_ARRAY *aO;	/* in: original data */
	int nAO[];	/* in: size of each dimension of aO (must be powers of 2) */
	int nDim;	/* in: number of dimensions (size of nAO[] and nAR[]) */
	bool isFwd;			/* in: TRUE <=> forward transform */
	waveletfilter *wfltr;	/* in: wavelet filter to use */
	int nAR[];	/* in: size of each dimension of aR (must be powers of 2) */
	TYPE_ARRAY *aR;	/* out: refined data (ok if == aO) */
{
	int k;
	int iDim, nAOTotal, nAOMax, nARMax;
	int iO1, iO2, iO3, nO, nONext
	int nOPrev = 1;
	int nRPrev = 1;
	int nR, nRNext;
	TYPE_ARRAY *aOTmp, *aRTmp;

	nAOMax = 1;
	nARMax = 1;
	nAOTotal = 1;
	for (iDim = 0; iDim < nDim; iDim++) {
		if (nAO[iDim] > nAOMax)
			nAOMax = nAO[iDim];
		if (nAR[iDim] > nARMax)
			nARMax = nAR[iDim];
		nAOTotal *= nAO[iDim];
	}

	/*
	 *	Temporary arrays must be big enough to hold the longest dimension.
	 */
	(void) MALLOC_LINTOK(aOTmp, nAOMax, TYPE_ARRAY);
	(void) MALLOC_LINTOK(aRTmp, nARMax, TYPE_ARRAY);

	for (iDim = 0; iDim < nDim; iDim++) {
		nO = nAO[nDim - 1 - iDim];
		nR = nAR[nDim - 1 - iDim];
		nONext = nO * nOPrev;
		nRNext = nR * nRPrev;
		if (nO >= MIN_ORDER) {
			for (iO2 = 0; iO2 < nAOTotal; iO2 += nONext) {
				for (iO1 = 0; iO1 < nOPrev; iO1++) {
					/*
					 *	Copy the relevant row, column, or whatever into the
					 *	workspace.
					 */
					iO3 = iO1 + iO2;
					for (k = 0; k < nO; k++) {
						aOTmp[k] = aO[iO3];
						iO3 += nOPrev;
					}

					/* Apply the one-dimensional wavelet refinement. */
					FUNC_1D(aOTmp, nO, nR, wfltr, aRTmp);

					/* Copy back from temporary array. */
					i3 = i1 + i2;
>>>>>>>					for (k = 0; k < nR; k++) {
						aR[i3] = aRTmp[k];
						i3 += nRPrev;
					}
				}
			}
		}
		nOPrev = nONext;
		nRPrev = nRNext;
	}

	FREE_LINTOK(aRTmp);
	FREE_LINTOK(aOTmp);

	return;
}
#endif

/* wrefine_step -- perform one iteration of a wavelet refinement */
static void wrefine_step(wfltr, a, n)
	waveletfilter *wfltr;
	TYPE_ARRAY *a;
	int n;
{
	int nDiv2 = n / 2;
	int i, iHtilde, j, iA;
	TYPE_ARRAY *aNew;

	(void) MALLOC_LINTOK(aNew, n, TYPE_ARRAY);
	for (i = 0; i < n; i++)
		aNew[i] = 0.0;

	/*
	 *	Wavelet refinement is identical to an inverse wavelet transform (see
	 *	"wxfrm") with the elements of Gtilde set identically to zero.
	 *
	 *	A single step of the reconstruction is summarized by:
	 *
	 *		aNew[0..n-1] = Htilde^t * a[0..n/2-1]
	 *
	 *	where Htilde^t is the transpose of Htilde.
	 */
	for (j = 0; j < nDiv2; j++) {
		for (iHtilde = 0; iHtilde < wfltr->nHtilde; iHtilde++) {
			/*
			 *	Each row of Htilde is offset by 2 from the previous one.
			 */
			iA = MOD(2 * j + iHtilde - wfltr->offHtilde, n);
			aNew[iA] += (TYPE_ARRAY)(wfltr->cHtilde[iHtilde] * a[j]);
		}
	}
	for (i = 0; i < n; i++)
		a[i] = aNew[i];

	FREE_LINTOK(aNew);

	return;
}
