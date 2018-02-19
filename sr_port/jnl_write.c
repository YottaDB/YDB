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
#include "repl_msg.h"		/* needed by gtmsource.h */
#include "gtmsource.h"
#include "min_max.h"
#include "sleep_cnt.h"
#include "jnl_write.h"
#include "copy.h"
#include "jnl_get_checksum.h"
#include "is_proc_alive.h"
#include "wbox_test_init.h"
#include "gtmimagename.h"
#include "memcoherency.h"
#include "interlock.h"
#include "gdsbgtr.h"
#ifdef DEBUG
#include "gdskill.h"		/* needed for tp.h */
#include "gdscc.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#endif

GBLREF	uint4			process_id;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		in_jnl_file_autoswitch;
GBLREF	uint4			dollar_tlevel;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	jnl_gbls_t		jgbl;

#ifdef DEBUG
#define	MAX_JNL_WRITE_RECURSION_DEPTH	3
STATICDEF	int		jnl_write_recursion_depth;
#endif

error_def(ERR_JNLEXTEND);
error_def(ERR_JNLWRTNOWWRTR);
error_def(ERR_JNLWRTDEFER);

#define	JNL_PUTSTR(lcl_free, lcl_buff, src, len, lcl_size)			\
{										\
	uint4	size_before_wrap;						\
										\
	assert(len <= lcl_size);						\
	size_before_wrap = lcl_size - lcl_free;					\
	if (len <= size_before_wrap)						\
	{									\
		memcpy(&lcl_buff[lcl_free], src, len);				\
		lcl_free += len;						\
		if (len == size_before_wrap)					\
			lcl_free = 0;						\
	} else									\
	{									\
		memcpy(&lcl_buff[lcl_free], src, size_before_wrap);		\
		lcl_free = len - size_before_wrap;				\
		memcpy(&lcl_buff[0], src + size_before_wrap, lcl_free);		\
	}									\
}

/* jpc 	   : Journal private control
 * rectype : Record type
 * jnl_rec : This contains fixed part of a variable size record or the complete fixed size records.
 * parm1   : For JRT_PBLK and JRT_AIMG this has the block image (blk_ptr)
 *         : For SET/KILL/ZKILL/ZTWORM/LGTRIG/ZTRIG records it contains the journal-format-buffer (jfb)
 */
void	jnl_write(jnl_private_control *jpc, enum jnl_record_type rectype, jnl_record *jnl_rec, void *parm1)
{
	uint4			align_filler_len, rlen, lcl_size, lcl_free, lcl_orig_free, next_align_addr;
	jnl_buffer_ptr_t	jb;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	struct_jrec_align	*jrec_align;
	jrec_suffix		suffix;
	boolean_t		nowrap, in_phase2, is_align;
	struct_jrec_blk		*jrec_blk;
	uint4			checksum, lcl_freeaddr, new_freeaddr, start_freeaddr;
	gtm_int64_t		min_dskaddr;
	sm_uc_ptr_t		lcl_buff;
	gd_region		*reg;
	int			commit_index, index2;
	uint4			jnl_fs_block_size, aligned_lcl_free, padding_size;
	blk_hdr_ptr_t		blk_ptr;
	jnl_format_buffer	*jfb;
	jbuf_phase2_in_prog_t	*phs2cmt;
	boolean_t		was_latch_owner;
#	ifdef DEBUG
	uint4			tmp_csum1, tmp_csum2;
	uint4			mumps_node_sz;
	char			*mumps_node_ptr;
	struct_jrec_align	*align_rec;
	uint4			end_freeaddr;
#	endif

	assert(MAX_JNL_WRITE_RECURSION_DEPTH > jnl_write_recursion_depth++);
	reg = jpc->region;
	csa = &FILE_INFO(reg)->s_addrs;
	jb = jpc->jnl_buff;
	in_phase2 = (IN_PHASE2_JNL_COMMIT(csa) || jpc->in_jnl_phase2_salvage);
	lcl_freeaddr = JB_FREEADDR_APPROPRIATE(in_phase2, jpc, jb);
	assert(NULL != jnl_rec);
	rlen = jnl_rec->prefix.forwptr;
	assert(((gtm_uint64_t)lcl_freeaddr + rlen) < MAXUINT4);	/* so the below + can be done without gtm_uint64_t typecast */
	new_freeaddr = lcl_freeaddr + rlen;
	is_align = (JRT_ALIGN == rectype);
	if (!in_phase2 && !is_align)
	{
		next_align_addr = jb->next_align_addr;
		assert(lcl_freeaddr <= next_align_addr);
		if (new_freeaddr > next_align_addr)
		{	/* If we have to write an ALIGN record here before writing a PINI record, thankfully there is no issue.
			 * This is because an ALIGN record does not have a pini_addr (if it did then we would have used
			 * JNL_FILE_FIRST_RECORD, the pini_addr field of the first PINI journal record in the journal file).
			 */
			jb->next_align_addr += jb->alignsize;
			assert(jb->next_align_addr < MAXUINT4);
			assert(new_freeaddr < jb->next_align_addr);
			jnl_write_align_rec(csa, next_align_addr - lcl_freeaddr, jnl_rec->prefix.time);
			lcl_freeaddr = jb->rsrv_freeaddr; /* can safely examine this because "in_phase2" is 0 at this point */
			new_freeaddr = lcl_freeaddr + rlen;
			/* Now that we have written an ALIGN record, move on to write the requested "rectype" which is guaranteed
			 * to not be an ALIGN record (since "is_align" is FALSE).
			 */
			if (JRT_PINI == rectype)
			{
				jnl_rec->prefix.pini_addr = lcl_freeaddr;
				/* Checksum needs to be recomputed since prefix.pini_addr is changed in above statement */
				jnl_rec->prefix.checksum = INIT_CHECKSUM_SEED;
				jnl_rec->prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED,
								(unsigned char *)&jnl_rec->jrec_pini, SIZEOF(struct_jrec_pini));
			}
		}
	}
	csd = csa->hdr;
