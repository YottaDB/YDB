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

/* gtm_statvfs.h - interlude to <sys/statvfs.h> system header file.  */
#ifndef GTM_STATVFSH
#define GTM_STATVFSH

#include <sys/statvfs.h>

#define STATVFS(pathname,fsinfo,statvfs_res) (statvfs_res = statvfs(pathname, fsinfo))
#define FSTATVFS(filedesc,fstatvfsinfo,fstatvfs_res) 		\
{								\
	DEFER_INTERRUPTS(INTRPT_IN_FSTAT);			\
	fstatvfs_res = fstatvfs(filedesc, fstatvfsinfo);	\
	ENABLE_INTERRUPTS(INTRPT_IN_FSTAT);			\
}

#endif
