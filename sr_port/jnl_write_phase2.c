/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
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
#include "gdscc.h"
#include "jnl.h"
#include "jnl_typedef.h"
#include "jnl_get_checksum.h"
#include "jnl_write_pblk.h"
#include "jnl_write_aimg_rec.h"
#include "jnl_write.h"

GBLREF	jnl_gbls_t	jgbl;
GBLREF	uint4		process_id;

void	jnl_write_phase2(sgmnt_addrs *csa, jbuf_rsrv_struct_t *jrs)
{
	boolean_t		need_pini_adjustment;	/* TRUE => first ALIGN needs to set pini_addr of JNL_FILE_FIRST_RECORD */
	cw_set_element		*cse;
	enum jnl_record_type	rectype;
	jnl_buffer_ptr_t	jbp;
	jnl_format_buffer	*jfb;
	jnl_private_control	*jpc;
	struct_jrec_tcom	*tcom_record_ptr;
	jrec_rsrv_elem_t	*first_jre, *jre, *jre_top;
	uint4			common_csum, pini_addr, reclen, freeaddr, prev_freeaddr, start_freeaddr;
	void			*param1;
	jbuf_phase2_in_prog_t	*phs2cmt;
	int			commit_index;
	boolean_t		write_to_jnlbuff, write_to_jnlpool;

	assert(JNL_ALLOWED(csa));
	assert(NULL != jrs);
	assert(jrs->tot_jrec_len);
	jpc = csa->jnl;
	common_csum = 0;
	write_to_jnlbuff = JNL_ENABLED(csa);
	assert(write_to_jnlbuff || REPL_ALLOWED(csa));
	first_jre = jrs->jrs_array;
	jre_top = first_jre + jrs->usedlen;
	/* Note: In REPL_WAS_ALLOWED case, "write_to_jnlbuff" will be FALSE and so pini_addr etc. won't be initialized in
	 * the logical jnl records we write to the jnlpool. But that is okay since the update process does not care about
	 * these fields in the jnl record anyways. Also see similar comment in "jnl_write_logical"
	 */
	if (write_to_jnlbuff)
	{
		jbp = jpc->jnl_buff;
		ADJUST_CHECKSUM_TN(INIT_CHECKSUM_SEED, &jpc->curr_tn, common_csum);
		pini_addr = jpc->pini_addr;
		need_pini_adjustment = FALSE;
		commit_index = jrs->phase2_commit_index;
		phs2cmt = &jbp->phase2_commit_array[commit_index];
		start_freeaddr = phs2cmt->start_freeaddr;
#		ifdef DEBUG
		assert(start_freeaddr >= jbp->freeaddr);
		assert(start_freeaddr < jbp->rsrv_freeaddr);
		assert(process_id == phs2cmt->process_id);
		assert(!phs2cmt->write_complete);
		assert(jpc->phase2_freeaddr == start_freeaddr + phs2cmt->tot_jrec_len);
#		endif
		if (!pini_addr)
		{	/* PINI record would have been written as the first in this transaction in that case.
			 * Use the corresponding freeaddr as the pini_addr for the checksum computation.
			 * There is an exception in that an ALIGN record might have been written before the PINI
			 * record. Thankfully an ALIGN record does not have a pini_addr so no need to worry about that.
			 */
			rectype = first_jre->rectype;
			pini_addr = start_freeaddr;
			if (JRT_PINI != rectype)
			{
				assert(JRT_ALIGN == rectype);
				assert(&first_jre[1] < jre_top);
				assert(JRT_PINI == first_jre[1].rectype);
				pini_addr += first_jre->reclen;
				need_pini_adjustment = TRUE;
			}
		}
		assert(pini_addr);
		ADJUST_CHECKSUM(common_csum, pini_addr, common_csum);
		ADJUST_CHECKSUM(common_csum, jgbl.gbl_jrec_time, common_csum);
		SET_JPC_PHASE2_FREEADDR(jpc, jbp, start_freeaddr);	/* needed by "jnl_write_*" calls below */
	}
	for (jre = first_jre; jre < jre_top; jre++)
	{
#		ifdef DEBUG
		if (write_to_jnlbuff)
			prev_freeaddr = jpc->phase2_freeaddr;
#		endif
		rectype = jre->rectype;
		reclen = jre->reclen;
		param1 = jre->param1;
		switch (rectype)
		{
		case JRT_ALIGN:
			assert(write_to_jnlbuff);
			assert((jre == first_jre) || jpc->pini_addr);
			jnl_write_align_rec(csa, reclen - MIN_ALIGN_RECLEN, jgbl.gbl_jrec_time);
			break;
		case JRT_PINI:
			assert(write_to_jnlbuff);
			assert(PINI_RECLEN == reclen);
			jnl_write_pini(csa);
			break;
		case JRT_PBLK:
			assert(write_to_jnlbuff);
			assert(reclen <= ROUND_UP2(FIXED_PBLK_RECLEN + JREC_SUFFIX_SIZE + csa->hdr->blk_size, JNL_REC_START_BNDRY));
			cse = (cw_set_element *)param1;
			jnl_write_pblk(csa, cse, common_csum);
			break;
		case JRT_AIMG:
			assert(write_to_jnlbuff);
			assert(reclen <= ROUND_UP2(FIXED_AIMG_RECLEN + JREC_SUFFIX_SIZE + csa->hdr->blk_size, JNL_REC_START_BNDRY));
			cse = (cw_set_element *)param1;
			jnl_write_aimg_rec(csa, cse, common_csum);
			break;
		case JRT_INCTN:
			assert(write_to_jnlbuff);
			assert(INCTN_RECLEN == reclen);
			jnl_write_inctn_rec(csa);
			break;
		case JRT_TCOM:
			assert(TCOM_RECLEN == reclen);
			tcom_record_ptr = (struct_jrec_tcom *)param1;
			/* Note: tcom_record_ptr->prefix.time, num_participants, token_seq.token and strm_seqno are already set
			 * in tp_tend and is common across all participating TP regions.
			 */
			tcom_record_ptr->prefix.pini_addr = jpc->pini_addr;
			tcom_record_ptr->prefix.tn = jpc->curr_tn;
			tcom_record_ptr->prefix.checksum = INIT_CHECKSUM_SEED;
			tcom_record_ptr->prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED,
								(unsigned char *)tcom_record_ptr, SIZEOF(struct_jrec_tcom));
			JNL_WRITE_APPROPRIATE(csa, jpc, JRT_TCOM, (jnl_record *)tcom_record_ptr, NULL);
			break;
		default:
			assert(IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype) || (JRT_NULL == rectype));
			jfb = param1;
			assert(rectype == jfb->rectype);
			assert(reclen == jfb->record_size);
			if (!IS_ZTP(rectype))
				jnl_write_logical(csa, jfb, common_csum);
			else
			{
				assert(write_to_jnlbuff);
				jnl_write_ztp_logical(csa, jfb, common_csum, 0);
			}
			break;
		}
		/* It is possible one of the "jnl_write_*" calls above encounters a "jnl_write_attempt -> jnl_file_lost"
		 * scenario. In that case jpc->channel would be set to NOJNL. Take that into account in the assert below.
		 * It is okay to finish the "for" loop and make more "jnl_write_*" calls instead of adding "if" checks
		 * to address this rare scenario. It is just wasted effort in a rare case but should be a safe invocation.
		 */
		assert(!write_to_jnlbuff || (jpc->phase2_freeaddr == (prev_freeaddr + reclen)) || (NOJNL == jpc->channel));
	}
	if (write_to_jnlbuff)
	{
		DEBUG_ONLY(freeaddr = jpc->phase2_freeaddr;)
		assert((freeaddr > jbp->freeaddr) || (csa->now_crit && (freeaddr == jbp->freeaddr)));
		assert(freeaddr <= jbp->rsrv_freeaddr);
		JNL_PHASE2_WRITE_COMPLETE(csa, jbp, commit_index, freeaddr);
	}
	jrs->tot_jrec_len = 0;	/* reset needed to prevent duplicate calls to "jnl_write_phase2" for same curr_tn */
}
