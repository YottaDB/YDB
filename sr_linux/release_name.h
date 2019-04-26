/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
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
<<<<<<< HEAD

#define GTM_VERSION		"V6.3"
#define	GTM_ZVERSION		"V6.3-007"
#define	YDB_ZYRELEASE		"r1.29"		/* changes here should be reflected in YDB_RELEASE in sr_unix/libyottadb.h */

/* Note: YDB_RELEASE_STAMP is set as part of the cmake build process.
 * Example values are
 *	#define YDB_RELEASE_STAMP	"20180316 15:27"
 */

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
=======
#ifdef __CYGWIN__
#define GTM_RELEASE_NAME 	"GT.M V6.3-008 CYGWIN x86"
#elif defined(__ia64)
#define GTM_RELEASE_NAME 	"GT.M V6.3-008 Linux IA64"
#elif defined(__x86_64__)
#define GTM_RELEASE_NAME 	"GT.M V6.3-008 Linux x86_64"
#elif defined(__s390__)
#define GTM_RELEASE_NAME 	"GT.M V6.3-008 Linux S390X"
#else
#define GTM_RELEASE_NAME 	"GT.M V6.3-008 Linux x86"
#endif
>>>>>>> a6cd7b01f... GT.M V6.3-008
#endif

#define GTM_RELEASE_NAME 	"GT.M" " " GTM_ZVERSION " " YDB_PLATFORM
#define YDB_RELEASE_NAME 	"YottaDB" " " YDB_ZYRELEASE " " YDB_PLATFORM
#define	YDB_AND_GTM_RELEASE_NAME	GTM_RELEASE_NAME " " "YottaDB" " " YDB_ZYRELEASE
#define GTM_PRODUCT 		"GT.M"
<<<<<<< HEAD
#define YDB_PRODUCT		"YottaDB"

#endif
=======
#define GTM_VERSION		"V6.3"
#define GTM_RELEASE_STAMP	"20190417 10:37"
>>>>>>> a6cd7b01f... GT.M V6.3-008
