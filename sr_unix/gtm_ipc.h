/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Serivces, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_ipc.h - interlude to <ipc.h> system header file.  */
#ifndef GTM_IPCH
#define GTM_IPCH

#include <sys/ipc.h>

#ifdef __MVS__
/* For shmget with __IPC_MEGA or _LP64 */
#define MEGA_BOUND    (1024 * 1024)
#endif

#define FTOK	ftok

#endif
