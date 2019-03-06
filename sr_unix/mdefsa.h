/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#define	DOT_CHAR	"."
#define DOTM			".m"
#define DOTOBJ			".o"
#define YDB_IMAGE_NAME		"mumps"
#define YDB_IMAGE_NAMELEN	(SIZEOF(YDB_IMAGE_NAME) - 1)
#define	GTMSECSHR_NAME		"gtmsecshr"
#define GTMSECSHR_NAMELEN	(SIZEOF(GTMSECSHR_NAME) - 1)

#define	ICU_LIBFLAGS		(RTLD_NOW | RTLD_GLOBAL)

<<<<<<< HEAD
#define	ICU_LIBNAME_ROOT	"libicuio"
#define YOTTADB_IMAGE_NAME	"libyottadb.so"
#define	ICU_LIBNAME_EXT		"so"
#define	ICU_LIBNAME		ICU_LIBNAME_ROOT "." ICU_LIBNAME_EXT
=======
#define	ICU_LIBNAME_ROOT		"libicuio"
#if defined(__MVS__)
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
#		define	AIX_SHR_64	"(shr_64.o)"	/* Could also be "(libicuio.so)" */
#		define	ICU_LIBNAME_DEF	ICU_LIBNAME_ROOT "." ICU_LIBNAME_EXT AIX_SHR_64
#		define	LIBRARY_PATH_MAX (GTM_PATH_MAX - SIZEOF(AIX_SHR_64))
#	else
#		define	ICU_LIBNAME_EXT	"so"
#	endif
#endif
#ifndef LIBRARY_PATH_MAX
#define	LIBRARY_PATH_MAX GTM_PATH_MAX
#endif
#define	ICU_LIBNAME	ICU_LIBNAME_ROOT "." ICU_LIBNAME_EXT
>>>>>>> 7a1d2b3e... GT.M V6.3-007

#define	GTM_PLUGIN_FMT_SHORT	"%s/plugin/"
#define	GTM_PLUGIN_FMT_FULL	"%s/plugin/%s"

#define GTM_MAIN_FUNC		"gtm_main"

/* Prefix GT.M callback functions with "gtm_" */
#define GTM_PREFIX(func)	gtm_##func
#define cancel_timer		GTM_PREFIX(cancel_timer)
#define hiber_start		GTM_PREFIX(hiber_start)
#define hiber_start_wait_any	GTM_PREFIX(hiber_start_wait_any)

#endif /* MDEFSA_included */
