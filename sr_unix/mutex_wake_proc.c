/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_socket.h"
#include "gtm_string.h"
#include <sys/un.h>
#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mutex.h"
#include "eintr_wrappers.h"
#include "is_proc_alive.h"
#include "send_msg.h"
#include "gtmsecshr.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#endif

error_def(ERR_MUTEXERR);
error_def(ERR_TEXT);
error_def(ERR_SYSCALL);

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

	unsigned char   	mutex_wake_this_proc_str[2 * SIZEOF(pid_t) + 1];
	mutex_wake_msg_t	msg;
	int			status;
	ssize_t			sendto_res;
	static int		sendto_fail_pid;
	char			sendtomsg[256];

	/* Set up the socket structure for sending */
	strcpy(mutex_wake_this_proc.sun_path + mutex_wake_this_proc_prefix_len,
	       (char *)pid2ascx(mutex_wake_this_proc_str, *pid));
	msg.pid = process_id;
	msg.mutex_wake_instance = mutex_wake_instance;
#	ifdef DEBUG
	if (gtm_white_box_test_case_enabled
		&& (WBTEST_SENDTO_EPERM == gtm_white_box_test_case_number))
	{
		FPRINTF(stderr, "PATH TO SOCKET IS\n%s\n", mutex_wake_this_proc.sun_path);
		LONG_SLEEP(20);
	}
#	endif
	/* We have seen an issue where the sendto() call done below blocked for at least more than a minute. The only reason
	 * we know of this can happen is if the TCPIP buffer on the receiving end of the pipe is already full. But as long as
	 * the receiving side is waiting for this wakeup message, it should be in a loop clearing the incoming messages thereby
	 * avoiding this situation. But in reality, we have seen the receiving side not waiting for the wakeup signal at all
	 * but instead doing something totally different effectively causing a deadlock. One approach to fix this issue is to
	 * open the mutex socket with the O_NONBLOCK parameter that way the sendto() done below would be a non-blocking send
	 * but that introduces issues in the mutex logic as it then needs to handle partial messages (this is because a
	 * non-blocking sendto would send as much bytes as possible before things would block). For now, the only platform
	 * that use this wakeup scheme is Linux and that is almost transitioning to using memory-semaphores. The blocking
	 * sendto() issue is therefore not considered critical at this moment. This might need to be revisited in case this
	 * code starts to get used again. -- nars - 2008/01/15. */
	SENDTO_SOCK(mutex_sock_fd, (char *)&msg, SIZEOF(msg), 0, (struct sockaddr *)&mutex_wake_this_proc,
		mutex_wake_this_proc_len, sendto_res);
	if (0 > sendto_res)
	{	/* Sending wakeup to the mutex socket file of the waiting pid can fail if the process terminated (and hence deleted
		 * its mutex socket file) while waiting for crit. Except for that case, signal an error in case wakeup send fails.
		 */
		status = errno;
		assert(0 != *pid);
		/* if the other process could not be woken up with SENDTO_SOCK due to permissions issue,
		 * try continue_proc() before erroring out */
		if (EACCES == status)
			continue_proc(*pid);
#		ifdef DEBUG
		if (gtm_white_box_test_case_enabled
			&& (WBTEST_SENDTO_EPERM == gtm_white_box_test_case_number))
		{
			FPRINTF(stderr, "CALLED CONTINUE_PROC() ON THE OTHER PROCESS\n");
			LONG_SLEEP(20);
		}
#		endif
		/* check if the process is still hung; if so, signal an error */
		if ((sendto_fail_pid == *pid) && is_proc_alive(*pid, 0))
		{
			SNPRINTF(sendtomsg, ARRAYSIZE(sendtomsg), "sendto() to pid [%d]", *pid);
			send_msg(VARLSTCNT(10) ERR_MUTEXERR, 0, ERR_SYSCALL, 5, LEN_AND_STR(sendtomsg), CALLFROM, status);
			assert(FALSE);
		}
		sendto_fail_pid = *pid;
	}
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
	{
		assert(FALSE);
		rts_error(VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
			  RTS_ERROR_TEXT("Error with msem_unlock()/sem_post()"), errno);
	}
	return;
}

#endif
