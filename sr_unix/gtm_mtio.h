/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* UNIX gtm_mtio.h - File to include the appropriate mag tape files for UNIX.
    We are not supporting mag tape on os390 at this time.
 */

#include <sys/types.h>

#ifdef _SYSTYPE_SVR4
#include <s2/mtio.h>
#else
#ifndef __MVS__
#include <sys/mtio.h>
#endif	/* MVS */
#endif	/* SVR4 */

#include <sys/ioctl.h>


