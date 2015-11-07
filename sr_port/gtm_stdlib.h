/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#define GETENV	getenv
#else
char *gtm_getenv(char *varname);
#define GETENV gtm_getenv
#endif
#define ATOI	atoi
#define ATOL	atol
#define ATOF	atof
#define PUTENV	putenv
#define STRTOL	strtol
#define STRTOLL	strtoll
#define STRTOUL	strtoul
#if INT_MAX < LONG_MAX	/* like Tru64 */
#define STRTO64L	strtol
#define STRTOU64L	strtoul
#elif defined(__hpux)
#include <inttypes.h>
#define STRTO64L	strtoimax
#define STRTOU64L	strtoumax
#else
#define STRTO64L	strtoll
#define STRTOU64L	strtoull
#endif
#define MKSTEMP(template,mkstemp_res)	(mkstemp_res = mkstemp(template))
#ifdef VMS
#define SYSTEM system
#else
#define SYSTEM	gtm_system
int gtm_system(const char *cmdline);
#endif

#endif
