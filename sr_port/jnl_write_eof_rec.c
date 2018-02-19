/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "jnl_get_checksum.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF 	jnl_gbls_t		jgbl;

void	jnl_write_eof_rec(sgmnt_addrs *csa, struct_jrec_eof *eof_record)
{
	jnl_private_control	*jpc;
	jnlpool_addrs_ptr_t	save_jnlpool;

	assert(!IN_PHASE2_JNL_COMMIT(csa));
	jpc = csa->jnl;
	assert((0 != jpc->pini_addr)
		|| ((off_t)jpc->jnl_buff->rsrv_freeaddr
				> ((off_t)DISK_BLOCK_SIZE * jpc->jnl_buff->filesize) - JNL_FILE_TAIL_PRESERVE));
	eof_record->prefix.jrec_type = JRT_EOF;
	eof_record->prefix.forwptr = eof_record->suffix.backptr = EOF_RECLEN;
	eof_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	eof_record->prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	eof_record->prefix.tn = csa->hdr->trans_hist.curr_tn;
	eof_record->prefix.checksum = INIT_CHECKSUM_SEED;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	eof_record->prefix.time = jgbl.gbl_jrec_time;
	save_jnlpool = jnlpool;
	if (csa->jnlpool && (csa->jnlpool == jnlpool))
		jnlpool = csa->jnlpool;
	ASSERT_JNL_SEQNO_FILEHDR_JNLPOOL(csa, jnlpool); /* debug-only sanity check between seqno of csa->hdr and jnlpool */
	/* In UNIX, mur_close_files, at the beginning sets both jgbl.mur_jrec_seqno and csa->hdr->reg_seqno to
	 * murgbl.consist_jnl_seqno. Assert that this is indeed the case. However, csa->hdr->reg_seqno is NOT
	 * maintained by rollback during forward phase of recovery and is set only at mur_close_files whereas
	 * jgbl.mur_jrec_seqno is maintained all along. So, unless we are called from mur_close_files, we cannot
	 * rely csa->hdr->reg_seqno and so we can do the equality check only if we are called from mur_close_files
	 */
	assert(!jgbl.forw_phase_recovery || !jgbl.mur_rollback || (jgbl.mur_jrec_seqno == csa->hdr->reg_seqno) || !process_exiting);
	/* If caller is MUPIP JOURNAL ROLLBACK, it cannot be FORWARD rollback since that runs with journaling turned off
	 * and we are writing journal records here. Assert accordingly.
	 */
	assert(!jgbl.mur_rollback || !jgbl.mur_options_forward);
	if (!jgbl.forw_phase_recovery)
	{
		if (REPL_ALLOWED(csa))
			eof_record->jnl_seqno = csa->hdr->reg_seqno;/* Note we cannot use jnlpool_ctl->jnl_seqno since
								     * we might not presently hold the journal pool lock */
		else
			eof_record->jnl_seqno = 0;
	} else
		QWASSIGN(eof_record->jnl_seqno, jgbl.mur_jrec_seqno);
	eof_record->filler = 0;
	eof_record->prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)eof_record, SIZEOF(struct_jrec_eof));
	jnl_write(jpc, JRT_EOF, (jnl_record *)eof_record, NULL);
	if (save_jnlpool != jnlpool)
		jnlpool = save_jnlpool;
}
