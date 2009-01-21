#ifndef INCLUDED_LINTOK

/* $Header: /grads6/bobl/src/lib/lintok/RCS/lintok.h,v 1.3 1995/02/15 00:15:11 bobl Exp bobl $ */

/*
 *	These definitions stop lint from complaining about malloc, calloc,
 *	realloc, and free while at the same time performing checks of
 *	their arguments.  They include:
 *
 *		CALLOC_LINTOK(ptr, nelem, type)
 *		MALLOC_LINTOK(ptr, nelem, type)
 *		REALLOC_LINTOK(ptr, nelem, type)
 *		FREE_LINTOK(ptr)
 *
 *	where "ptr" is a variable of type "type *".  After invoking any
 *	of the "*ALLOC_LINTOK" macros, "ptr" will either be NULL (if the
 *	underlying function returned it) or it will point to an array of
 *	"nelem" objects of type "type".
 *
 *	Note that the usage of MALLOC_LINTOK, CALLOC_LINTOK, and
 *	REALLOC_LINTOK requires more than a name change.  An original
 *	call of the form:
 *
 *		ptr = (type *) malloc(nelem * sizeof(type))
 *
 *	must be replaced by
 *
 *		MALLOC_LINTOK(ptr, nelem, type)
 *
 *	and similarly for CALLOC_LINTOK and REALLOC_LINTOK.  All three
 *	macros return values and may therefore be used as part of
 *	expressions.  You may need to add void casts if you want to
 *	get rid of the "*alloc returns value which is sometimes ignored"
 *	messages (see the accompanying "t_lintok.c" file for examples).
 *
 *	Calls to these macros may be mixed with calls to the original
 *	functions.  The only difference is in the lint behavior.
 *
 *	NULL must be defined.  (Usually, this means including <stdio.h>
 *	somewhere along the way.)
 *
 *	Bob Lewis produced these macros using his own resources and
 *	hereby releases them to the public domain.  The author will
 *	not be held responsible for their use, misuse, or abuse or
 *	any damages arising from same.
 *
 *	This file also includes Doug Brown's "CAST" macro, designed to shut lint
 *	up about casts in general.
 */

#ifndef NULL
#include <stdio.h>
# if defined(NULL) && defined(ARCH_IBM)
# undef NULL
/*
 *	AIX defines NULL as ((void *) 0).  This causes problems with lint.
 */
# define NULL 0
# endif
#endif

/*
 *	Make sure we include externs of malloc() and friends.  These are
 *	system-dependent.
 */
#include <stdlib.h>

/*
 *	The CAST macro is intended to stop lint complaints about non-memory
 *	casts.
 */
#ifdef lint
#define CAST(type, value) \
    ( (value) ? (type) NULL : (type) NULL )
#else
#define CAST(type, value)  ((type) (value))
#endif

#ifdef lint
#define CALLOC_LINTOK(ptr, nelem, type) \
	calloc((ptr = (type *) NULL, (unsigned) (nelem)), (size_t) sizeof(type))
#else
#define CALLOC_LINTOK(ptr, nelem, type) \
	(ptr = (type *) calloc((unsigned) (nelem), (size_t) sizeof(type)))
#endif

#ifdef lint
#define MALLOC_LINTOK(ptr, nelem, type) \
	malloc((ptr = (type *) 0, (size_t) ((nelem) * sizeof(type))))
#else
#define MALLOC_LINTOK(ptr, nelem, type) \
	(ptr = (type *) malloc((size_t) ((nelem) * sizeof(type))))
#endif

#ifdef lint
#define REALLOC_LINTOK(ptr, nelem, type) \
	realloc( \
		(ptr = (type *) 0, (genericptr) 0), \
		(size_t) ((nelem) * sizeof(type)))
#else
#define REALLOC_LINTOK(ptr, nelem, type) \
	(ptr = (type *) realloc( \
		(genericptr) ptr, \
		(size_t) ((nelem) * sizeof(type))))
#endif

/* common uses of malloc/realloc -- use 'em or don't use 'em */

/*   returns a value: */
#define MR_ALLOC_LINTOK(ptr, nelem, type) \
	( (ptr) == NULL \
		? MALLOC_LINTOK(ptr, (nelem), type) \
		: REALLOC_LINTOK(ptr, (nelem), type) )
/*   doesn't return a value: */
#ifdef lint
/*
 *	Remember that the code lint checks need not be semantically equivalent
 *	to non-lint code.
 */
#define VOID_MR_ALLOC_LINTOK(ptr, nelem, type) \
	(((void) MALLOC_LINTOK(ptr, (nelem), type), \
		(void) REALLOC_LINTOK(ptr, (nelem), type)))
#else
#define VOID_MR_ALLOC_LINTOK(ptr, nelem, type) \
	((void) MR_ALLOC_LINTOK(ptr, (nelem), type))
#endif

/*
 *	These next macros invoke CALLOC_LINTOK, MALLOC_LINTOK, REALLOC_LINTOK,
 *	and MR_ALLOC_LINTOK with an error exit if they fail.
 *
 *	If you want to handle your own memory allocation errors, just
 *	"#undef ERROR_EXIT_LINTOK" and define your own.
 */
#define ERROR_EXIT_LINTOK(nelem, size) \
	{ \
		(void) fprintf(stderr, \
			"Memory allocation of %d * %d bytes on line %d of \"%s\" failed.\n", \
			(nelem), CAST(int, (size)), __LINE__, __FILE__); \
		exit(1); \
	}

#define CALLOC_OR_ELSE_LINTOK(ptr, nelem, type) \
	{ \
		if (CALLOC_LINTOK(ptr, (nelem), type) == NULL) \
			ERROR_EXIT_LINTOK((nelem), sizeof(type)); \
	}

#define MALLOC_OR_ELSE_LINTOK(ptr, nelem, type) \
	{ \
		if (MALLOC_LINTOK(ptr, (nelem), type) == NULL) \
			ERROR_EXIT_LINTOK((nelem), sizeof(type)); \
	}

#define REALLOC_OR_ELSE_LINTOK(ptr, nelem, type) \
	{ \
		if (REALLOC_LINTOK(ptr, (nelem), type) == NULL) \
			ERROR_EXIT_LINTOK((nelem), sizeof(type)); \
	}

#define MR_ALLOC_OR_ELSE_LINTOK(ptr, nelem, type) \
	{ \
		if (MR_ALLOC_LINTOK(ptr, (nelem), type) == NULL) \
			ERROR_EXIT_LINTOK((nelem), sizeof(type)); \
	}

/*
 *	It would be nice if the non-lint version of FREE_LINTOK could set
 *	its argument to NULL, but I can find no way to do this that's
 *	syntactically clean, especially if FREE_LINTOK preceeds an "else"
 *	(to ";" or not to ";").  Besides, that's something of a departure
 *	from free()'s runtime behavior.
 */
#ifdef lint
#define FREE_LINTOK(ptr) \
	free(((void) abs(ptr == ptr), ptr = NULL, (genericptr) NULL))
#else
#define FREE_LINTOK(ptr) \
	free((genericptr) ptr)
#endif

/* We could have a "CFREE_LINTOK", but what's the point? */

#define INCLUDED_LINTOK
#endif
