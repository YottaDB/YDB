/****************************************************************
 *								*
 *	Copyright 2006, 2007 Fidelity Information Services, Inc	*
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

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl.h"
#include "iosp.h"
#include "ftok_sems.h"
#include "repl_instance.h"
#include "repl_dbg.h"
#include "repl_log.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gd_region		*gtmsource_mru_reg;
GBLREF	gd_addr			*gd_header;
GBLREF	seq_num			gtmsource_last_flush_reg_seq;
GBLREF	time_t			gtmsource_last_flush_time;
GBLREF	volatile time_t		gtmsource_now;
GBLREF	FILE			*gtmsource_log_fp;

/* Flush the source server's current resync_seqno into the corresponding slot in the replication instance file header */
void gtmsource_flush_fh(seq_num resync_seqno)
{
	seq_num		max_reg_seqno, pre_update_seqno;
	gd_region	*reg, *region_top, *mru_reg;
	sgmnt_addrs	*csa, *mru_reg_csa;
	boolean_t	was_crit;
	REPL_DEBUG_ONLY(
		char	reg_name[MAX_RN_LEN + 1]; /* + 1 for trailing '\0' */
	)

	error_def(ERR_REPLFTOKSEM);

	jnlpool.gtmsource_local->read_jnl_seqno = resync_seqno;
	gtmsource_last_flush_time = gtmsource_now;
	if (jnlpool.gtmsource_local->last_flush_resync_seqno == resync_seqno)
		return;
	/* need to flush resync_seqno to instance file. get the ftok semaphore lock before flushing */
	repl_inst_ftok_sem_lock();
	if (REPL_PROTO_VER_DUALSITE == jnlpool.gtmsource_local->remote_proto_ver)
	{	/* We need to update the resync seqno in the journal pool. But no need to hold the lock on the journal
		 * pool as we are guaranteed to be the ONLY source server running (due to secondary being dual-site).
		 */
		jnlpool.jnlpool_ctl->max_dualsite_resync_seqno = resync_seqno;
	}
	repl_inst_flush_gtmsrc_lcl();	/* this requires the ftok semaphore to be held */
	repl_inst_ftok_sem_release();
	if (REPL_PROTO_VER_DUALSITE != jnlpool.gtmsource_local->remote_proto_ver)
		return;
	/* If remote side is dualsite, we need to maintain "dualsite_resync_seqno" in the db file headers and flush */
	max_reg_seqno = 0;
	for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
	{
		csa = &FILE_INFO(reg)->s_addrs;
		if (REPL_ALLOWED(csa->hdr))
		{
			assert(csa->hdr->dualsite_resync_seqno <= resync_seqno); /* don't want to go back */
			pre_update_seqno = csa->hdr->dualsite_resync_seqno;
			UPDATE_DUALSITE_RESYNC_SEQNO(reg, pre_update_seqno, resync_seqno); /* updates csa (no change though) */
			assert(csa->hdr->dualsite_resync_seqno == resync_seqno);
			if (csa->hdr->reg_seqno > max_reg_seqno)
			{ /* there could be multiple regions with the same max_reg_seqno; we choose the first in the list */
				max_reg_seqno = csa->hdr->reg_seqno; /* where is the system at? */
				mru_reg_csa = csa;
				mru_reg = reg;
			}
		}
	}
	if (0 == max_reg_seqno) /* we are in trouble, no replicated region found */
		GTMASSERT;
	assert(max_reg_seqno >= resync_seqno);
	assert(max_reg_seqno >= gtmsource_last_flush_reg_seq);
	if (max_reg_seqno == gtmsource_last_flush_reg_seq)	/* no updates to the system */
	{
		if (FALSE == (was_crit = mru_reg_csa->now_crit))
			grab_crit(mru_reg);
		fileheader_sync(mru_reg);
		if (!was_crit)
			rel_crit(mru_reg);
	} /* else, GTM process will flush file header */
	REPL_DEBUG_ONLY(
		memcpy(reg_name, mru_reg->rname, mru_reg->rname_len);
		reg_name[mru_reg->rname_len] = '\0';
		if (max_reg_seqno == gtmsource_last_flush_reg_seq)
			REPL_DPRINT2("flush_fh: flushed file header of %s\n", reg_name);
		REPL_DPRINT4("flush_fh: set mru_reg to %s, last_flush_reg_seqno to %llu, last_flush_resync_seqno to %llu\n",
			reg_name, max_reg_seqno, resync_seqno);
	)
	gtmsource_mru_reg = mru_reg;
	gtmsource_last_flush_reg_seq = max_reg_seqno;
	return;
}
