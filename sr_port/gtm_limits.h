/****************************************************************
 *								*
 * Copyright 2002, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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

/* The value 1023 for PATH_MAX is derived using pathconf("path", _PC_PATH_MAX) on z/OS and
 * we figure other POSIX platforms are at least as capable if they don't define PATH_MAX.
 * Since we can't afford to call a function on each use of PATH_MAX/YDB_PATH_MAX, this
 * value is hardcoded here.
 *
 * Note on Linux (at least), PATH_MAX is actually defined in <sys/param.h>. We would include
 * that here unconditionally but on AIX, param.h includes limits.h. Note that regardless of where
 * it gets defined, PATH_MAX needs to be defined prior to including stdlib.h. This is because in a
 * pro build, at least Linux verifies the 2nd parm of realpath() is PATH_MAX bytes or more.
 * Since param.h sets PATH_MAX to 4K on Linux, this can cause structures defined as YDB_PATH_MAX
 * to raise an error when used in the 2nd argument of realpath().
 */
#ifndef PATH_MAX
#  ifdef __linux__
#    include <sys/param.h>
#  else
#    define PATH_MAX 	1023
#  endif
#endif
/* Now define our version which includes space for a terminating NULL byte */
#define	YDB_PATH_MAX	PATH_MAX + 1
#define GTM_PATH_MAX	YDB_PATH_MAX	/* so defined because GTM_PATH_MAX is required by Java plugin */
/* The maximum path to the YottaDB distribution is complicated by the paths underneath $ydb_dist.
 * At the top level, there is libyottadb.so which is roughly 12 characters plus 1 for the
 * slash. The path length of gtmsecshrdir/gtmsecshr doesn't  come into play because
 * $ydb_dist/gtmsecshr will change directory to $ydb_dist/gtmsecshrdir (13 characters including the
 * leading slash) and then exec gtmsecshr, avoiding the maximum path issue. Going to "UTF-8" mode adds
 * another 5 characters ("utf8/") to the path name. The encryption library path,
 * $ydb_dist/plubin/libgtmcrypt.so is a symlink to some much longer named files which the code will
 * realpath() before dlopen()ing. As it stands, the longest path is 47 characters (including the "UTF-8"
 * directory. Thus PATH_MAX - 50 characters should be a good compromise for today and future expansion.
 * Just in case, the build script verify that nothing past $ydb_dist is more than 50 characters long. */
#define	YDB_DIST_PATH_MAX	YDB_PATH_MAX - 50

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

