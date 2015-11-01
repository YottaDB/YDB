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

/* gtm_pwd.h - interlude to <pwd.h> system header file.  */
#ifndef GTM_PWDH
#define GTM_PWDH

#include <pwd.h>

#define GETPWUID(uid,getpwuid_res) (getpwuid_res = getpwuid(uid))

#endif
