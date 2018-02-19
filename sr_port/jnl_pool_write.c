/****************************************************************
 *								*
 * Copyright (c) 2007-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_inet.h"

#include <stddef.h> /* for offsetof() macro */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "iosp.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "min_max.h"
#include "sleep_cnt.h"
#include "jnl_write.h"
#include "copy.h"
#include "sleep.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		is_replicator;

/* This function writes the journal records ONLY TO the journal pool.
 *
 * csa 	   : sgmnt_addrs corresponding to region
 * rectype : Record type
 * jnl_rec : This contains fixed part of a variable size record or the complete fixed size records.
 * jfb     : For SET/KILL/ZKILL records entire record is formatted in this.
 */
void	jnl_pool_write(sgmnt_addrs *csa, enum jnl_record_type rectype, jnl_record *jnl_rec, jnl_format_buffer *jfb)
{
	boolean_t		pool_overflow;
	int			max_iters, num_iters, num_participants;
	uint4			dstlen, rlen;
	uint4			jnlpool_size, tot_jrec_len;
	uchar_ptr_t		jnlrecptr;
	jnlpool_addrs_ptr_t	local_jnlpool;
	jnlpool_ctl_ptr_t	jctl;
	jpl_rsrv_struct_t	*jrs;
	uint4			write, write_total;
	qw_off_t		cur_write_addr, end_write_addr;
	gtm_int64_t		wait_write_addr;	/* needed signed because of subtraction happening below */

	assert(is_replicator);
	assert(NULL != csa);
	local_jnlpool = JNLPOOL_FROM(csa);
	assert(NULL != local_jnlpool);
	assert(NULL != jnl_rec);
	assert(IS_VALID_RECTYPES_RANGE(rectype) && IS_REPLICATED(rectype));
	assert(JNL_ENABLED(csa) || REPL_WAS_ENABLED(csa));
	jctl = local_jnlpool->jnlpool_ctl;
	assert(NULL != jctl); /* ensure we haven't yet detached from the jnlpool */
	jrs = &local_jnlpool->jrs;
	jnlpool_size = jctl->jnlpool_size;
	tot_jrec_len = jrs->tot_jrec_len;
	DEBUG_ONLY(jgbl.cu_jnl_index++;)
	if (JRT_TCOM == rectype)
	{	/* If this is a TCOM record, check if this is a multi-region TP transaction. In that case, tp_tend would call
		 * "jnl_pool_write" with a sequence say TSET1, TCOM1, TSET2, TCOM2 (where 1 implies REG1, 2 implies REG2).
		 * But we cannot write this sequence into the journal pool as replication ("repl_sort_tr_buff" etc.) relies on
		 * all TCOMs coming at the end, i.e. the desired order is TSET1, TSET2, TCOM1, TCOM2. So adjust that by noting
		 * down if this TCOM is not the last one and if so skip writing it to the jnlpool and write it only when it is.
		 * Thankfully it is okay to write N copies of the last TCOM record to correspond to each of the N regions since
		 * replication does not care about region-specific information in the TCOM record (e.g. checksum, pini_addr etc.).
		 * All replication cares about is the time, seqno etc. which is all common across all the regions.
		 */
		num_participants = jnl_rec->jrec_tcom.num_participants;
		assert(jrs->num_tcoms < num_participants);
		if (++jrs->num_tcoms != num_participants)
			return;
		max_iters = num_participants;	/* write one TCOM record per region in a loop */
	} else
		max_iters = 1;
	rlen = jnl_rec->prefix.forwptr;
	assert(0 == rlen % JNL_REC_START_BNDRY);
	assert((rlen + SIZEOF(jnldata_hdr_struct)) <= tot_jrec_len);
	pool_overflow = (tot_jrec_len > jnlpool_size);
	write_total = jrs->write_total;
	cur_write_addr = jrs->cur_write_addr;
	for (num_iters = 0; num_iters < max_iters; num_iters++)
	{
		write_total += rlen;
		if (write_total > tot_jrec_len)
		{	/* "tot_jrec_len" (computed in phase1) becomes lesser than "write_total" (computed in phase2).
			 * There is not enough reserved space in the jnlpool to write the transaction's journal records.
			 * Skip writing any more records in the reserved space. A later call to JPL_PHASE2_WRITE_COMPLETE
			 * will know this happened by checking jrs->write_total and will take appropriate action.
			 * But continue "write_total" accumulation (used at end to set jrs->write_total) hence
			 * the "continue" below instead of a "break".
			 */
			assert(FALSE);
			continue;
		}
		assert(cur_write_addr >= jctl->write_addr);
		end_write_addr = cur_write_addr + rlen;
		assert(end_write_addr <= jctl->rsrv_write_addr);
		/* If we cannot fit in this whole transaction in the journal pool, source server will anyways read this
		 * transaction from the journal files. So skip the memcpy onto the jnlpool in the interest of time.
		 */
		if (!pool_overflow)
		{
			assert(!jrs->memcpy_skipped);
			/* Wait for jctl->write_addr to be high so we can go ahead with write
			 * without overflowing/underflowing the pool.
			 */
			wait_write_addr = (gtm_int64_t)end_write_addr - jnlpool_size;
			while ((gtm_int64_t)jctl->write_addr < wait_write_addr)
			{
				JPL_TRACE_PRO(jctl, jnl_pool_write_sleep);
				SLEEP_USEC(1, FALSE);
				/* TODO: Need to handle case of too-many "repl_phase2_cleanup" (and hence "is_proc_alive")
				 * calls by concurrent processes at same time. If we see a lot of the "jnl_pool_write_sleep"
				 * counter value then this will become a priority. For now we expect that counter to be ~ 0.
				 */
				repl_phase2_cleanup(local_jnlpool);
			}
			/* If the database is encrypted, then at this point jfb->buff will contain encrypted
			 * data which we don't want to to push into the jnlpool. Instead, we make use of the
			 * alternate alt_buff which is guaranteed to contain the original unencrypted data.
			 */
			if (jrt_fixed_size[rectype])
				jnlrecptr = (uchar_ptr_t)jnl_rec;
			else if (IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype) && USES_ANY_KEY(csa->hdr))
				jnlrecptr = (uchar_ptr_t)jfb->alt_buff;
			else
				jnlrecptr = (uchar_ptr_t)jfb->buff;
			write = cur_write_addr % jnlpool_size;
			dstlen = jnlpool_size - write;
			assert(rlen < jnlpool_size);	/* Because of "if (tot_jrec_len <= jnlpool_size)" above */
			/* Inspite of the above assert, do a "rlen < jnlpool_size" check below in pro to be safe */
			if (rlen <= dstlen)		/* dstlen & srclen >= rlen  (most frequent case) */
				memcpy(local_jnlpool->jnldata_base + write, jnlrecptr, rlen);
			else if (rlen < jnlpool_size)	/* dstlen < rlen <= jnlpool_size */
			{
				memcpy(local_jnlpool->jnldata_base + write, jnlrecptr, dstlen);
				memcpy(local_jnlpool->jnldata_base, jnlrecptr + dstlen, rlen - dstlen);
			}
		} else
			jrs->memcpy_skipped = TRUE;
		cur_write_addr = end_write_addr;
	}
	assert(end_write_addr > jrs->cur_write_addr);
	jrs->cur_write_addr = end_write_addr;
	jrs->write_total = write_total;
	return;
}
