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

/* gtm_iconv.h - interlude to <iconv.h> system header file.  */
#ifndef GTM_ICONVH
#define GTM_ICONVH

#ifdef USING_ICONV

#define _OSF_SOURCE
#include <iconv.h>
#undef _OSF_SOURCE

#define ICONV_OPEN	iconv_open

#endif

#endif