#	ifdef DEBUG
	/* Ensure that journaling is turned ON in this database file if we are about to write a journal record AND that
	 * no replicated journal record is written by this routine if replication is in WAS_ON state.
	 * The only exception for journaling being turned OFF is if we are in phase2 (outside crit) and had reserved
	 * space during phase1 but journaling got turned OFF in between phase1 and phase2. In that case JNL_ENABLED(csa)
	 * would still be TRUE and it is okay to continue writing the journal records since we are writing in the
	 * reserved space only (no need to flush etc. for the most part). The only exception is if we need to invoke
	 * "jnl_write_attempt" below and that invokes "jnl_file_lost". In that case, jpc->channel would be set to NOJNL
	 * and we return right away. "jnl_write_phase2" has an assert which takes this scenario into account.
	 */
	assert((JNL_ENABLED(csd) && (!REPL_WAS_ENABLED(csd) || !jrt_is_replicated[rectype])) || (in_phase2 && JNL_ENABLED(csa)));
	/* Assert that the only journal records that the source server ever writes are PINI/PFIN/EPOCH/EOF
	 * which it does at the very end when the database is about to be shut down. The only exception is
	 * an ALIGN record which is an indirectly written record and a NULL record which it could write on behalf
	 * of another GT.M process (in "jnl_phase2_salvage"). So account for that too.
	 */
	assert(!is_src_server || (JRT_EOF == rectype) || (JRT_PINI == rectype)
			|| (JRT_EPOCH == rectype) || (JRT_PFIN == rectype) || (JRT_ALIGN == rectype) || (JRT_NULL == rectype));
	assert(csa->now_crit || in_phase2 || (csd->clustered  &&  (csa->nl->ccp_state == CCST_CLOSED)));
	assert(IS_VALID_RECTYPES_RANGE(rectype));
