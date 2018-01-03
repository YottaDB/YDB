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
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl_get_checksum.h"
#ifdef DEBUG
#include "is_proc_alive.h"
#endif

error_def(ERR_JNLPOOLPHS2SALVAGE);

/* The below code is very similar to "jnl_phase2_salvage" except that this salvages jnl records in jnlpool */
void	repl_phase2_salvage(jnlpool_addrs *jpa, jnlpool_ctl_ptr_t jpl, jpl_phase2_in_prog_t *deadCmt)
{
	struct_jrec_null	null_rec;
	qw_off_t		start_write_addr, rsrv_write_addr;
	uint4			jnlpool_size, write;
	uint4			dstlen, rlen;
	uchar_ptr_t		jnlrecptr;
	jnldata_hdr_ptr_t	jnl_header;
	sm_uc_ptr_t		jnldata_base;

	assert(jpa && jpl && (jpl == jpa->jnlpool_ctl));
	assert((&FILE_INFO(jpa->jnlpool_dummy_reg)->s_addrs)->now_crit);
	assert(!is_proc_alive(deadCmt->process_id, 0));
	/* This commit entry was added by UPDATE_JPL_RSRV_WRITE_ADDR in t_end/tp_tend. Just like "jnl_phase2_salvage", we
	 * replace the reserved space for logical journal records in the jnlpool with one JRT_NULL record. But unlike
	 * "jnl_phase2_salvage", the rest of the reserved space is not filled with JRT_ALIGN records. Instead we just
	 * fix the source server to check if the first record it read from the pool is a NULL record and if so ignore
	 * everything else except the NULL record.
	 */
	assert(NULL_RECLEN <= deadCmt->tot_jrec_len);
	start_write_addr = deadCmt->start_write_addr;
	rsrv_write_addr = jpl->rsrv_write_addr;
	assert(start_write_addr < rsrv_write_addr);
	assert(deadCmt->jnl_seqno);
	assert(FALSE == deadCmt->write_complete);
	assert(deadCmt->tot_jrec_len <= (rsrv_write_addr - start_write_addr));
	jnlpool_size = jpl->jnlpool_size;
	if ((rsrv_write_addr - start_write_addr) <= jnlpool_size)
	{	/* The reserved space for this seqno is still somewhere in the journal pool (has not yet overflown the pool).
		 * So fill the space with a JRT_NULL record.
		 */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_JNLPOOLPHS2SALVAGE, 6, deadCmt->process_id,
			DB_LEN_STR(jpa->jnlpool_dummy_reg), &deadCmt->jnl_seqno, &start_write_addr, deadCmt->tot_jrec_len);
		null_rec.prefix.jrec_type = JRT_NULL;
		null_rec.prefix.forwptr = NULL_RECLEN;
		null_rec.prefix.pini_addr = 0;			/* this field does not matter in replication */
		null_rec.prefix.time = 0;			/* this field does not matter in replication */
		null_rec.prefix.checksum = INIT_CHECKSUM_SEED;	/* this field does not matter in replication */
		null_rec.prefix.tn = TN_INVALID;		/* this field does not matter in replication */
		assert(deadCmt->jnl_seqno);
		null_rec.jnl_seqno = deadCmt->jnl_seqno;
		null_rec.strm_seqno = deadCmt->strm_seqno;
		null_rec.filler = 0;
		null_rec.suffix.backptr = NULL_RECLEN;
		null_rec.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		write = (start_write_addr % jnlpool_size);
		jnldata_base = (sm_uc_ptr_t)jpl + JNLDATA_BASE_OFF; /* Cannot use jpa->jnldata_base as it might not be initialized
								     * for online rollback (does not go through "jnlpool_init").
								     */
		jnl_header = (jnldata_hdr_ptr_t)(jnldata_base + write);
		jnl_header->jnldata_len = deadCmt->tot_jrec_len;
		jnl_header->prev_jnldata_len = deadCmt->prev_jrec_len;
		write += SIZEOF(jnldata_hdr_struct);
		dstlen = jnlpool_size - write;
		rlen = NULL_RECLEN;
		jnlrecptr = (uchar_ptr_t)&null_rec;
		assert(rlen < jnlpool_size);
		if (rlen <= dstlen)		/* dstlen & srclen >= rlen  (most frequent case) */
			memcpy(jnldata_base + write, &null_rec, rlen);
		else				/* dstlen < rlen <= jnlpool_size */
		{
			memcpy(jnldata_base + write, jnlrecptr, dstlen);
			memcpy(jnldata_base, jnlrecptr + dstlen, rlen - dstlen);
		}
	}
	deadCmt->write_complete = TRUE;	/* signal dead pid's jnl write is now complete */
}
