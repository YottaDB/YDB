/****************************************************************
 *								*
 * Copyright (c) 2006-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_instance.h"
#include "gtmsource_srv_latch.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	time_t			gtmsource_last_flush_time;
GBLREF	volatile time_t		gtmsource_now;
GBLREF	gtmsource_state_t	gtmsource_state;

/* Flush the source server's current resync_seqno into the corresponding slot in the replication instance file header */
void gtmsource_flush_fh(seq_num resync_seqno, bool get_latch)
{
	sgmnt_addrs	*repl_csa;

	assert((NULL != jnlpool) && (NULL != jnlpool->jnlpool_dummy_reg) && jnlpool->jnlpool_dummy_reg->open);
#	ifdef DEBUG
	repl_csa = &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs;
	ASSERT_VALID_JNLPOOL(repl_csa);
#	endif
	jnlpool->gtmsource_local->read_jnl_seqno = resync_seqno;
	gtmsource_last_flush_time = gtmsource_now;
	if (jnlpool->gtmsource_local->last_flush_resync_seqno == resync_seqno)
		return;
	/* need to flush resync_seqno to instance file. Grab the journal pool lock before flushing */
	if (get_latch)
	{
		grab_gtmsource_srv_latch(&jnlpool->gtmsource_local->gtmsource_srv_latch, UINT32_MAX,
				HANDLE_CONCUR_ONLINE_ROLLBACK);
		if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
			return;
	}
	repl_inst_flush_gtmsrc_lcl();	/* this requires the ftok semaphore to be held */
	if (get_latch)
		rel_gtmsource_srv_latch(&jnlpool->gtmsource_local->gtmsource_srv_latch);
	return;
}
