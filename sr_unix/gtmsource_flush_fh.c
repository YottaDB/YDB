/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
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

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	time_t			gtmsource_last_flush_time;
GBLREF	volatile time_t		gtmsource_now;
GBLREF	gtmsource_state_t	gtmsource_state;

/* Flush the source server's current resync_seqno into the corresponding slot in the replication instance file header */
void gtmsource_flush_fh(seq_num resync_seqno)
{
	sgmnt_addrs	*repl_csa;

	assert((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open);
	DEBUG_ONLY(
		repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
		assert(!repl_csa->hold_onto_crit);
		ASSERT_VALID_JNLPOOL(repl_csa);
	)
	jnlpool.gtmsource_local->read_jnl_seqno = resync_seqno;
	gtmsource_last_flush_time = gtmsource_now;
	if (jnlpool.gtmsource_local->last_flush_resync_seqno == resync_seqno)
		return;
	/* need to flush resync_seqno to instance file. Grab the journal pool lock before flushing */
	grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, HANDLE_CONCUR_ONLINE_ROLLBACK); /* sets gtmsource_state */
	if (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state)
		return;
	repl_inst_flush_gtmsrc_lcl();	/* this requires the ftok semaphore to be held */
	rel_lock(jnlpool.jnlpool_dummy_reg);
	return;
}
