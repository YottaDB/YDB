/****************************************************************
 *								*
 *	Copyright 2005, 2009 Fidelity Information Services, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#if defined(__MVS__) && !defined(_ISOC99_SOURCE)
#define _ISOC99_SOURCE
#endif

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#ifdef VMS
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
#include "gtmmsg.h"
#include "is_proc_alive.h"
#include "memcoherency.h"

GBLREF recvpool_addrs		recvpool;

int gtmrecv_start_helpers(int n_readers, int n_writers)
{ /* Set flag in recvpool telling the receiver server to start n_readers and n_writers helper processes.
   * Wait for receiver server to complete the process - completed successfully, or terminated with error
   */
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	upd_helper_entry_ptr_t	helper, helper_top;
	char			err_str[BUFSIZ];
	int			avail_slots, started_readers, started_writers;

	error_def(ERR_REPLERR);
	error_def(ERR_REPLINFO);
	error_def(ERR_REPLWARN);

	assert(0 != n_readers || 0 != n_writers);
	upd_helper_ctl = recvpool.upd_helper_ctl;

	/* let's clean up dead helpers first so we get an accurate count of available slots */
	upd_helper_ctl->reap_helpers = HELPER_REAP_NOWAIT;
	while (HELPER_REAP_NONE != upd_helper_ctl->reap_helpers && SRV_ALIVE == is_recv_srv_alive())
		SHORT_SLEEP(GTMRECV_WAIT_FOR_UPD_SHUTDOWN);
	upd_helper_ctl->reap_helpers = HELPER_REAP_NONE; /* just in case recvr died */

	/* count available slots so receiver doesn't have to */
	for (avail_slots = 0, helper = upd_helper_ctl->helper_list, helper_top = helper + MAX_UPD_HELPERS;
		helper < helper_top; helper++)
	{
		if (0 == helper->helper_pid)
		{
			avail_slots++;
			helper->helper_pid_prev = 0; /* force out abnormally terminated helpers as well */
			helper->helper_shutdown = NO_SHUTDOWN; /* clean state */
		}
	}
	if (avail_slots < n_readers + n_writers)
	{
		SNPRINTF(err_str, SIZEOF(err_str),
				"%d helpers will exceed the maximum allowed (%d), limit the helpers to %d\n",
				n_readers + n_writers, MAX_UPD_HELPERS, avail_slots);
		gtm_putmsg(VARLSTCNT(4) ERR_REPLERR, 2, LEN_AND_STR(err_str));
		return ABNORMAL_SHUTDOWN;
	}

	upd_helper_ctl->start_n_readers = n_readers;
	upd_helper_ctl->start_n_writers = n_writers;
	SHM_WRITE_MEMORY_BARRIER;
	upd_helper_ctl->start_helpers = TRUE; /* hey receiver, let's go, start 'em up */
	while (upd_helper_ctl->start_helpers && SRV_ALIVE == is_recv_srv_alive())
		SHORT_SLEEP(GTMRECV_WAIT_FOR_SRV_START);
	if (!upd_helper_ctl->start_helpers)
	{
		started_readers = upd_helper_ctl->start_n_readers;
		started_writers = upd_helper_ctl->start_n_writers;
		SNPRINTF(err_str, SIZEOF(err_str), "%s %d out of %d readers and %d out of %d writers started",
				((started_readers + started_writers) == (n_readers + n_writers)) ? "All" : "Only",
				started_readers, n_readers, started_writers, n_writers);
		if ((started_readers + started_writers) == (n_readers + n_writers))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_REPLINFO, 2, LEN_AND_STR(err_str));
			return NORMAL_SHUTDOWN;
		}
		gtm_putmsg(VARLSTCNT(4) ERR_REPLWARN, 2, LEN_AND_STR(err_str));
		return ABNORMAL_SHUTDOWN;
	}
	gtm_putmsg(VARLSTCNT(4) ERR_REPLERR, 2,
			LEN_AND_LIT("Receiver server is not alive to start helpers. Start receiver server first"));
	return ABNORMAL_SHUTDOWN;
}
