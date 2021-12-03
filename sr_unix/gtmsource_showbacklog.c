/****************************************************************
 *								*
 * Copyright (c) 2006-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#if !defined(__MVS__) && !defined(VMS)
#include <sys/param.h>
#endif

#include <sys/time.h>
#include "gtm_inet.h"
#include <errno.h>

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "error.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "util.h"
#include "repl_log.h"
#include "is_proc_alive.h"
#include "min_max.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];

error_def(ERR_LASTTRANS);
error_def(ERR_SRCSRVNOTEXIST);
error_def(ERR_SRCBACKLOGSTATUS);

int gtmsource_showbacklog(void)
{
	seq_num			heartbeat_jnl_seqno, jnl_seqno, read_jnl_seqno, src_backlog;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;
	char * 			lasttrans[] = {"posted        ","sent          ","acknowledged  "};
	char * 			syncstate[] = {"is behind by", "has not acknowledged", "is ahead by"};
	int4			index, syncstateindex = 0;
	boolean_t		srv_alive;

	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);

	if (NULL != jnlpool->gtmsource_local)	/* Show backlog for a specific source server */
		gtmsourcelocal_ptr = jnlpool->gtmsource_local;
	else
		gtmsourcelocal_ptr = &jnlpool->gtmsource_local_array[0];
	for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++)
	{
		if ('\0' == gtmsourcelocal_ptr->secondary_instname[0])
		{
			assert(NULL == jnlpool->gtmsource_local);
			continue;
		}
		/* If SHOWBACKLOG on a specific secondary instance is requested, print the backlog information irrespective
		 * of whether a source server for that instance is alive or not. For SHOWBACKLOG on ALL secondary instances
		 * print backlog information only for those instances that have an active or passive source server alive.
		 */
		heartbeat_jnl_seqno = gtmsourcelocal_ptr->heartbeat_jnl_seqno;

		if ((NULL == jnlpool->gtmsource_local) && (0 == gtmsourcelocal_ptr->gtmsource_pid))
			continue;
		repl_log(stderr, TRUE, TRUE,
			"Initiating SHOWBACKLOG operation on source server pid [%d] for secondary instance [%s]\n",
			gtmsourcelocal_ptr->gtmsource_pid, gtmsourcelocal_ptr->secondary_instname);
		/* jnlpool->jnlpool_ctl->jnl_seqno >= gtmsourcelocal_ptr->read_jnl_seqno is the most common case;
		 * see gtmsource_readpool() for when the rare case can occur
		 * Within a Source Server, jnlpool->jnlpool_ctl->jnl_seqno and gtmsourcelocal_ptr->read_jnl_seqno
		 * counters start with 1 and cannot be 0 or less. heartbeat_jnl_seqno is 0 whenever the Source Server
		 * restarts or we have an empty database.
		 * Use local variables for arithmetic computation to prevent adjustments to the memory structure.
		 */
		if (0 < jnlpool->jnlpool_ctl->jnl_seqno)
			jnl_seqno = jnlpool->jnlpool_ctl->jnl_seqno - 1;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LASTTRANS, 3, LEN_AND_STR(lasttrans[0]), &jnl_seqno);
		if (0 < gtmsourcelocal_ptr->read_jnl_seqno)
			read_jnl_seqno = gtmsourcelocal_ptr->read_jnl_seqno - 1;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LASTTRANS, 3, LEN_AND_STR(lasttrans[1]), &read_jnl_seqno);
		if (0 != heartbeat_jnl_seqno)
			heartbeat_jnl_seqno--;
		assert(0 <= heartbeat_jnl_seqno);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LASTTRANS, 3, LEN_AND_STR(lasttrans[2]), &heartbeat_jnl_seqno);
		src_backlog = MAX(jnl_seqno, read_jnl_seqno) - heartbeat_jnl_seqno;
		if (0 == heartbeat_jnl_seqno)
			syncstateindex = 1;
		if (heartbeat_jnl_seqno > MIN(jnl_seqno, read_jnl_seqno))
		{
			src_backlog = heartbeat_jnl_seqno - MIN(jnl_seqno, read_jnl_seqno);
			syncstateindex = 2;
		}
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SRCBACKLOGSTATUS, 5,
				LEN_AND_STR(gtmsourcelocal_ptr->secondary_instname),
				LEN_AND_STR(syncstate[syncstateindex]), &src_backlog);
		srv_alive = (0 == gtmsourcelocal_ptr->gtmsource_pid) ? FALSE : is_proc_alive(gtmsourcelocal_ptr->gtmsource_pid, 0);
		if (!srv_alive)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_WARNING(ERR_SRCSRVNOTEXIST), 2,
						LEN_AND_STR(gtmsourcelocal_ptr->secondary_instname));
		else if ((gtmsourcelocal_ptr->mode == GTMSOURCE_MODE_PASSIVE)
				|| (gtmsourcelocal_ptr->mode == GTMSOURCE_MODE_ACTIVE_REQUESTED))
			util_out_print("WARNING - Source Server is in passive mode, transactions are not being replicated", TRUE);
		if (NULL != jnlpool->gtmsource_local)
			break;
	}
	return (NORMAL_SHUTDOWN);
}
