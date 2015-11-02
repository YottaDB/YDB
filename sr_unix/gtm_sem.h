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

#ifndef GTM_SEM_INCLUDED
#define GTM_SEM_INCLUDED

#define FTOK_SEM_PER_ID 3


union   semun {
	int     val;
	struct  semid_ds *buf;
	u_short *array;
#if defined(__linux__) && (__ia64)
	struct seminfo *__buf;		/* buffer for IPC_INFO */
	void *__pad;
#endif
} arg;

#endif /* GTM_SEM_INCLUDED */
