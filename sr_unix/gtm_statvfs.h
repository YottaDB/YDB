/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
	intrpt_state_t		prev_intrpt_state;		\
								\
	DEFER_INTERRUPTS(INTRPT_IN_FSTAT, prev_intrpt_state);	\
	fstatvfs_res = fstatvfs(filedesc, fstatvfsinfo);	\
	ENABLE_INTERRUPTS(INTRPT_IN_FSTAT, prev_intrpt_state);	\
}

#endif
