/****************************************************************
 *								*
 *	Copyright 2002, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Interlude to <limits.h> */

#ifndef GTM_LIMITSH
#define GTM_LIMITSH

#include <limits.h>
#ifdef __hpux
#include <inttypes.h>
#endif

/* The value 1023 for PATH_MAX is derived using pathconf("path", _PC_PATH_MAX) on z/OS.
 * Since we cant afford calling a function on each use of PATH_MAX/GTM_PATH_MAX,
 * this value is hardcoded here.
 */
#if defined (__MVS__)
#ifndef PATH_MAX
#define PATH_MAX 	1023
#endif
#define	GTM_PATH_MAX	PATH_MAX + 1
#else
#define GTM_PATH_MAX	1024	/* includes terminating NULL */
#endif

#if defined(LLONG_MAX)		/* C99 and others */
#define GTM_INT64_MIN LLONG_MIN
#define GTM_INT64_MAX LLONG_MAX
#define GTM_UINT64_MAX ULLONG_MAX
#elif defined(LONG_LONG_MAX)
#define GTM_INT64_MIN LONG_LONG_MIN
#define GTM_INT64_MAX LONG_LONG_MAX
#define GTM_UINT64_MAX ULONG_LONG_MAX
#elif defined(LONGLONG_MAX)
#define GTM_INT64_MIN LONGLONG_MIN
#define GTM_INT64_MAX LONGLONG_MAX
#define GTM_UINT64_MAX ULONGLONG_MAX
#elif defined(__INT64_MAX)	/* OpenVMS Alpha */
#define GTM_INT64_MIN __INT64_MIN
#define GTM_INT64_MAX __INT64_MAX
#define GTM_UINT64_MAX __UINT64_MAX
#elif defined(INTMAX_MAX)	/* HP-UX */
#define GTM_INT64_MIN INTMAX_MIN
#define GTM_INT64_MAX INTMAX_MAX
#define GTM_UINT64_MAX UINTMAX_MAX
#elif LONG_MAX != INT_MAX	/* Tru64 */
#define GTM_INT64_MIN LONG_MIN
#define GTM_INT64_MAX LONG_MAX
#define GTM_UINT64_MAX ULONG_MAX
#else
#error Unable to determine 64 bit MAX in gtm_limits.h
#endif

#endif

