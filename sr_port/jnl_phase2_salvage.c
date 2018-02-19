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
#include "jnl_get_checksum.h"
#include "jnl_write.h"
#include "send_msg.h"
#ifdef DEBUG
#include "is_proc_alive.h"
#endif

error_def(ERR_JNLBUFFPHS2SALVAGE);

/* The below code is very similar to "repl_phase2_salvage" */
/* This function is invoked when we find a process that had reserved space in the journal buffers for its journal records
 * died before it finished its transaction commit in phase2 (outside crit). Since many other reservations could have happened
 * after that and those could correspond to live processes, we now have a situation where there is a hole/gap in the journal
 * file. The below function patches those holes by filling them with JRT_NULL/JRT_INCTN/JRT_ALIGN records to indicate some
 * transaction happened. This keeps the jnl_seqno/curr_tn contiguity necessary for journal recovery, replication etc.
 * The actual transaction's journal records are lost permanently but that is okay since the process had not touched the
 * database for that commit (it would have started phase2 of db commit only after all phase2 jnl records were written to
 * the journal buffer and got shot before that finished).
 */
void	jnl_phase2_salvage(sgmnt_addrs *csa, jnl_buffer_ptr_t jbp, jbuf_phase2_in_prog_t *deadCmt)
{
	boolean_t		write_null_record;
	uint4			next_align_addr, start_freeaddr, end_freeaddr, save_phase2_freeaddr;
	uint4			alignsize, pini_addr, rlen, tot_jrec_len;
	struct_jrec_null	null_rec;
	struct_jrec_inctn	inctn_rec;
	jnl_record		*jrec;
	jnl_private_control	*jpc;

	assert(!is_proc_alive(deadCmt->process_id, 0));
	if (!deadCmt->in_phase2)
	{	/* A process in "jnl_write" got killed after it did the UPDATE_JBP_RSRV_FREEADDR call but before it did
		 * the JNL_PHASE2_WRITE_COMPLETE call. The corresponding journal record was already written to the journal
		 * buffer before the UPDATE_JBP_RSRV_FREEADDR call so all that is needed is to signal the phase2 as complete.
		 * We cannot call the JNL_PHASE2_WRITE_COMPLETE macro (which does this signaling) directly as it will fail asserts.
		 * Hence do its equivalent before returning right away.
		 */
		deadCmt->write_complete = TRUE;
		return;
	}
	/* This commit entry was added by UPDATE_JRS_RSRV_FREEADDR in t_end/tp_tend. And so we are guaranteed that the journal
	 * records added are not PFIN or EOF which have sizes of 0x20 and 0x28 respectively. This means the minimum jnl rec len
	 * of this transaction is guaranteed to be 0x30 which is the length of a JRT_NULL record. And so it is possible for us
	 * to replace this transaction's journal records with a NULL record (if replication is turned on) OR with an INCTN
	 * journal record (if replication is turned OFF). And pad the rest of the space with JRT_ALIGN records as needed.
	 *
	 * Below is an explanation of why we can always replace an existing transaction's journal records with a sequence of
	 * JRT_NULL/JRT_INCTN and one or more JRT_ALIGN records.
	 *
	 * The minimum size of a JRT_SET/JRT_KILL etc. (any logical update record) is 0x40. So the minimum size of the total
	 * journal record size of the transaction is at least 0x40.
	 * (a) If replicating, we can then replace this with a JRT_NULL record (size = 0x30) + JRT_ALIGN (size = 0x10, which is
	 *	the minimum align record size and so 0-byte padding). If the transaction journal record size is > 0x40, we can
	 *	add enough padding in the JRT_ALIGN and add a few more of JRT_ALIGN records as required if the transaction
	 *	spans multiple align boundaries). In this case, the JRT_ALIGN is overloaded to also act as a filler journal record.
	 * (b) If not replicating, we can then replace this with a JRT_INCTN record (size = 0x28) + JRT_ALIGN (size = 0x18, where
	 *	the align record has 8-byte of padding). If > 0x40, then add more padding just like case (a) above.
	 * It is possible the transaction is not a logical one (e.g. inctn due to file extension, 2nd phase of kill, reorg etc.).
	 *	In that case, the transaction journal size would be just a JRT_INCTN (0x28) but in that case we should be
	 *	able to replace it with another JRT_INCTN instead.
	 * This is asserted below and in "UPDATE_JBP_RSRV_FREEADDR" macro.
	 */
	tot_jrec_len = deadCmt->tot_jrec_len;
	assert((!deadCmt->replication && (INCTN_RECLEN == tot_jrec_len))
		|| (deadCmt->replication && (NULL_RECLEN == tot_jrec_len))
		|| ((tot_jrec_len >= (NULL_RECLEN + MIN_ALIGN_RECLEN)) && (tot_jrec_len >= (INCTN_RECLEN + MIN_ALIGN_RECLEN))));
	start_freeaddr = deadCmt->start_freeaddr;
	send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLBUFFPHS2SALVAGE, 7, deadCmt->process_id, DB_LEN_STR(csa->region),
						&deadCmt->curr_tn, &deadCmt->jnl_seqno, start_freeaddr, tot_jrec_len);
	alignsize = jbp->alignsize;
	/* Use "gtm_uint64_t" typecast below to avoid 4G overflow issues with the ROUND_UP2 */
	next_align_addr = (uint4)(ROUND_UP2((gtm_uint64_t)start_freeaddr, alignsize) - MIN_ALIGN_RECLEN);
	assert(start_freeaddr <= next_align_addr);
	jpc = csa->jnl;
	/* Temporarily adjust jpc->phase2_freeaddr to reflect the state of the dead pid so we write jnl records where it
	 * would have written in the jnl buffer or jnl file.
	 */
	save_phase2_freeaddr = jpc->phase2_freeaddr;
	assert(jpc->phase2_free == jpc->phase2_freeaddr % jbp->size);
	assert(!jpc->in_jnl_phase2_salvage);
	jpc->in_jnl_phase2_salvage = TRUE;
	SET_JPC_PHASE2_FREEADDR(jpc, jbp, start_freeaddr);	/* needed by "jnl_write_align_rec" etc. calls below */
	end_freeaddr = start_freeaddr + tot_jrec_len;
	write_null_record = REPL_ALLOWED(csa) && deadCmt->replication;	/* see comment in jnl.h (jbuf_phase2_in_prog_t.replication)
									 * for why it is necessary to decide JRT_NULL vs JRT_INCTN.
									 */
	assert(!write_null_record || deadCmt->jnl_seqno);
	rlen = write_null_record ? NULL_RECLEN : INCTN_RECLEN;
	pini_addr = deadCmt->pini_addr;
	if (!pini_addr)
		pini_addr = JNL_FILE_FIRST_RECORD;
	if ((start_freeaddr + rlen) > next_align_addr)
	{	/* Write an ALIGN record first */
		jnl_write_align_rec(csa, next_align_addr - start_freeaddr, deadCmt->jrec_time);
		DEBUG_ONLY(start_freeaddr = jpc->phase2_freeaddr;)
		assert(0 == (start_freeaddr % alignsize));
		assert(start_freeaddr == (next_align_addr + MIN_ALIGN_RECLEN));
		next_align_addr += alignsize;
		assert(start_freeaddr < next_align_addr);
		assert(start_freeaddr <= end_freeaddr);
	}
	assert(start_freeaddr + rlen <= end_freeaddr);
	if (write_null_record)
	{	/* Write JRT_NULL record */
		assert(NULL_RECLEN == rlen);
		null_rec.prefix.jrec_type = JRT_NULL;
		null_rec.prefix.forwptr = NULL_RECLEN;
		null_rec.prefix.pini_addr = pini_addr;
		null_rec.prefix.time = deadCmt->jrec_time;
		null_rec.prefix.checksum = INIT_CHECKSUM_SEED;
		null_rec.prefix.tn = deadCmt->curr_tn;
		assert(deadCmt->jnl_seqno);
		null_rec.jnl_seqno = deadCmt->jnl_seqno;
		null_rec.strm_seqno = deadCmt->strm_seqno;
		null_rec.filler = 0;
		null_rec.suffix.backptr = NULL_RECLEN;
		null_rec.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		jrec = (jnl_record *)&null_rec;
		null_rec.prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)jrec, NULL_RECLEN);
	} else
	{	/* Write JRT_INCTN record */
		inctn_rec.prefix.jrec_type = JRT_INCTN;
		inctn_rec.prefix.forwptr = INCTN_RECLEN;
		inctn_rec.prefix.pini_addr = pini_addr;
		inctn_rec.prefix.time = deadCmt->jrec_time;
		inctn_rec.prefix.checksum = INIT_CHECKSUM_SEED;
		inctn_rec.prefix.tn = deadCmt->curr_tn;
		inctn_rec.detail.blknum_struct.opcode = inctn_jnlphase2salvage;
		inctn_rec.detail.blknum_struct.filler_uint4 = 0;
		inctn_rec.detail.blknum_struct.filler_short = 0;
		inctn_rec.detail.blknum_struct.suffix.backptr = INCTN_RECLEN;
		inctn_rec.detail.blknum_struct.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		jrec = (jnl_record *)&inctn_rec;
		inctn_rec.prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)jrec, INCTN_RECLEN);
		deadCmt->jnl_seqno = 0;	/* for the send_msg_csa call below */
	}
	jnl_write(jpc, jrec->prefix.jrec_type, jrec, NULL);
	assert(start_freeaddr + rlen == jpc->phase2_freeaddr);
	while (next_align_addr < end_freeaddr)
	{	/* Write one or more JRT_ALIGN records to fill up one "alignsize" space. Note that one JRT_ALIGN record
		 * size cannot exceed jbp->max_jrec_len which could be way less than the current "alignsize" and hence
		 * the call to "jnl_write_multi_align_rec" instead of "jnl_write_align_rec" directly.
		 */
		jnl_write_multi_align_rec(csa, next_align_addr - jpc->phase2_freeaddr, deadCmt->jrec_time);
		assert(0 == (jpc->phase2_freeaddr % alignsize));
		assert(jpc->phase2_freeaddr == (next_align_addr + MIN_ALIGN_RECLEN));
		next_align_addr += alignsize;
		assert(start_freeaddr <= end_freeaddr);
	}
	if (end_freeaddr != jpc->phase2_freeaddr)
	{
		assert((end_freeaddr - jpc->phase2_freeaddr) >= MIN_ALIGN_RECLEN);
		/* Write one last set of JRT_ALIGN record(s) */
		jnl_write_multi_align_rec(csa, end_freeaddr - jpc->phase2_freeaddr - MIN_ALIGN_RECLEN, deadCmt->jrec_time);
		assert(jpc->phase2_freeaddr == end_freeaddr);
	}
	jpc->in_jnl_phase2_salvage = FALSE;
	SET_JPC_PHASE2_FREEADDR(jpc, jbp, save_phase2_freeaddr);	/* End simulation of dead pid jnl writing */
	deadCmt->write_complete = TRUE;	/* signal dead pid's jnl write is now complete */
}
