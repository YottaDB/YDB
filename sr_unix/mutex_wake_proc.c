/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_socket.h"
#include <sys/un.h>
#include <string.h>
#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mutex.h"
#include "eintr_wrappers.h"

#ifndef MUTEX_MSEM_WAKE

GBLREF int			mutex_sock_fd;
GBLREF struct sockaddr_un	mutex_wake_this_proc;
GBLREF int 			mutex_wake_this_proc_len;
GBLREF int 			mutex_wake_this_proc_prefix_len;
GBLREF uint4			process_id;

void
mutex_wake_proc(sm_int_ptr_t pid, int mutex_wake_instance)
{
	/*
	 * Wakeup process by sending a message over waiting process's socket.
	 * The waiting process (in select) is woken up on sensing input on its
	 * socket. The message is not relevant, a character will achieve the
	 * objective. But, we will send the waking process's pid which might
	 * be of use for debugging.
	 */

	unsigned char   mutex_wake_this_proc_str[2 * sizeof(pid_t) + 1];
	mutex_wake_msg_t	msg;
	int		sendto_res;

	/* Set up the socket structure for sending */
	strcpy(mutex_wake_this_proc.sun_path + mutex_wake_this_proc_prefix_len,
	       (char *)pid2ascx(mutex_wake_this_proc_str, *pid));
	msg.pid = process_id;
	msg.mutex_wake_instance = mutex_wake_instance;
	SENDTO_SOCK(mutex_sock_fd, (char *)&msg, sizeof(msg), 0, (struct sockaddr *)&mutex_wake_this_proc,
		mutex_wake_this_proc_len, sendto_res);
	return;
}

#else

void
#ifdef POSIX_MSEM
mutex_wake_proc(sem_t *mutex_wake_msem_ptr)
#else
mutex_wake_proc(msemaphore *mutex_wake_msem_ptr)
#endif
{
	/* Unlock the memsem to wake the proc waiting on it */
	int	rc;

	error_def(ERR_TEXT);

	/*
	 * CAUTION : man pages on beowulf and hrothgar do not
	 * mention anything about msem_unlock being interrupted.
	 * It is being assumed here that msem_unlock, if interrupted
	 * returns -1 and sets errno to EINTR. If the behavior is
	 * undefined when interrupted, processes waiting to be woken
	 * up may hang, and WE ARE TOAST!!!
	 */

	/*
	 * Additonal note: this was converted to an EINTR wrapper macro.
	 */

	do
	{
		rc = MSEM_UNLOCK(mutex_wake_msem_ptr);
	} while (-1 == rc && EINTR == errno);
	if (0 > rc)
		rts_error(VARLSTCNT(5) ERR_TEXT, 2,
			  RTS_ERROR_TEXT("Mutual Exclusion subsytem : Error with msem_unlock/sem_post"), errno);
	return;
}

#endif
