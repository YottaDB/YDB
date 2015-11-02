/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#	define	CREAT	creat
#	define	OPEN	open
#	define	OPEN3	open
#else
/* Note we no longer redefine open to gtm_open because of the problems it creates requiring includes to be specified in
 * a specific order (anything that includes this include must come BEFORE gdsfhead to avoid errors). This ordering was
 * nearly impossible when trace flags were specified in error.h or gtm_trigger_src.h. At the time of this removal (10/2012),
 * a search was completed to verify no open() calls existed that needed this support but that does not prevent new calls from
 * being added. Therefore, attentiveness is required.
 */
#	define	CREAT	gtm_creat
#	define	OPEN	gtm_open
#	define	OPEN3	gtm_open3
#	undef	creat		/* in case this is already defined by <fcntl.h> (at least AIX and HPUX seem to do this) */
#	define	creat	gtm_creat
#endif

int gtm_open(const char *pathname, int flags);
int gtm_open3(const char *pathname, int flags, mode_t mode);
int gtm_creat(const char *pathname, mode_t mode);

#endif
