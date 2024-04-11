/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 * Copyright (c) 2017-2018 Stephen L Johnson.			*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_RELEASE_NAME

#define GTM_VERSION		"V7.0"
#define	GTM_ZVERSION		"V7.0-005"
#define	YDB_ZYRELEASE		"r2.01"		/* changes here should be reflected in YDB_RELEASE in sr_unix/libyottadb.h */

/* This sets YDB_RELEASE_STAMP as part of the cmake build process.
 * Example values are
 *	#define YDB_RELEASE_STAMP	"20200731 20:46 6996f60301958081b568fb2a804695d700f51c5e (dirty)"
 */
# include "release_commit.h"

#if defined(__x86_64__)
# define YDB_PLATFORM		"Linux x86_64"
#elif defined(__armv6l__)
# define YDB_PLATFORM		"Linux armv6l"
#elif defined(__armv7l__)
# define YDB_PLATFORM		"Linux armv7l"
#elif defined(__aarch64__)
# define YDB_PLATFORM		"Linux aarch64"
#else
# define YDB_PLATFORM		"Linux x86"
#endif

#define GTM_RELEASE_NAME 	"GT.M" " " GTM_ZVERSION " " YDB_PLATFORM
#define YDB_RELEASE_NAME 	"YottaDB" " " YDB_ZYRELEASE " " YDB_PLATFORM
#define	YDB_AND_GTM_RELEASE_NAME	GTM_RELEASE_NAME " " "YottaDB" " " YDB_ZYRELEASE
#define GTM_PRODUCT 		"GT.M"
#define YDB_PRODUCT		"YottaDB"

#endif