#	endif
	lcl_free = JB_FREE_APPROPRIATE(in_phase2, jpc, jb);
	lcl_size = jb->size;
	lcl_buff = &jb->buff[jb->buff_off];
	++jb->reccnt[rectype];
	/* Do high-level check on rlen */
	assert(rlen <= jb->max_jrec_len);
	/* Do fine-grained checks on rlen */
	GTMTRIG_ONLY(assert(!IS_ZTWORM(rectype) || (MAX_ZTWORM_JREC_LEN >= rlen));)		/* ZTWORMHOLE */
	GTMTRIG_ONLY(assert(!IS_LGTRIG(rectype) || (MAX_LGTRIG_JREC_LEN >= rlen));)		/* LGTRIG */
	assert(!IS_SET_KILL_ZKILL_ZTRIG(rectype) || (JNL_MAX_SET_KILL_RECLEN(csd) >= rlen));	/* SET, KILL, ZKILL, ZTRIG */
	jb->bytcnt += rlen;
	assert (0 == rlen % JNL_REC_START_BNDRY);
	cnl = csa->nl;
	/* If we are currently extending the journal file and writing the closing part of journal records,
	 * it better be the records that we expect. This is because we will skip the padding check for these
	 * records. The macro JNL_FILE_TAIL_PRESERVE already takes into account padding space for these.
	 */
	assert(!in_jnl_file_autoswitch
		|| (JRT_PINI == rectype) || (JRT_PFIN == rectype) || (JRT_EPOCH == rectype)
		|| (JRT_INCTN == rectype) || (JRT_EOF == rectype) || (JRT_ALIGN == rectype));
	checksum = GET_JREC_CHECKSUM(jnl_rec, rectype);
	assert(checksum);
#	ifdef DEBUG
	/* Ensure that the checksum computed earlier in jnl_format or jnl_write_pblk or jnl_write_aimg_rec
	 * or fixed-sized records matches with the block's content.
	 */
	blk_ptr = NULL;
	jfb = NULL;
	if ((JRT_PBLK == rectype) || (JRT_AIMG == rectype))
	{
		blk_ptr = (blk_hdr_ptr_t)parm1;
		assert(JNL_MAX_PBLK_RECLEN(csd) >= rlen);			/* PBLK and AIMG */
		COMPUTE_COMMON_CHECKSUM(tmp_csum2, jnl_rec->prefix);
		tmp_csum1 = jnl_get_checksum(blk_ptr, NULL, jnl_rec->jrec_pblk.bsiz);
		COMPUTE_PBLK_CHECKSUM(tmp_csum1, &jnl_rec->jrec_pblk, tmp_csum2, tmp_csum1);
	} else if (IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype))
	{
		jfb = (jnl_format_buffer *)parm1;
		COMPUTE_COMMON_CHECKSUM(tmp_csum2, jnl_rec->prefix);
		mumps_node_ptr = jfb->buff + FIXED_UPD_RECLEN;
		mumps_node_sz = jfb->record_size - (FIXED_UPD_RECLEN + JREC_SUFFIX_SIZE);
		tmp_csum1 = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)mumps_node_ptr, mumps_node_sz);
		COMPUTE_LOGICAL_REC_CHECKSUM(tmp_csum1, &jnl_rec->jrec_set_kill, tmp_csum2, tmp_csum1);
	} else if (jrt_fixed_size[rectype])
	{
		jnl_rec->prefix.checksum = INIT_CHECKSUM_SEED;
		assert(JRT_TRIPLE != rectype);
		assert(JRT_HISTREC != rectype);
		tmp_csum1 = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)jnl_rec, jnl_rec->prefix.forwptr);
		jnl_rec->prefix.checksum = checksum;
	} else if (is_align)
	{	/* Note: "struct_jrec_align" has a different layout (e.g. "checksum" at different offset etc.) than all
		 * other jnl records. So handle this specially.
		 */
		align_rec = (struct_jrec_align *)jnl_rec;
		align_rec->checksum = INIT_CHECKSUM_SEED;
		tmp_csum1 = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)jnl_rec, FIXED_ALIGN_RECLEN);
		align_rec->checksum = checksum;
	} else
		assert(FALSE);
	/* It is possible "jnl_file_lost" was invoked by us during phase2 of commit in which case jpc->pini_addr would have
	 * been reset to 0 and so the checksums might not match. Allow for that in the below assert.
	 */
	assert((checksum == tmp_csum1) || ((!JNL_ENABLED(csd)) && (JNL_ENABLED(csa))));
