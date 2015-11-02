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

/* gtm_fcntl.h - interlude to <fcntl.h> system header file.  */
#ifndef GTM_FCNTLH
#define GTM_FCNTLH

#include <fcntl.h>

#ifndef GTM_FD_TRACE
#	define	CREAT			creat
#	define	OPEN			open
#	define	OPEN3			open
#else
#	define	CREAT			gtm_creat
#	define	OPEN			gtm_open
#	define	OPEN3			gtm_open3
#	undef	open		/* in case this is already defined by <fcntl.h> (at least AIX and HPUX seem to do this) */
#	undef	creat		/* in case this is already defined by <fcntl.h> (at least AIX and HPUX seem to do this) */
#	define	open	gtm_open
#	define	creat	gtm_creat
#endif

int gtm_open(const char *pathname, int flags);
int gtm_open3(const char *pathname, int flags, mode_t mode);
int gtm_creat(const char *pathname, mode_t mode);

#endif
