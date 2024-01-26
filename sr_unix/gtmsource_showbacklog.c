/****************************************************************
 *								*
 * Copyright (c) 2006-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
	char * 			syncstate[] = {
						"is behind by",
						"has not acknowledged",
						"is ahead by",
						"is not receiving transactions as source server is in passive mode"
						};
	int4			index, syncstateindex;
	boolean_t		srv_alive;
	char			buf[128];
	int			buflen;

	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);

	if (NULL != jnlpool->gtmsource_local)	/* Show backlog for a specific source server */
		gtmsourcelocal_ptr = jnlpool->gtmsource_local;
	else
		gtmsourcelocal_ptr = &jnlpool->gtmsource_local_array[0];
	for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++)
	{
		syncstateindex = 0;
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
		/* Within a Source Server, jnlpool->jnlpool_ctl->jnl_seqno and gtmsourcelocal_ptr->read_jnl_seqno
		 * counters start with 1 and cannot be 0 or less. heartbeat_jnl_seqno is 0 whenever the Source Server
		 * restarts or we have an empty database.
		 * Use local variables for arithmetic computation to prevent adjustments to the memory structure.
		 */
		assert(0 < jnlpool->jnlpool_ctl->jnl_seqno);
		jnl_seqno = jnlpool->jnlpool_ctl->jnl_seqno - 1;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LASTTRANS, 3, LEN_AND_STR(lasttrans[0]), &jnl_seqno);
		assert(0 < gtmsourcelocal_ptr->read_jnl_seqno);
		read_jnl_seqno = gtmsourcelocal_ptr->read_jnl_seqno - 1;
		if (read_jnl_seqno > jnl_seqno)
		{	/* jnlpool->jnlpool_ctl->jnl_seqno >= gtmsourcelocal_ptr->read_jnl_seqno is the most common case;
			 * 1) See gtmsource_readpool() for when the rare opposite case can occur.
			 * 2) Additionally it is possible that in between fetching "jnlpool->jnlpool_ctl->jnl_seqno" and
			 *    "gtmsourcelocal_ptr->read_jnl_seqno", a few updates happen and get sent across by the source
			 *    server and so "read_jnl_seqno > jnl_seqno" is possible even without (1).
			 * Therefore handle it by forcing "read_jnl_seqno" to be equal to "jnl_seqno".
			 */
			read_jnl_seqno = jnl_seqno;
		}
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LASTTRANS, 3, LEN_AND_STR(lasttrans[1]), &read_jnl_seqno);
		if (0 != heartbeat_jnl_seqno)
			heartbeat_jnl_seqno--;
		assert(0 <= heartbeat_jnl_seqno);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LASTTRANS, 3, LEN_AND_STR(lasttrans[2]), &heartbeat_jnl_seqno);
		src_backlog = jnl_seqno - heartbeat_jnl_seqno;
		if (0 == heartbeat_jnl_seqno)
			syncstateindex = 1;
		if (heartbeat_jnl_seqno > read_jnl_seqno)
		{
			src_backlog = heartbeat_jnl_seqno - read_jnl_seqno;
			syncstateindex = 2;
		}
		srv_alive = (0 == gtmsourcelocal_ptr->gtmsource_pid) ? FALSE : is_proc_alive(gtmsourcelocal_ptr->gtmsource_pid, 0);
		if (srv_alive
			&& ((gtmsourcelocal_ptr->mode == GTMSOURCE_MODE_PASSIVE)
				|| (gtmsourcelocal_ptr->mode == GTMSOURCE_MODE_ACTIVE_REQUESTED)))
			syncstateindex = 3;
		if (3 == syncstateindex)
			buflen = SNPRINTF(buf, SIZEOF(buf), "%s", syncstate[syncstateindex]);
		else
			buflen = SNPRINTF(buf, SIZEOF(buf), "%s %llu transaction(s)", syncstate[syncstateindex], src_backlog);
		assert(buflen == STRLEN(buf));	/* assert that "buf[]" allocation had enough space to begin with */
		PRO_ONLY(UNUSED(buflen));	/* to avoid [clang-analyzer-deadcode.DeadStores] warning */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SRCBACKLOGSTATUS, 4,
				LEN_AND_STR(gtmsourcelocal_ptr->secondary_instname), LEN_AND_STR(buf));
		if (!srv_alive)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) MAKE_MSG_WARNING(ERR_SRCSRVNOTEXIST), 2,
						LEN_AND_STR(gtmsourcelocal_ptr->secondary_instname));
		if (NULL != jnlpool->gtmsource_local)
			break;
	}
	return (NORMAL_SHUTDOWN);
}
