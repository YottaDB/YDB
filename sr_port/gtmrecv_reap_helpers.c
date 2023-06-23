/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#ifdef UNIX
#include <sys/wait.h>
#elif defined(VMS)
#include <descrip.h> /* Required for gtmrecv.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "repl_shutdcode.h"
#include "gtmrecv.h"
#include "is_proc_alive.h"
#include "eintr_wrappers.h"
#include "repl_log.h"

GBLREF recvpool_addrs	recvpool;

void gtmrecv_reap_helpers(boolean_t wait)
{
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	upd_helper_entry_ptr_t	helper, helper_top;
	uint4			helper_pid;
	int			exit_status;
	UNIX_ONLY(pid_t	waitpid_res;)

	upd_helper_ctl = recvpool.upd_helper_ctl;
	for (helper = upd_helper_ctl->helper_list, helper_top = helper + MAX_UPD_HELPERS; helper < helper_top; helper++)
	{
		while (0 != (helper_pid = helper->helper_pid_prev))
		{
			UNIX_ONLY(WAITPID(helper_pid, &exit_status, WNOHANG, waitpid_res);) /* release defunct helper if dead */
			if (UNIX_ONLY(waitpid_res == helper_pid || ) !is_proc_alive(helper_pid, 0))
			{
				helper->helper_pid = 0; /* release entry */
				if (NORMAL_SHUTDOWN == helper->helper_shutdown)
				{ /* zombie has been released (on Unix only). Clean-up the slot */
					helper->helper_pid_prev = 0;
					helper->helper_shutdown = NO_SHUTDOWN;
				} /* else helper shutdown abnormal, continue to report in checkhealth */
				break;
			}
			if (!wait)
				break;
			SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_SHUTDOWN);
		}
	}
	upd_helper_ctl->reap_helpers = HELPER_REAP_NONE;
}
