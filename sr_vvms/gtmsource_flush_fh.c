/****************************************************************
 *								*
 *	Copyright 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

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
#include "repl_dbg.h"

GBLREF gd_region	*gtmsource_mru_reg;
GBLREF gd_addr		*gd_header;
GBLREF seq_num		gtmsource_last_flush_reg_seq, gtmsource_last_flush_resync_seq;

void gtmsource_flush_fh(seq_num resync_seqno)
{ /* Update all replicated regions' file header resync_seqno to given number
   * Flush at least one region's file header to disk if there has been no updates since the last time we flushed. We do this to
   *     keep the system's last exchanged seqno as current as we can. In case the system crashes, the difference between
   *     the two systems' seqno shouldn't be too large to cause large backlog and so long resynchronization. On a system that
   *     is contantly updated, there is a good chance that a GTM process flushes file header hence recording replication progress,
   *     in which case the source server doesn't have to flush.
   * We choose the most recently updated region to flush
   */
	seq_num		max_reg_seqno, pre_update_seqno;
	gd_region	*reg, *region_top, *mru_reg;
	sgmnt_addrs	*csa, *mru_reg_csa;
	boolean_t	was_crit;
	REPL_DEBUG_ONLY(
		char	reg_name[MAX_RN_LEN + 1]; /* + 1 for trailing '\0' */
	)

	assert(gtmsource_last_flush_resync_seq <= resync_seqno);
	if (gtmsource_last_flush_resync_seq == resync_seqno) /* already flushed */
	{
		REPL_EXTRA_DPRINT1("flush_fh: no action, returning\n");
		return;
	}
	max_reg_seqno = 0;
	for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
	{
		csa = &FILE_INFO(reg)->s_addrs;
		if (REPL_ALLOWED(csa->hdr))
		{
			assert(csa->hdr->resync_seqno <= resync_seqno); /* don't want to go back */
			pre_update_seqno = csa->hdr->resync_seqno;
			UPDATE_RESYNC_SEQNO(reg, pre_update_seqno, resync_seqno); /* updates csa (no change though) and now_crit */
			assert(csa->hdr->resync_seqno == resync_seqno);
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
	assert(resync_seqno  <= max_reg_seqno);
	assert(max_reg_seqno >= gtmsource_last_flush_reg_seq);
	assert(max_reg_seqno >= gtmsource_last_flush_resync_seq);
	if (max_reg_seqno == gtmsource_last_flush_reg_seq)	/* no updates to the system */
	{
		if (FALSE == (was_crit = mru_reg_csa->now_crit))
			grab_crit(mru_reg);
		fileheader_sync(mru_reg);
		if (!was_crit)
			rel_crit(mru_reg);
		gtmsource_last_flush_resync_seq = resync_seqno;
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
