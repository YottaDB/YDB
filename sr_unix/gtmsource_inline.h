/****************************************************************
 *								*
 * Copyright (c) 2017-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMSOURCE_INLINE_H_INCLUDED
#define GTMSOURCE_INLINE_H_INCLUDED

#include "jnl.h"
#include "memcoherency.h"
#include "gtmsource.h"

static inline void jpl_phase2_write_complete(struct jnlpool_addrs_struct *jnlpool)
{
	int			index;
	jpl_phase2_in_prog_t	*phs2cmt;
	jnldata_hdr_ptr_t	jnl_header;
	jrec_prefix		*prefix;
	uint4			tot_jrec_len;

	GBLREF	uint4		process_id;
	GBLREF	jnl_gbls_t	jgbl;
	GBLREF	void		repl_phase2_cleanup(jnlpool_addrs *jpa);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	tot_jrec_len = jnlpool->jrs.tot_jrec_len;
	assert(tot_jrec_len);
	index = jnlpool->jrs.phase2_commit_index;
	ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(index, JPL_PHASE2_COMMIT_ARRAY_SIZE);
	phs2cmt = &jnlpool->jnlpool_ctl->phase2_commit_array[index];
	assert(phs2cmt->process_id == process_id);
	assert(FALSE == phs2cmt->write_complete);
	assert(phs2cmt->tot_jrec_len == tot_jrec_len);
	assert(jgbl.cumul_index == jgbl.cu_jnl_index);
	if (!jnlpool->jrs.memcpy_skipped)
	{
		assert(jnlpool->jrs.start_write_addr >= jnlpool->jnlpool_ctl->write_addr);
		assert(jnlpool->jrs.start_write_addr < jnlpool->jnlpool_ctl->rsrv_write_addr);
		jnl_header = (jnldata_hdr_ptr_t)(jnlpool->jnldata_base
				+ (jnlpool->jrs.start_write_addr % jnlpool->jnlpool_ctl->jnlpool_size));
		jnl_header->jnldata_len = tot_jrec_len;
		assert(0 == (phs2cmt->prev_jrec_len % JNL_REC_START_BNDRY));
		jnl_header->prev_jnldata_len = phs2cmt->prev_jrec_len;
		DEBUG_ONLY(prefix = (jrec_prefix *)(jnlpool->jnldata_base
					+ (jnlpool->jrs.start_write_addr + SIZEOF(jnldata_hdr_struct))
							% jnlpool->jnlpool_ctl->jnlpool_size));
		assert(JRT_BAD != prefix->jrec_type);
		if ((jnlpool->jrs.write_total != tot_jrec_len)
			DEBUG_ONLY(|| ((0 != TREF(gtm_test_jnlpool_sync))
					&& (0 == (phs2cmt->jnl_seqno % TREF(gtm_test_jnlpool_sync))))))
		{	/* This is an out-of-sync situation. "tot_jrec_len" (computed in phase1) is not equal
			 * to "write_total" (computed in phase2). Not sure how this can happen but recover
			 * from this situation by replacing the first record in the reserved space with a
			 * JRT_BAD rectype. That way the source server knows this is a transaction that it
			 * has to read from the jnlfiles and not the jnlpool.
			 */
			assert((0 != TREF(gtm_test_jnlpool_sync))
					&& (0 == (phs2cmt->jnl_seqno % TREF(gtm_test_jnlpool_sync))));
			assert(tot_jrec_len >= (SIZEOF(jnldata_hdr_struct) + SIZEOF(jrec_prefix)));
			/* Note that it is possible jnl_header is 8 bytes shy of the jnlpool end in which case
			 * "prefix" below would end up going outside the jnlpool range hence a simple
			 * (jnl_header + 1) would not work to set prefix (and instead the % needed below).
			 */
			prefix = (jrec_prefix *)(jnlpool->jnldata_base
					+ (jnlpool->jrs.start_write_addr + SIZEOF(jnldata_hdr_struct))
							% jnlpool->jnlpool_ctl->jnlpool_size);
			prefix->jrec_type = JRT_BAD;
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLRECOVERY, 4,
				tot_jrec_len, jnlpool->jrs.write_total,
				&phs2cmt->jnl_seqno, jnlpool->jnlpool_ctl->jnlpool_id.instfilename);
			/* Now that JRT_BAD is set, fix cur_write_addr so it is set back in sync
			 * (so later assert can succeed).
			 */
			DEBUG_ONLY(jnlpool->jrs.cur_write_addr = (jnlpool->jrs.start_write_addr + tot_jrec_len));
		}
		/* Need to make sure the writes of jnl_header->jnldata_len & jnl_header->prev_jnldata_len
		 * happen BEFORE the write of phs2cmt->write_complete in that order. Hence need the write
		 * memory barrier. Not doing this could cause another process in "repl_phase2_cleanup" to
		 * see phs2cmt->write_complete as TRUE and update jnlpool_ctl->write_addr to reflect this
		 * particular seqno even though the jnl_header write has not still happened. This could cause
		 * a concurrently running source server to decide to read this seqno in "gtmsource_readpool"
		 * and read garbage lengths in the jnl_header section.
		 */
		SHM_WRITE_MEMORY_BARRIER;
	}
	assert((jnlpool->jrs.start_write_addr + tot_jrec_len) == jnlpool->jrs.cur_write_addr);
	phs2cmt->write_complete = TRUE;
	jnlpool->jrs.tot_jrec_len = 0;	/* reset needed to prevent duplicate calls (e.g. "secshr_db_clnup") */
	/* Invoke "repl_phase2_cleanup" sparingly as it calls "grab_latch". So we do it twice.
	 * Once at half-way mark and once when a wrap occurs.
	 */
	if (!index || ((JPL_PHASE2_COMMIT_ARRAY_SIZE / 2) == index))
		repl_phase2_cleanup(jnlpool);
}
#endif
