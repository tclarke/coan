/* ::[[ @(#) getopt.c 1.5 89/07/02 00:18:19 ]]:: */
#ifndef LINT
static char sccsid[]="::[[ @(#) getopt.c 1.5 89/07/02 00:18:19 ]]::";
#endif

/*
Checksum:  505161377      (check or update this with "brik")
*/

/*
 * Here's something you've all been waiting for:  the AT&T public domain
 * source for getopt(3).  It is the code which was given out at the 1985
 * UNIFORUM conference in Dallas.  I obtained it by electronic mail
 * directly from AT&T.  The people there assure me that it is indeed
 * in the public domain.
 *
 * There is no manual page.  That is because the one they gave out at
 * UNIFORUM was slightly different from the current System V Release 2
 * manual page.  The difference apparently involved a note about the
 * famous rules 5 and 6, recommending using white space between an option
 * and its first argument, and not grouping options that have arguments.
 * Getopt itself is currently lenient about both of these things White
 * space is allowed, but not mandatory, and the last option in a group can
 * have an argument.  That particular version of the man page evidently
 * has no official existence, and my source at AT&T did not send a copy.
 * The current SVR2 man page reflects the actual behavor of this getopt.
 * However, I am not about to post a copy of anything licensed by AT&T.
 */

/*
Minor modifications by Rahul Dhesi 1989/03/06
*/

#include <stdio.h>
#define PARMS(X) X

#ifdef STDINCLUDE
# include <string.h>
#else
extern int strcmp PARMS ((char *, char *));
extern char *strchr PARMS ((char *, char));
#endif

/* Avoid possible compiler warning if we simply redefine NULL or EOF */
#define XNULL   0
#define XEOF (-1)

#define ERR(szz,czz) if(opterr){fprintf(stderr,"%s%s%c\n",argv[0],szz,czz);}

int   opterr = 1;
int   optind = 1;
int   optopt;
char  *optarg;

int
getopt(argc, argv, opts)
int   argc;
char  **argv, *opts;
{
   static int sp = 1;
   register int c;
   register char *cp;

   if(sp == 1)
      if(optind >= argc ||
         argv[optind][0] != '-' || argv[optind][1] == '\0')
         return(XEOF);
      else if(strcmp(argv[optind], "--") == XNULL) {
         optind++;
         return(XEOF);
      }
   optopt = c = argv[optind][sp];
   if(c == ':' || (cp=strchr(opts, c)) == XNULL) {
      ERR(": illegal option -- ", c);
      if(argv[optind][++sp] == '\0') {
         optind++;
         sp = 1;
      }
      return('?');
   }
   if(*++cp == ':') {
      if(argv[optind][sp+1] != '\0')
         optarg = &argv[optind++][sp+1];
      else if(++optind >= argc) {
         ERR(": option requires an argument -- ", c);
         sp = 1;
         return('?');
      } else
         optarg = argv[optind++];
      sp = 1;
   } else {
      if(argv[optind][++sp] == '\0') {
         sp = 1;
         optind++;
      }
      optarg = XNULL;
   }
   return(c);
}
