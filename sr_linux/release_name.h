/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_RELEASE_NAME
#ifdef __CYGWIN__
#define GTM_RELEASE_NAME 	"GT.M V6.3-014 CYGWIN x86"
#elif defined(__ia64)
#define GTM_RELEASE_NAME 	"GT.M V6.3-014 Linux IA64"
#elif defined(__x86_64__)
#define GTM_RELEASE_NAME 	"GT.M V6.3-014 Linux x86_64"
#elif defined(__s390__)
#define GTM_RELEASE_NAME 	"GT.M V6.3-014 Linux S390X"
#else
#define GTM_RELEASE_NAME 	"GT.M V6.3-014 Linux x86"
#endif
#endif
#define GTM_PRODUCT 		"GT.M"
#define GTM_VERSION		"V6.3"
#define GTM_RELEASE_STAMP	"20200928 19:00"
