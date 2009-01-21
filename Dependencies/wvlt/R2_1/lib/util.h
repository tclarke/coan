/* $Header: /grads6/bobl/src/lib/util/RCS/util.h,v 1.3 1994/07/14 19:33:53 bobl Exp $ */

#ifndef INCLUDED_UTIL
#define INCLUDED_UTIL

#if !defined(FILE) && !defined(INCLUDED_STDIO)
#define INCLUDED_STDIO
#include <stdio.h>
#endif

#if defined(ARCH_SUN4)
#include <unistd.h>
#endif

//#include <values.h>
#include <stdlib.h>
#include <string.h>

#if defined(NULL) && defined(ARCH_IBM)
#undef NULL
/*
 *	AIX defines NULL as ((void *) 0).  This causes problems with lint.
 */
#define NULL 0
#endif

/*
 *	IBM lint doesn't like mixing old style function declarations with function
 *	prototypes, so we disable prototypes under those circumstances.  This is
 *	okay, since lint will still check function arguments, but of course it
 *	won't check against prototypes.  We should be able to rely on the compiler
 *	for that.
 */
#if defined(__STDC__) \
	|| defined(__cplusplus) \
	|| defined(ARCH_SGI) \
	|| defined(ARCH_DOSTC) \
	|| (defined(ARCH_IBM) && !defined(lint))
# define _PROTO(s) s
#else
# define _PROTO(s) ()
#endif

/*	@(#)assert.h 1.7 88/02/07 SMI; from UCB 4.2 85/01/21	*/
/* modified to call abort() by bobl 91/11/15 */

#ifdef _assert
#undef _assert
#endif

#ifdef assert
#undef assert
#endif

#ifndef NDEBUG
#if defined(ARCH_SGI) || defined(ARCH_SUN4)
# if defined(ARCH_SGI)
extern void abort _PROTO((void));
# endif
# if defined(__GNUC__) && defined(ARCH_SUN4) && OSREL_MAJOR < 5
extern int fprintf _PROTO((FILE *f, char *s, ...));
# endif
#endif
#define _assert(ex)	\
	{ \
		if (!(ex)) { \
			(void) fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", \
					__FILE__, __LINE__); \
			abort(); \
		} \
	}
#define assert(ex)	_assert(ex)
#else
#define _assert(ex)
#define assert(ex)
#endif

#ifndef DEFINED_BOOL
#define DEFINED_BOOL
typedef int bool;
/*
 *	If TRUE and FALSE are previously defined, they had better be the right
 *	values.  What follows should blow up if that is not the case.
 */
# if !defined(TRUE) || TRUE != 1
#define TRUE 1
# endif
# if !defined(FALSE) || FALSE != 0
#define FALSE 0
# endif
#endif

#ifndef DEFINED_GENERICPTR
#define DEFINED_GENERICPTR
#if defined(ARCH_IBM) || defined(ARCH_SGI) || (defined(ARCH_SUN4) && OSREL_MAJOR >= 5)
typedef void *genericptr;
#else
# ifdef lint /* lint is kinda stupid about void * */
typedef char *genericptr;
# else
typedef void *genericptr;
# endif
#endif
#endif

#define N_ELEM(st) (sizeof(st)/sizeof((st)[0]))

#if defined(lint) || defined(NDEBUG)
#define NOT_REACHED
#else
#define NOT_REACHED \
	{ (void) fprintf(stderr, \
			"Unreachable statement reached at line %d in file \"%s\" -- exiting.\n", \
			__LINE__, __FILE__); \
	exit(1); }
#endif


/*
 *	Useful minimum/maximum macros.
 */
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )

/*
 *	According to K & R, the value of i % j is undefined for negative operands.
 *	It is often the case, however, that we want the result to be cyclical, so
 *	that -1 mod 5 = 4, for example.  The following macro will do this in a
 *	fairly machine-independent manner.  (Notice that we ignore the mathematical
 *	possibility of j <= 0, since in such a case there's probably something
 *	wrong with the code.)
 */
#define MOD(i, j) ( (i) >= 0 ? (i) % (j) : ((j) - ((-(i)) % (j))) % (j) )

#define RATIO(i,j) (((double) (i))/((double) (j)))

#define STR_EQ(s1, s2) (strcmp((s1), (s2)) == 0)

#if defined(ARCH_IBM) || defined(ARCH_SGI) || (defined(ARCH_SUN4) && OSREL_MAJOR >= 5)
/* sincos() is not available under AIX or IRIX */
#define SINCOS(a, pS, pC) \
	{ \
		(*pS) = sin(a); \
		(*pC) = cos(a); \
	}
#else
#define SINCOS(a, pS, pC) sincos(a, pS, pC)
#endif

#ifndef M_PI
#define M_PI 3.1415926535897931160
#endif
#define DEG_TO_RAD(d) (M_PI * (d) / 180.0)
#define RAD_TO_DEG(d) (180.0 * (d) / M_PI)

#define MXL_PATH 128

/*
 *	GNU CC complains if these aren't declared.
 */
#if (defined(__GNUC__) && (!defined(ARCH_SUN4) || OSREL_MAJOR < 5)) || defined(ARCH_DOSTC)
extern int getopt _PROTO((int argc, char **argv, char *optstring));
#endif
/* SGI's have extern declarations of these */
#if defined(__GNUC__) && !defined(ARCH_SGI)
extern int _filbuf _PROTO((FILE *f));
extern int _flsbuf _PROTO((unsigned char c, FILE *f));
#endif
/* IBM RS6000's have extern declarations of these */
#if defined(__GNUC__) && !defined(ARCH_IBM)
extern int abs _PROTO((int i));
# ifndef ARCH_SUN4
extern void exit _PROTO((int status));
extern qsort _PROTO((char *base, int nel, int width, int (*compar)()));
# endif
# if !defined(ARCH_SUN4) || OSREL_MAJOR < 5
extern long strtol _PROTO((char *s, char **pS, int base));
# endif
#endif
/* IBM RS6000's and SGI's have extern declarations of these */
#if defined(__GNUC__) && defined(ARCH_SUN4) && OSREL_MAJOR < 5
extern int fclose _PROTO((FILE *f));
extern int fflush _PROTO((FILE *f));
extern int fgetc _PROTO((FILE *f));
extern int fprintf _PROTO((FILE *f, char *s, ... ));
extern int fread _PROTO((char *p, int s, int n, FILE *f));
extern int fscanf _PROTO((FILE *f, char *s, ...));
extern int fseek _PROTO((FILE *f, long off, int whence));
extern int fwrite _PROTO((char *p, int s, int n, FILE *f));
extern int printf _PROTO((char *s, ... ));
extern puts _PROTO((char *s));
extern long random _PROTO(());
extern int scanf _PROTO((char *s, ...));
extern int srandom _PROTO((int s));
#endif

/*
 *	The following BEGIN EXTERNS and END EXTERNS are used to strip util library
 *	extern declarations from util.h when it is being exported.  Delete them at
 *	your peril!
 */


#endif
