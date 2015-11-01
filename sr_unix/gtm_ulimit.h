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

/* Interlude to <ulimit.h> */

#if defined(sun) && !defined(__SVR4) 	/* no <ulimit.h> on SunOS 4.1.x */
#	define UL_GETFSIZE 1
#else
#	include <ulimit.h>
#endif
