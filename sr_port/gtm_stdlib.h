/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_stdlib.h - interlude to <stdlib.h> system header file.  */
#ifndef GTM_STDLIBH
#define GTM_STDLIBH

#include <stdlib.h>

#ifndef __CYGWIN__
#  define GETENV	getenv
#else
char *gtm_getenv(char *varname);
#  define GETENV 	gtm_getenv
#endif
#define ATOI		atoi
#define ATOL		atol
#define ATOF		atof
#ifdef UNIX
/* If interrupted, this function has previously caused hangs to do a subsequent syslog() invocation from generic_signal_handler(),
 * so just defer interrupts to be safe. UNIX is a GT.M-specific compiler switch, which we expect to be undefined for any non-GT.M
 * compilation that might include this file.
 */
#  define PUTENV(VAR, ARG)				\
{							\
	DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC);	\
	VAR = putenv(ARG);				\
	ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC);	\
}
#else
#  define PUTENV(VAR, ARG)				\
{							\
	VAR = putenv(ARG);				\
}
#endif
#define STRTOL		strtol
#define STRTOLL		strtoll
#define STRTOUL		strtoul
#if INT_MAX < LONG_MAX	/* like Tru64 */
#  define STRTO64L	strtol
#  define STRTOU64L	strtoul
#elif defined(__hpux)
#  include <inttypes.h>
#  define STRTO64L	strtoimax
#  define STRTOU64L	strtoumax
#else
#  define STRTO64L	strtoll
#  define STRTOU64L	strtoull
#endif
#define MKSTEMP(template,mkstemp_res)	(mkstemp_res = mkstemp(template))
#if defined(VMS) || defined(STATIC_ANALYSIS)
#  define SYSTEM	system
#else
#  define SYSTEM	gtm_system
int gtm_system(const char *cmdline);
#endif

#endif
