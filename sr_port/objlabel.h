/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* objlabel.h -- define OMAGIC and OBJ_LABEL here.  */

#define	OMAGIC	0407		/* old impure format */

/* The Object file label is composed of a platform-generic part and platform-specific part.
 *
 * 	OBJ_LABEL = (OBJ_UNIX_LABEL << 8) + OBJ_PLATFORM_LABEL
 *
 * For every object format change that spans across all platforms, we increment the platform-generic part OBJ_UNIX_LABEL.
 * For every platform-specific object format change, we increment the platform-specific part OBJ_PLATFORM_LABEL
 * 	(only on the platform of change)
 *
 * Note that OBJ_UNIX_LABEL and OBJ_PLATFORM_LABEL should not exceed 255.
 */

#define OBJ_UNIX_LABEL	3

#if defined(__osf__)
#	define	OBJ_PLATFORM_LABEL	0
#elif defined(_AIX)
#	define	OBJ_PLATFORM_LABEL	0
#elif defined(__linux__) && defined(Linux390)
#	define	OBJ_PLATFORM_LABEL	0		/* s390 Linux */
#elif defined(__linux__) && !defined(Linux390)
#	define	OBJ_PLATFORM_LABEL	0		/* i386 Linux */
#elif defined(__MVS__)
#	define	OBJ_PLATFORM_LABEL	0		/* os390 */
#elif defined(__hpux)
#	define	OBJ_PLATFORM_LABEL	0
#elif defined(SUNOS)
#	define	OBJ_PLATFORM_LABEL	0
#elif defined(VMS)
#	define OBJ_PLATFORM_LABEL	0
#else
#error UNSUPPORTED PLATFORM
#endif

#define OBJ_LABEL	((OBJ_UNIX_LABEL << 8) + (OBJ_PLATFORM_LABEL))
