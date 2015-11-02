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

int gtmrecv_end_helpers(boolean_t is_rcvr_srvr)
{ /* Set flag in recvpool telling the receiver server to stop all reader and writer helpers.
   * Wait for receiver server to complete the processs - all processes shut down, or some error occurred
   */
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	upd_helper_entry_ptr_t	helper, helper_top;

	repl_log(stdout, TRUE, TRUE, "Initiating shut down of Helpers\n");
	upd_helper_ctl = recvpool.upd_helper_ctl;
	for (helper = upd_helper_ctl->helper_list, helper_top = helper + MAX_UPD_HELPERS; helper < helper_top; helper++)
		helper->helper_shutdown = SHUTDOWN;  /* indicate to the helper to shut down */
	if (is_rcvr_srvr)
		gtmrecv_reap_helpers(TRUE);
	else
	{
		upd_helper_ctl->reap_helpers = HELPER_REAP_WAIT;
		while (HELPER_REAP_NONE != upd_helper_ctl->reap_helpers && SRV_ALIVE == is_recv_srv_alive())
			SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_SHUTDOWN);
		upd_helper_ctl->reap_helpers = HELPER_REAP_NONE;
	}
	return NORMAL_SHUTDOWN;
}
