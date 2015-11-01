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

/* gtm_utsname.h - interlude to <utsname.h> system header file.  */
#ifndef GTM_UTSNAMEH
#define GTM_UTSNAMEH

#include <sys/utsname.h>

#define UNAME(name,uname_res) (uname_res = uname(name))

#endif
