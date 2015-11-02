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
#define GTM_RELEASE_NAME 	"GT.M V5.3-000 CYGWIN x86"
#elif defined(__ia64)
#define GTM_RELEASE_NAME 	"GT.M V5.3-000 Linux IA64"
#elif defined(__s390__)
#define GTM_RELEASE_NAME 	"GT.M V5.3-000 Linux S390"
#else
#define GTM_RELEASE_NAME 	"GT.M V5.3-000 Linux x86"
#endif
#define GTM_PRODUCT 		"GT.M"
#define GTM_VERSION		"V5.3"
