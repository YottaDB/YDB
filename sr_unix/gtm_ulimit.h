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

/* Interlude to <ulimit.h> */

#if !defined(__CYGWIN__) && !(defined(sun) && !defined(__SVR4)) 	/* no <ulimit.h> on Cygwin or SunOS 4.1.x */
#	include <ulimit.h>
#endif
