/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#include "gtm_inet.h"

#include "gtm_time.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmio.h"
#include "iosp.h"
#include "jnl_get_checksum.h"
#include "anticipatory_freeze.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	seq_num			seq_num_zero;
GBLREF	int4			strm_index;

error_def	(ERR_PREMATEOF);

void	jnl_write_epoch_rec(sgmnt_addrs *csa)
{
	struct_jrec_epoch	epoch_record;
	jnl_buffer_ptr_t	jb;
	jnl_private_control	*jpc;
	jnl_file_header		*header;
	jnlpool_addrs_ptr_t	save_jnlpool;
	unsigned char		hdr_base[REAL_JNL_HDR_LEN + MAX_IO_BLOCK_SIZE];
	sgmnt_data_ptr_t	csd;
	uint4			jnl_fs_block_size, read_write_size;
	int			idx;

	assert(!IN_PHASE2_JNL_COMMIT(csa));
	jpc = csa->jnl;
	save_jnlpool = jnlpool;
	if (csa->jnlpool && (csa->jnlpool != jnlpool))
		jnlpool = csa->jnlpool;
	jb = jpc->jnl_buff;
	assert((csa->ti->early_tn == csa->ti->curr_tn) || (csa->ti->early_tn == csa->ti->curr_tn + 1));
	assert(0 != jpc->pini_addr);
	csd = csa->hdr;
	epoch_record.prefix.jrec_type = JRT_EPOCH;
	epoch_record.prefix.forwptr = epoch_record.suffix.backptr = EPOCH_RECLEN;
	epoch_record.blks_to_upgrd = csd->blks_to_upgrd;
	epoch_record.total_blks    = csd->trans_hist.total_blks;
	epoch_record.free_blocks   = csd->trans_hist.free_blocks;
	epoch_record.fully_upgraded = csd->fully_upgraded;
	epoch_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	/* in case jpc->pini_addr turns out to be zero (not clear how), we use the pini_addr field of the
	 * first PINI journal record in the journal file which is nothing but JNL_HDR_LEN.
	 */
	epoch_record.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	jb->epoch_tn = epoch_record.prefix.tn = csa->ti->curr_tn;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	epoch_record.prefix.time = jgbl.gbl_jrec_time;
	/* we need to write epochs if jgbl.forw_phase_recovery so future recovers will have a closer turnaround point */
	jb->next_epoch_time =  epoch_record.prefix.time + jb->epoch_interval;
	epoch_record.prefix.checksum = INIT_CHECKSUM_SEED;
	ASSERT_JNL_SEQNO_FILEHDR_JNLPOOL(csa, jnlpool);	/* debug-only sanity check between seqno of csa->hdr and jnlpool */
	if (jgbl.forw_phase_recovery)
	{	/* Set jnl-seqno of epoch record from the current seqno that rollback is playing. Note that in case of -recover
		 * we don't actually care what seqnos get assigned to the epoch record so we go ahead and set it to the same
		 * fields even though those might be 0 or not.
		 */
		epoch_record.jnl_seqno = jgbl.mur_jrec_seqno;
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			epoch_record.strm_seqno[idx] = csd->strm_reg_seqno[idx];
		/* If MUPIP JOURNAL -ROLLBACK, might need to do additional processing. See macro definition for comments */
		MUR_ADJUST_STRM_REG_SEQNO_IF_NEEDED(csd, epoch_record.strm_seqno);
	} else if (REPL_ALLOWED(csd))
	{
		epoch_record.jnl_seqno = csd->reg_seqno;	/* Note we cannot use jnlpool_ctl->jnl_seqno since
								 * we might not presently hold the journal pool lock */
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			epoch_record.strm_seqno[idx] = csd->strm_reg_seqno[idx];
	} else
	{
		epoch_record.jnl_seqno = seq_num_zero;
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			epoch_record.strm_seqno[idx] = 0;
	}
	if (jb->end_of_data)
	{
		jnl_fs_block_size = jb->fs_block_size;
		header = (jnl_file_header *)(ROUND_UP2((uintszofptr_t)hdr_base, jnl_fs_block_size));
		read_write_size = ROUND_UP2(REAL_JNL_HDR_LEN, jnl_fs_block_size);
		assert((unsigned char *)header + read_write_size <= ARRAYTOP(hdr_base));
		DO_FILE_READ(jpc->channel, 0, header, read_write_size, jpc->status, jpc->status2);
		assert(SS_NORMAL != jpc->status || SS_NORMAL == jpc->status2);
		if (SS_NORMAL == jpc->status)
		{
			header->end_of_data = jb->end_of_data;
			csa->hdr->jnl_eovtn = header->eov_tn;
			header->eov_tn = jb->eov_tn;
			header->eov_timestamp = jb->eov_timestamp;
			header->end_seqno = jb->end_seqno;
			/* Keep header->strm_end_seqno[] uptodate as well if applicable */
			if (INVALID_SUPPL_STRM != strm_index)
			{
				for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
					header->strm_end_seqno[idx] = jb->strm_end_seqno[idx];
			}
			JNL_DO_FILE_WRITE(csa, csd->jnl_file_name,
				jpc->channel, 0, header, read_write_size, jpc->status, jpc->status2);
			/* for abnormal status do not do anything. journal file header will have previous end_of_data */
		}
	}
	jb->end_of_data = jb->rsrv_freeaddr;
	jb->eov_tn = csa->ti->curr_tn;
	jb->eov_timestamp = jgbl.gbl_jrec_time;
	jb->end_seqno = epoch_record.jnl_seqno;
	/* Keep header->strm_end_seqno[] uptodate as well if applicable */
	if (INVALID_SUPPL_STRM != strm_index)
	{
		/* ROLLBACK turns off replication in mur_close_files and so we should never come here with csd->repl_state
		 * indicating replication is allowed (for ROLLBACK). The only exception is if we are done with ROLLBACK,
		 * but invoked gds_rundown (from mur_close_files) in which case repl_state will be turned back ON by
		 * mur_close_files (process_exiting = TRUE). Assert accordingly
		 */
		assert(!jgbl.mur_rollback || !REPL_ALLOWED(csd) || process_exiting);
		assert(jgbl.mur_rollback || REPL_ALLOWED(csd));
		/* If caller is MUPIP JOURNAL ROLLBACK, it cannot be FORWARD rollback since that runs with journaling
		 * turned off and we are writing journal records in this function. Assert accordingly.
		 */
		assert(!jgbl.mur_rollback || !jgbl.mur_options_forward);
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			jb->strm_end_seqno[idx] = csd->strm_reg_seqno[idx];
	}
	epoch_record.filler = 0;
	epoch_record.prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED,
								(unsigned char *)&epoch_record, SIZEOF(struct_jrec_epoch));
	jnl_write(jpc, JRT_EPOCH, (jnl_record *)&epoch_record, NULL);
	jb->post_epoch_freeaddr = jb->rsrv_freeaddr;
	if (save_jnlpool != jnlpool)
		jnlpool = save_jnlpool;
}
