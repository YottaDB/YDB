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

/* gtm_stdlib.h - interlude to <stdlib.h> system header file.  */
#ifndef GTM_STDLIBH
#define GTM_STDLIBH

#include <stdlib.h>

#define GETENV	getenv
#define ATOI	atoi
#define ATOL	atol
#define ATOF	atof
#define PUTENV	putenv
#define STRTOL	strtol
#define STRTOUL	strtoul
#if defined(__linux__) && !defined(Linux390)
/* only needed until glibc 2.1.3 aka post RH 6.1 */
int gtm_system(const char *);
#define SYSTEM	gtm_system
#else
#define SYSTEM	system
#endif

#endif
