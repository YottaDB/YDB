/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_pwd.h - interlude to <pwd.h> system header file.  */
#ifndef GTM_PWDH
#define GTM_PWDH

#include <pwd.h>

#define GETPWUID(uid,getpwuid_res) (getpwuid_res = getpwuid(uid))

/* #define to be gtm_getpwuid to serve as a wrapper for blocking signals since getpwuid is not signal-safe. */
#define	getpwuid	gtm_getpwuid
struct passwd	*gtm_getpwuid(uid_t uid);	/* Define prototype of "gtm_getpwuid" here */

#endif
