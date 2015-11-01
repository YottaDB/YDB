/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MDEFSA_included
#define MDEFSA_included

/* Declarations common to all unix mdefsp.h, to be moved here */

/* DSK_WRITE Macro needs <errno.h> to be included. */
#define	DSK_WRITE(reg, blk, ptr, status)			\
{								\
	if (-1 == dsk_write(reg, blk, ptr))			\
		status = errno;					\
	else							\
		status = 0;					\
}

#define DOTM			".m"
#define DOTOBJ			".o"
#define GTM_DIST		"gtm_dist"
#define GTM_IMAGE_NAME		"mumps"
#define GTM_IMAGE_NAMELEN	(sizeof(GTM_IMAGE_NAME) - 1)
#define	DIR_SEPARATOR		'/'
#define	GTMSECSHR_NAME		"gtmsecshr"
#define GTMSECSHR_NAMELEN	(sizeof(GTMSECSHR_NAME) - 1)

#ifdef __hpux
#define GTMSHR_IMAGE_NAME	"libgtmshr.sl"
#elif defined(__MVS__)
#define GTMSHR_IMAGE_NAME	"libgtmshr.dll"
#else
#define GTMSHR_IMAGE_NAME	"libgtmshr.so"
#endif

#define GTM_MAIN_FUNC		"gtm_main"

/* Prefix GT.M callback functions with "gtm_" */
#define GTM_PREFIX(func)	gtm_##func
#define cancel_timer		GTM_PREFIX(cancel_timer)
#define hiber_start		GTM_PREFIX(hiber_start)
#define hiber_start_wait_any	GTM_PREFIX(hiber_start_wait_any)
#define start_timer		GTM_PREFIX(start_timer)
#define jnlpool_detach		GTM_PREFIX(jnlpool_detach)

#endif /* MDEFSA_included */
