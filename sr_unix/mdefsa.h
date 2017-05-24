/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/* DSK_WRITE_NOCACHE macro needs <errno.h> to be included. Use this flavor if writing direct from storage (not cache buffer) */
#define	DSK_WRITE_NOCACHE(reg, blk, ptr, odv, status)		\
MBSTART {							\
	if (-1 == dsk_write_nocache(reg, blk, ptr, odv))	\
		status = errno;					\
	else							\
		status = 0;					\
} MBEND

#define DOTM			".m"
#define DOTOBJ			".o"
#define GTM_DIST		"gtm_dist"
#define GTM_IMAGE_NAME		"mumps"
#define GTM_IMAGE_NAMELEN	(SIZEOF(GTM_IMAGE_NAME) - 1)
#define	GTMSECSHR_NAME		"gtmsecshr"
#define GTMSECSHR_NAMELEN	(SIZEOF(GTMSECSHR_NAME) - 1)

#define	ICU_LIBFLAGS		(RTLD_NOW | RTLD_GLOBAL)

#define	ICU_LIBNAME_ROOT		"libicuio"
#ifdef __hpux
#  ifdef __ia64
#	define GTMSHR_IMAGE_NAME	"libgtmshr.so"
#  else
#	define GTMSHR_IMAGE_NAME	"libgtmshr.sl"
#  endif
#	define	ICU_LIBNAME_EXT		"sl"
#elif defined(__MVS__)
#	define GTMSHR_IMAGE_NAME	"libgtmshr.dll"
#	define	ICU_LIBNAME_EXT		"so"
#elif defined(__CYGWIN__)
#	define GTMSHR_IMAGE_NAME	"libgtmshr.dll"
#	define	ICU_LIBNAME_EXT		"dll"
#else
#	define GTMSHR_IMAGE_NAME	"libgtmshr.so"
#	ifdef _AIX
	/* Conventionally, AIX archives shared objects into a static library.
	 * So we need to link with a member of the library instead of the library itself.
	 */
#		define	ICU_LIBNAME_EXT	"a"
	/* AIX system default ICU library uses a different convention for the library name */
#		define	ICU_LIBNAME_DEF	ICU_LIBNAME_ROOT "." ICU_LIBNAME_EXT "(shr_64.o)"
#	else
#		define	ICU_LIBNAME_EXT	"so"
#	endif
#endif
#define	ICU_LIBNAME	ICU_LIBNAME_ROOT "." ICU_LIBNAME_EXT

#define GTM_MAIN_FUNC		"gtm_main"

/* Prefix GT.M callback functions with "gtm_" */
#define GTM_PREFIX(func)	gtm_##func
#define cancel_timer		GTM_PREFIX(cancel_timer)
#define hiber_start		GTM_PREFIX(hiber_start)
#define hiber_start_wait_any	GTM_PREFIX(hiber_start_wait_any)

#endif /* MDEFSA_included */
