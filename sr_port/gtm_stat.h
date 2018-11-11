/****************************************************************
 *								*
 * Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

/* gtm_stat.h - interlude to <sys/stat.h> system header file.  */
#ifndef GTM_STATH
#define GTM_STATH

#include <sys/stat.h>


#define CHMOD	chmod

#define FCHMOD	fchmod

#define MKDIR	mkdir

#define Stat	stat

#define MKNOD	mknod

#define LSTAT   lstat

/* Returns TRUE if STAT1 modification time is older than STAT2 modification time */
#define	IS_STAT1_MTIME_OLDER_THAN_STAT2(STAT1, STAT2) ((STAT1.st_mtim.tv_sec < STAT2.st_mtim.tv_sec)			\
							|| ((STAT1.st_mtim.tv_sec == STAT2.st_mtim.tv_sec)		\
								&& (STAT1.st_mtim.tv_nsec < STAT2.st_mtim.tv_nsec)))

#define	IS_STAT1_MTIME_EQUAL_TO_STAT2(STAT1, STAT2) ((STAT1.st_mtim.tv_sec == STAT2.st_mtim.tv_sec)			\
								&& (STAT1.st_mtim.tv_nsec == STAT2.st_mtim.tv_nsec))

#endif
