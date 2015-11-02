/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

#endif
