/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifdef __CYGWIN__
#define GTM_RELEASE_NAME 	"GT.M V6.0-003 CYGWIN x86"
#elif defined(__ia64)
#define GTM_RELEASE_NAME 	"GT.M V6.0-003 Linux IA64"
#elif defined(__x86_64__)
#define GTM_RELEASE_NAME 	"GT.M V6.0-003 Linux x86_64"
#elif defined(__s390__)
#define GTM_RELEASE_NAME 	"GT.M V6.0-003 Linux S390X"
#else
#define GTM_RELEASE_NAME 	"GT.M V6.0-003 Linux x86"
#endif
#define GTM_PRODUCT 		"GT.M"
#define GTM_VERSION		"V6.0"
