/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_statvfs.h - interlude to <sys/statvfs.h> system header file.  */
#ifndef GTM_STATVFSH
#define GTM_STATVFSH

#include <sys/statvfs.h>

#define STATVFS(pathname,fsinfo,statvfs_res) (statvfs_res = statvfs(pathname, fsinfo))
#define FSTATVFS(filedesc,fstatvfsinfo,fstatvfs_res) (fstatvfs_res = fstatvfs(filedesc, fstatvfsinfo))

#if (defined(__osf__) && defined(__alpha)) || defined(__ia64)
#define	GTM_BAVAIL_TYPE	unsigned long
#elif defined(__linux__) && defined(__USE_FILE_OFFSET64)
#define GTM_BAVAIL_TYPE unsigned long long int
#else
#define GTM_BAVAIL_TYPE	uint4
#endif

#endif