#	endif
	ADJUST_CHECKSUM(checksum, lcl_freeaddr, checksum);
	ADJUST_CHECKSUM(checksum, csd->jnl_checksum, checksum);
	SET_JREC_CHECKSUM(jnl_rec, rectype, checksum);
	if (!in_phase2)
	{
		assert((!jb->blocked) || (FALSE == is_proc_alive(jb->blocked, 0)));
		jb->blocked = process_id;
	}
	jnl_fs_block_size = jb->fs_block_size;
	min_dskaddr = (gtm_int64_t)new_freeaddr - lcl_size + jnl_fs_block_size;	/* gtm_int64_t used as result can be negative */
	/* If jb->dskaddr >= min_dskaddr, we are guaranteed we can write "rlen" bytes at "lcl_freeaddr" without
	 * overflowing the journal buffer and/or overwriting the filesystem-aligned block before "jb->dskaddr" in jnl buffer.
	 * The check is not exact in the sense we might invoke "jnl_write_attempt" even if it is not necessary but it is
	 * very rare and this avoids us from always having to use a ROUND_UP2 or "& ~(jb->fs_block_size - 1)" operation.
	 */
	if ((gtm_int64_t)jb->dskaddr < min_dskaddr)
	{
		if (csa->now_crit && in_phase2 && ((gtm_int64_t)jb->freeaddr < min_dskaddr))
		{	/* Holding crit AND phase2. Find corresponding phase2 commit entry in jb.
			 * Check if that commit entry writes journal records more than journal buffer size.
			 * If so, we need to adjust jb->freeaddr in the middle of the transaction thereby
			 * "jnl_write_attempt" can flush partial transaction data and make space for the remaining data.
			 */
			index2 = jb->phase2_commit_index2;
			assert(jb->phase2_commit_index1 != index2);
			ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(index2, JNL_PHASE2_COMMIT_ARRAY_SIZE);
			DECR_PHASE2_COMMIT_INDEX(index2, JNL_PHASE2_COMMIT_ARRAY_SIZE);
			phs2cmt = &jb->phase2_commit_array[index2];
			assert(phs2cmt->process_id == process_id);
			assert(!phs2cmt->write_complete);
			start_freeaddr = phs2cmt->start_freeaddr;
			/* If min_dskaddr < start_freeaddr, all we need is to flush out any phase2 commits before the
			 * current one so we can directly go to do a "jnl_write_attempt(min_dskaddr)" below which
			 * automatically takes care of that. Otherwise, we need to flush all prior phase2 commits
			 * first (i.e. "jnl_write_attempt(start_freeaddr)") and then adjust jb->freeaddr based on
			 * the ongoing/current phase2 commit as it fills up the journal buffer with jnl records.
			 */
			if (min_dskaddr >= start_freeaddr)
			{
				assert(dollar_tlevel);
				assert(phs2cmt->tot_jrec_len > (lcl_size - jnl_fs_block_size));
				DEBUG_ONLY(end_freeaddr = start_freeaddr + phs2cmt->tot_jrec_len);
				assert(end_freeaddr >= new_freeaddr);
				if ((jb->freeaddr < start_freeaddr) && (SS_NORMAL != jnl_write_attempt(jpc, start_freeaddr)))
				{
					assert(NOJNL == jpc->channel); /* jnl file lost */
					DEBUG_ONLY(jnl_write_recursion_depth--);
					return; /* let the caller handle the error */
				}
				assert(jb->freeaddr >= start_freeaddr);
				/* It is possible we already own the latch in case we are in timer-interrupt
				 * or process-exit code hence the below check.
				 */
				was_latch_owner = GLOBAL_LATCH_HELD_BY_US(&jb->phase2_commit_latch);
				if (!was_latch_owner)
				{	/* Return value of "grab_latch" does not need to be checked
					 * because we pass GRAB_LATCH_INDEFINITE_WAIT as timeout.
					 */
					grab_latch(&jb->phase2_commit_latch, GRAB_LATCH_INDEFINITE_WAIT);
				}
				SET_JBP_FREEADDR(jb, lcl_freeaddr);
				if (!was_latch_owner)
					rel_latch(&jb->phase2_commit_latch);
			}
		}
		if (SS_NORMAL != jnl_write_attempt(jpc, min_dskaddr))
		{
			assert(NOJNL == jpc->channel); /* jnl file lost */
			DEBUG_ONLY(jnl_write_recursion_depth--);
			return; /* let the caller handle the error */
		}
	}
	/* If we are in "phase2", the call to "jnl_write_reserve" would have done the needed "jnl_file_extend" calls in phase1
	 * so no need to do "jnl_write_extend_if_needed" if "in_phase2" is TRUE.
	 */
	if (!in_phase2)
	{
		jb->blocked = 0;
		if (0 != jnl_write_extend_if_needed(rlen, jb, lcl_freeaddr, csa, rectype, blk_ptr, jfb, reg, jpc, jnl_rec))
		{
			DEBUG_ONLY(jnl_write_recursion_depth--);
			return; /* let the caller handle the error */
		}
	}
	lcl_orig_free = lcl_free;
	nowrap = (lcl_size >= (lcl_free + rlen));
	assert(jrt_fixed_size[JRT_EOF]);
	if (jrt_fixed_size[rectype])
	{
		if (nowrap)
		{
			memcpy(lcl_buff + lcl_free, (uchar_ptr_t)jnl_rec, rlen);
			lcl_free += rlen;
			if (lcl_size == lcl_free)
				lcl_free = 0;
		} else
			JNL_PUTSTR(lcl_free, lcl_buff, (uchar_ptr_t)jnl_rec, rlen, lcl_size);
		/* As part of writing the EOF record into the journal buffer, add enough 0-padding needed to reach
		 * a filesystem-block-size aligned boundary. This way later jnl_qio_start can safely do aligned
		 * writes without having to write non-zero garbage after the EOF record. Note that this has to be
		 * done BEFORE updating freeaddr. Otherwise, it is possible that a jnl qio timer pops after freeaddr
		 * gets updated but before the 0-padding is done and flushes the eof record to disk without the 0-padding.
		 */
		if (JRT_EOF == rectype)
		{
			assert(!in_phase2);
			aligned_lcl_free = ROUND_UP2(lcl_free, jnl_fs_block_size);
			padding_size = aligned_lcl_free - lcl_free;
			if (padding_size)
				memset(lcl_buff + lcl_free, 0, padding_size);
		}
		if (JRT_EPOCH != rectype)
			INCR_GVSTATS_COUNTER(csa, cnl, n_jrec_other, 1);
		/* else for EPOCH, the increment of JRE or JRI is done after "jnl_write_epoch_rec" in caller */
	} else
	{
		switch(rectype)
		{
			case JRT_PBLK:
			case JRT_AIMG:
				blk_ptr = (blk_hdr_ptr_t)parm1;
				assert(NULL != blk_ptr);	/* PBLK and AIMG */
				assert(FIXED_BLK_RECLEN == FIXED_PBLK_RECLEN);
				assert(FIXED_BLK_RECLEN == FIXED_AIMG_RECLEN);
				jrec_blk = (struct_jrec_blk *)jnl_rec;
				suffix.backptr = rlen;
				suffix.suffix_code = JNL_REC_SUFFIX_CODE;
				if (nowrap)
				{	/* write fixed part of record before the actual gds block image */
					memcpy(lcl_buff + lcl_free, (uchar_ptr_t)jnl_rec, FIXED_BLK_RECLEN);
					lcl_free += (int4)FIXED_BLK_RECLEN;
					/* write actual block */
					memcpy(lcl_buff + lcl_free, (uchar_ptr_t)blk_ptr, jrec_blk->bsiz);
					lcl_free += jrec_blk->bsiz;
				} else
				{	/* write fixed part of record before the actual gds block image */
					JNL_PUTSTR(lcl_free, lcl_buff, (uchar_ptr_t)jnl_rec, (int4)FIXED_BLK_RECLEN, lcl_size);
					/* write actual block */
					JNL_PUTSTR(lcl_free, lcl_buff, (uchar_ptr_t)blk_ptr, jrec_blk->bsiz, lcl_size);
				}
				/* Skip over a few characters for 8-bye alignment and then write suffix */
				assert(lcl_free <= lcl_size);
				lcl_free = ROUND_UP2(lcl_free + JREC_SUFFIX_SIZE, JNL_REC_START_BNDRY);
				if (lcl_free > lcl_size)
				{
					assert(lcl_free == (lcl_size + JNL_REC_START_BNDRY));
					lcl_free = JNL_REC_START_BNDRY;
				}
				*((jrec_suffix *)(lcl_buff + lcl_free - JREC_SUFFIX_SIZE)) = suffix;
				assert(lcl_free <= lcl_size);
				if (lcl_size == lcl_free)
					lcl_free = 0;
				INCR_GVSTATS_COUNTER(csa, cnl, n_jrec_pblk, 1);
				break;
			case JRT_ALIGN:
				assert(lcl_free < lcl_size);
				jrec_align = &jnl_rec->jrec_align;
				align_filler_len = rlen - MIN_ALIGN_RECLEN;	/* Note: this filler section is not zeroed */
				if (nowrap)
				{
					memcpy(lcl_buff + lcl_free, (uchar_ptr_t)jrec_align, FIXED_ALIGN_RECLEN);
					lcl_free += (int4)(FIXED_ALIGN_RECLEN + align_filler_len);
				} else
				{
					JNL_PUTSTR(lcl_free, lcl_buff, (uchar_ptr_t)jrec_align, (int4)FIXED_ALIGN_RECLEN, lcl_size);
					/* Note: JNL_PUTSTR can wrap-adjust "lcl_free" while writing FIXED_ALIGN_RECLEN bytes.
					 * In that case, we should not wrap-adjust it (i.e. "- lcl_size").
					 * Hence the "if" check below.
					 */
					lcl_free += align_filler_len;
					if (lcl_size <= lcl_free)
					{
						lcl_free -= lcl_size;
						assert(lcl_free < lcl_size);
					}
				}
				/* Now copy suffix */
				assert(0 == (UINTPTR_T)(&lcl_buff[0] + lcl_free) % SIZEOF(jrec_suffix));
				suffix.backptr = rlen;
				suffix.suffix_code = JNL_REC_SUFFIX_CODE;
				*(jrec_suffix *)(lcl_buff + lcl_free) = suffix;
				lcl_free += SIZEOF(jrec_suffix);
				if (lcl_size == lcl_free)
					lcl_free = 0;
				INCR_GVSTATS_COUNTER(csa, cnl, n_jrec_other, 1);
				break;
			default:
				/* SET, KILL, ZKILL for TP, ZTP, non-TP */
				jfb = (jnl_format_buffer *)parm1;
				assert(NULL != jfb);
				assert(IS_TP(rectype) || IS_ZTP(rectype) || (0 == ((struct_jrec_upd *)jfb->buff)->update_num));
				assert((!IS_TP(rectype) && !IS_ZTP(rectype)) || (0 != ((struct_jrec_upd *)jfb->buff)->update_num));
				assert(((jrec_prefix *)jfb->buff)->forwptr == jfb->record_size);
				if (nowrap)
				{
					memcpy(lcl_buff + lcl_free, (uchar_ptr_t)jfb->buff, rlen);
					lcl_free += rlen;
					if (lcl_size == lcl_free)
						lcl_free = 0;
				} else
					JNL_PUTSTR(lcl_free, lcl_buff, (uchar_ptr_t)jfb->buff, rlen, lcl_size);
				INCR_GVSTATS_COUNTER(csa, cnl, n_jrec_logical, 1);
				break;
		}
	}
	assert((lcl_free - lcl_orig_free + lcl_size) % lcl_size == rlen);
	assert(lcl_buff[lcl_orig_free] == rectype);
	assert(lcl_orig_free < lcl_free  ||  lcl_free < jb->dsk);
	assert((lcl_freeaddr >= jb->dskaddr)
		|| (gtm_white_box_test_case_enabled && (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));
	jpc->new_freeaddr = new_freeaddr;
	INCR_GVSTATS_COUNTER(csa, cnl, n_jbuff_bytes, rlen);
	assert(lcl_free == jpc->new_freeaddr % lcl_size);
	if (!in_phase2)
	{
		assert(jnl_rec->prefix.time == jgbl.gbl_jrec_time); /* since latter is used in UPDATE_JBP_RSRV_FREEADDR */
		SET_JNLBUFF_PREV_JREC_TIME(jb, jnl_rec->prefix.time, DO_GBL_JREC_TIME_CHECK_TRUE);
			/* Keep jb->prev_jrec_time up to date */
		jpc->curr_tn = csa->ti->curr_tn;	/* needed below by UPDATE_JBP_RSRV_FREEADDR */
		UPDATE_JBP_RSRV_FREEADDR(csa, jpc, jb, NULL, rlen, commit_index, FALSE, 0, 0, FALSE); /* sets "commit_index" */
		assert(jb->phase2_commit_array[commit_index].curr_tn == jpc->curr_tn);
		JNL_PHASE2_WRITE_COMPLETE(csa, jb, commit_index, jpc->new_freeaddr);
	} else
	{
		jpc->phase2_freeaddr = jpc->new_freeaddr;
		jpc->phase2_free = lcl_free;
	}
	DEBUG_ONLY(jnl_write_recursion_depth--);
}
