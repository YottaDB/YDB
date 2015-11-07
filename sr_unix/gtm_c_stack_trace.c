/****************************************************************
*								*
*	Copyright 2012, 2013 Fidelity Information Services, Inc	*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <sys/un.h>
#include <signal.h>
#include <sys/wait.h>
#include "gtm_stdio.h"	/* For SPRINTF */
#include "gtm_string.h"
#include "send_msg.h"
#include "wbox_test_init.h"
#include "gt_timer.h"
#include "gtm_logicals.h"
#include "trans_log_name.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtm_c_stack_trace.h"
#include "jobsp.h"		/* for MAX_PIDSTR_LEN */
#include "gtm_limits.h"

error_def(ERR_STUCKACT);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

/* This looks up the environment variable gtm_procstuckexec, adds the calling information to it, passes it to a SYSTEM call
 * and checks the returns from both the system and the invoked shell command
 */
void gtm_c_stack_trace(char *message, pid_t waiting_pid, pid_t blocking_pid, uint4 count)
{
	int4		messagelen, arr_len;
	char 	 	*command;
	char		*currpos;
	int		save_errno;
	mstr		envvar_logical, trans;
	char		buf[GTM_PATH_MAX];
	int		status;
#	ifdef _BSD
	union wait      wait_stat;
#	else
	int4            wait_stat;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	messagelen = STRLEN(message);
	assert(SIZEOF(count) <= SIZEOF(pid_t));
	arr_len = GTM_MAX_DIR_LEN + messagelen + (3 * MAX_PIDSTR_LEN) + 5;	/* 4 spaces and a terminator */
	if (!(TREF(gtm_waitstuck_script)).len)
	{	/* uninitialized buffer - translate logical and move it to the buffer */
		envvar_logical.addr = GTM_PROCSTUCKEXEC;
		envvar_logical.len = SIZEOF(GTM_PROCSTUCKEXEC) - 1;
		if (SS_NORMAL == (status = TRANS_LOG_NAME(&envvar_logical, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
		{	/* the environmental variable is defined */
			assert(SIZEOF(buf) > trans.len);
			if (0 != trans.len)
			{	/* and it has a value - stick the length of the translation in char_len of mstr */
				(TREF(gtm_waitstuck_script)).len = trans.len + arr_len;
				(TREF(gtm_waitstuck_script)).addr
					= (char *)malloc((TREF(gtm_waitstuck_script)).len);
				memcpy((TREF(gtm_waitstuck_script)).addr, trans.addr, trans.len);
				*(char *)((TREF(gtm_waitstuck_script)).addr + trans.len) = ' ';
				trans.len += 1;
				(TREF(gtm_waitstuck_script)).char_len = trans.len;	/* abuse of mstr to hold second length */
			}
		}
	} else
	{	/* already have a pointer to the shell command  get its length */
		trans.len = (TREF(gtm_waitstuck_script)).char_len;
		assert(0 < trans.len);
		if ((trans.len + arr_len) > (TREF(gtm_waitstuck_script)).len)
		{	/* new message doesn't fit - malloc fresh space and free the old */
			(TREF(gtm_waitstuck_script)).len = trans.len + arr_len;
			trans.addr = (char *)malloc((TREF(gtm_waitstuck_script)).len);
			memcpy(trans.addr, (TREF(gtm_waitstuck_script)).addr, trans.len);
			free((TREF(gtm_waitstuck_script)).addr);
			(TREF(gtm_waitstuck_script)).addr = trans.addr;
		}
	}
	if (0 != (TREF(gtm_waitstuck_script)).len)
	{	/* have a command and a message */
		command = (TREF(gtm_waitstuck_script)).addr;
		currpos = command + trans.len;
		memcpy(currpos, message, messagelen);
		currpos += messagelen;
		*currpos++ = ' ';
		currpos = (char *)i2asc((unsigned char*)currpos, (unsigned int)waiting_pid);
		*currpos++ = ' ';
		currpos = (char *)i2asc((unsigned char*)currpos, (unsigned int)blocking_pid);
		*currpos++ = ' ';
		currpos = (char *)i2asc((unsigned char*)currpos, (unsigned int)count);
		*currpos++ = 0;
		assert(currpos - (TREF(gtm_waitstuck_script)).addr <= (TREF(gtm_waitstuck_script)).len);
		status = SYSTEM(((char *)((TREF(gtm_waitstuck_script)).addr)));
		if (-1 == status)
		{	/* SYSTEM failed */
			save_errno = errno;
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_STUCKACT, 4, LEN_AND_LIT("FAILURE"), LEN_AND_STR(command));
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("system"), CALLFROM, save_errno);
		} else
		{	/* check on how the command did */
			assert(SIZEOF(wait_stat) == SIZEOF(int4));
#			ifdef _BSD
			wait_stat.w_status = status;
#			else
			wait_stat = status;
#			endif
			if (WIFEXITED(wait_stat))
			{
				status = WEXITSTATUS(wait_stat);
				if (!status)
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_STUCKACT, 4,
							LEN_AND_LIT("SUCCESS"), LEN_AND_STR(command));
				else
				{
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_STUCKACT, 4,
							LEN_AND_LIT("FAILURE"), LEN_AND_STR(command));
					if (WIFSIGNALED(wait_stat))
					{
						status = WTERMSIG(wait_stat);
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							LEN_AND_LIT("PROCSTUCK terminated by signal"), CALLFROM, status);
					} else
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							LEN_AND_LIT("PROCSTUCK"), CALLFROM, status);
				}
			} else
			{	/* it's gone rogue' */
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_STUCKACT, 4,
						LEN_AND_LIT("FAILURE"), LEN_AND_STR(command));
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
						LEN_AND_LIT("PROCSTUCK did not report status"), CALLFROM, status);
				assert(FALSE);
			}
		}
	}
}
