/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* do_msgsnd.c - This function calls the UNIX system call msgsnd(),
 * restarting the call if it is interrupted (errno == EINTR).
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include "eintr_wrappers.h"
#include "do_msgsnd.h"
#include "record_msg.h"

int	do_msgsnd (int msqid, struct msgbuf *msgp, int msgsz, int msgflg)
{
	int	rv;

#ifdef DEBUG
	record_msg (msqid, msgp, msgsz, "do_msgsnd");
#endif

	MSGSND(msqid, msgp, msgsz, msgflg, rv);

	return rv;
}

