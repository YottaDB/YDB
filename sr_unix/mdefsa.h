/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
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

#define	ICU_LIBNAME_ROOT	"libicuio"
#define YOTTADB_IMAGE_NAME	"libyottadb.so"
#define	ICU_LIBNAME_EXT		"so"
#ifndef LIBRARY_PATH_MAX
#define	LIBRARY_PATH_MAX GTM_PATH_MAX
#endif
#define	ICU_LIBNAME		ICU_LIBNAME_ROOT "." ICU_LIBNAME_EXT

#define	GTM_PLUGIN_FMT_SHORT	"%s/plugin/"
#define	GTM_PLUGIN_FMT_FULL	"%s/plugin/%s"

#define GTM_MAIN_FUNC		"gtm_main"

/* Prefix GT.M callback functions with "gtm_" */
#define cancel_timer		gtm_cancel_timer
#define hiber_start		gtm_hiber_start
#define hiber_start_wait_any	gtm_hiber_start_wait_any

#endif /* MDEFSA_included */
