/****************************************************************
 *								*
 *	Copyright 2003, 2012 Fidelity Information Services, Inc	*
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

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

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
#include "jnl_get_checksum.h"
#include "memcoherency.h"
#include "is_proc_alive.h"
#include "wbox_test_init.h"
#include "gtmimagename.h"

GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF	uint4			process_id;
GBLREF	sm_uc_ptr_t		jnldata_base;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		in_jnl_file_autoswitch;

#ifdef DEBUG
STATICDEF	int		jnl_write_recursion_depth;

#define	MAX_JNL_WRITE_RECURSION_DEPTH	2

#endif

error_def(ERR_JNLWRTNOWWRTR);
error_def(ERR_JNLWRTDEFER);

#ifdef DEBUG
/* The fancy ordering of operators/operands in the JNL_SPACE_AVAILABLE calculation is to avoid overflows. */
#define	JNL_SPACE_AVAILABLE(jb, lcl_dskaddr, lcl_freeaddr, lcl_size, jnl_wrt_start_mask)	\
(												\
	assert(((jb)->dskaddr <= lcl_freeaddr)							\
		|| (gtm_white_box_test_case_enabled						\
			&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number))),	\
	/* the following assert is an || to take care of 4G value overflows or 0 underflows */	\
	assert((lcl_freeaddr <= lcl_size) || ((jb)->dskaddr >= lcl_freeaddr - lcl_size)		\
		|| (gtm_white_box_test_case_enabled						\
			&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number))),	\
	(lcl_size - (lcl_freeaddr - ((lcl_dskaddr = (jb)->dskaddr) & jnl_wrt_start_mask)))	\
)
#else
#define	JNL_SPACE_AVAILABLE(jb, dummy, lcl_freeaddr, lcl_size, jnl_wrt_start_mask)		\
	(lcl_size - (lcl_freeaddr - ((jb)->dskaddr & jnl_wrt_start_mask)))
#endif


#define	JNL_PUTSTR(lcl_free, lcl_buff, src, len, lcl_size)			\
{										\
	int	size_before_wrap;						\
										\
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

/* Note: DO_JNL_WRITE_ATTEMPT_IF_NEEDED and DO_JNL_FILE_EXTEND_IF_NEEDED are macros (instead of functions) for performance
 * reasons since they are invoked for the fast path (no-align-record) always and for the ALIGN record once in a while.
 */
#define	DO_JNL_WRITE_ATTEMPT_IF_NEEDED(JPC, JB, LCL_DSKADDR, LCL_FREEADDR, LCL_SIZE,					\
						JNL_WRT_START_MASK, REC_LEN, JNL_WRT_START_MODULUS)			\
{															\
	GBLREF	uint4	process_id;											\
															\
	assert((!JB->blocked) || (FALSE == is_proc_alive(JB->blocked, 0))						\
		VMS_ONLY(|| ((JB->blocked == process_id) && lib$ast_in_prog())));					\
	JB->blocked = process_id;											\
	/* We should differentiate between a full and an empty journal buffer, hence the pessimism reflected		\
	 * in the <= check below. Hence also the -1 in LCL_FREEADDR - (LCL_SIZE - REC_LEN - 1).* This means		\
	 * that although we have space we might still be invoking jnl_write_attempt (very unlikely).			\
	 */														\
	if (JNL_SPACE_AVAILABLE(JB, LCL_DSKADDR, LCL_FREEADDR, LCL_SIZE, JNL_WRT_START_MASK) <= REC_LEN)		\
	{	/* The fancy ordering of operators/operands in the calculation done below is to avoid overflows. */	\
		if (SS_NORMAL != jnl_write_attempt(JPC,									\
				ROUND_UP2(LCL_FREEADDR - (LCL_SIZE - REC_LEN- 1), JNL_WRT_START_MODULUS)))		\
		{													\
			assert(NOJNL == JPC->channel); /* jnl file lost */						\
			DEBUG_ONLY(jnl_write_recursion_depth--);							\
			return; /* let the caller handle the error */							\
		}													\
	}														\
	JB->blocked = 0;												\
}

#define	DO_JNL_FILE_EXTEND_IF_NEEDED(JREC_LEN, JB, LCL_FREEADDR, CSA, RECTYPE, BLK_PTR, JFB, REG, JPC, JNL_REC)			\
{																\
	int4			jrec_len_padded;										\
																\
	GBLREF	boolean_t	in_jnl_file_autoswitch;										\
																\
	/* Before writing a journal record, check if we have some padding space							\
	 * to close the journal file in case we are on the verge of an autoswitch.						\
	 * If we are about to autoswitch the journal file at this point, dont							\
	 * do the padding check since the padding space has already been checked						\
	 * in jnl_write calls before this autoswitch invocation. We can safely							\
	 * write the input record without worrying about autoswitch limit overflow.						\
	 */															\
	jrec_len_padded = JREC_LEN;												\
	if (!in_jnl_file_autoswitch)												\
		jrec_len_padded = JREC_LEN + JNL_FILE_TAIL_PRESERVE;								\
	if (JB->filesize < DISK_BLOCKS_SUM(LCL_FREEADDR, jrec_len_padded)) /* not enough room in jnl file, extend it */		\
	{	/* We should never reach here if we are called from t_end/tp_tend. We check that by using the fact that		\
		 * early_tn is different from curr_tn in the t_end/tp_tend case. The only exception is wcs_recover which	\
		 * also sets these to be different in case of writing an INCTN record. For this case though it is okay to	\
		 * extend/autoswitch the file. So allow that.									\
		 */														\
		assertpro((CSA->ti->early_tn == CSA->ti->curr_tn) || (JRT_INCTN == RECTYPE));					\
		assert(!IS_REPLICATED(RECTYPE)); /* all replicated jnl records should have gone through t_end/tp_tend */	\
		assert(jrt_fixed_size[RECTYPE]); /* this is used later in re-computing checksums */				\
		assert(NULL == BLK_PTR);	/* as otherwise it is a PBLK or AIMG record which is of variable record		\
						 * length that conflicts with the immediately above assert.			\
						 */										\
		assert(NULL == JFB);		/* as otherwise it is a logical record with formatted journal records which	\
						 * is of variable record length (conflicts with the jrt_fixed_size assert).	\
						 */										\
		assertpro(!in_jnl_file_autoswitch);	/* avoid recursion of jnl_file_extend */				\
		if (SS_NORMAL != jnl_flush(REG))										\
		{														\
			assert(NOJNL == JPC->channel); /* jnl file lost */							\
			DEBUG_ONLY(jnl_write_recursion_depth--);								\
			return; /* let the caller handle the error */								\
		}														\
		assert(LCL_FREEADDR == JB->dskaddr);										\
		if (EXIT_ERR == jnl_file_extend(JPC, JREC_LEN))	/* if extension fails, not much we can do */			\
		{														\
			DEBUG_ONLY(jnl_write_recursion_depth--);								\
			assert(FALSE);												\
			return;													\
		}														\
		if (0 == JPC->pini_addr)											\
		{	/* This can happen only if jnl got switched in jnl_file_extend above.					\
			 * Write a PINI record in the new journal file and then continue writing the input record.		\
			 * Basically we need to redo the processing in jnl_write because a lot of the local variables		\
			 * have changed state (e.g. JB->freeaddr etc.). So we instead call jnl_write()				\
			 * recursively and then return immediately.								\
			 */													\
			jnl_put_jrt_pini(CSA);											\
			assertpro(JPC->pini_addr);	/* should have been set in "jnl_put_jrt_pini" */			\
			if (JRT_PINI != RECTYPE)										\
			{													\
				JNL_REC->prefix.pini_addr = JPC->pini_addr;							\
				/* Checksum needs to be recomputed since prefix.pini_addr is changed in above statement */	\
				JNL_REC->prefix.checksum = INIT_CHECKSUM_SEED;							\
				JNL_REC->prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED,					\
									(uint4 *)JNL_REC, JNL_REC->prefix.forwptr);		\
				jnl_write(JPC, RECTYPE, JNL_REC, NULL, NULL);							\
			}													\
			DEBUG_ONLY(jnl_write_recursion_depth--);								\
			return;													\
		}														\
	}															\
}

/* jpc 	   : Journal private control
 * rectype : Record type
 * jnl_rec : This contains fixed part of a variable size record or the complete fixed size records.
 * blk_ptr : For JRT_PBLK and JRT_AIMG this has the block image
 * jfb     : For SET/KILL/ZKILL/ZTWORM records entire record is formatted in this.
 * 	     For JRT_PBLK and JRT_AIMG it contains partial records
 */
void	jnl_write(jnl_private_control *jpc, enum jnl_record_type rectype, jnl_record *jnl_rec, blk_hdr_ptr_t blk_ptr,
	jnl_format_buffer *jfb)
{
	int4			align_rec_len, rlen, rlen_with_align, dstlen, lcl_size, lcl_free, lcl_orig_free;
	jnl_buffer_ptr_t	jb;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	struct_jrec_align	align_rec;
	uint4 			status;
	jrec_suffix		suffix;
	boolean_t		nowrap, is_replicated;
	struct_jrec_blk		*jrec_blk;
	uint4			checksum, jnlpool_size, lcl_freeaddr;
	sm_uc_ptr_t		lcl_buff;
	gd_region		*reg;
	char			*ptr;
	int			jnl_wrt_start_modulus, jnl_wrt_start_mask;
	uint4			jnl_fs_block_size, aligned_lcl_free, padding_size;
	uint4			tmp_csum1, tmp_csum2;
#	ifdef DEBUG
	uint4			lcl_dskaddr, mumps_node_sz;
	char			*mumps_node_ptr;
#	endif

	assert(jnl_write_recursion_depth++ < MAX_JNL_WRITE_RECURSION_DEPTH);
	reg = jpc->region;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	is_replicated = jrt_is_replicated[rectype];
	/* Ensure that no replicated journal record is written by this routine if REPL-WAS_ENABLED(csa) is TRUE */
	assert((JNL_ENABLED(csa) && !REPL_WAS_ENABLED(csa)) || !is_replicated);
	/* Assert that the only journal records that the source server ever writes are PINI/PFIN/EPOCH/EOF
	 * which it does at the very end when the database is about to be shut down
	 */
	assert(!is_src_server || (JRT_EOF == rectype) || (JRT_PINI == rectype) || (JRT_EPOCH == rectype) || (JRT_PFIN == rectype));
	assert(csa->now_crit  ||  (csd->clustered  &&  csa->nl->ccp_state == CCST_CLOSED));
	assert(rectype > JRT_BAD  &&  rectype < JRT_RECTYPES && JRT_ALIGN != rectype);
	jb = jpc->jnl_buff;
	/* Before taking a copy of jb->freeaddr, determine if both free and freeaddr are in sync. If not fix that first. */
	if (jb->free_update_pid)
	{
		FIX_NONZERO_FREE_UPDATE_PID(csa, jb);
	}
	lcl_freeaddr = jb->freeaddr;
	lcl_free = jb->free;
	lcl_size = jb->size;
	lcl_buff = &jb->buff[jb->buff_off];
	DBG_CHECK_JNL_BUFF_FREEADDR(jb);
	++jb->reccnt[rectype];
	assert(NULL != jnl_rec);
	rlen = jnl_rec->prefix.forwptr;
	/* Do high-level check on rlen */
	assert(rlen <= jb->max_jrec_len);
	/* Do fine-grained checks on rlen */
	GTMTRIG_ONLY(assert(!IS_ZTWORM(rectype) || (MAX_ZTWORM_JREC_LEN >= rlen));)	/* ZTWORMHOLE */
	assert(!IS_SET_KILL_ZKILL_ZTRIG(rectype) || (JNL_MAX_SET_KILL_RECLEN(csd) >= rlen));	/* SET, KILL, ZKILL */
	assert((NULL == blk_ptr) || (JNL_MAX_PBLK_RECLEN(csd) >= rlen));		/* PBLK and AIMG */
	jb->bytcnt += rlen;
	assert (0 == rlen % JNL_REC_START_BNDRY);
	rlen_with_align = rlen + (int4)MIN_ALIGN_RECLEN;
	assert(0 == rlen_with_align % JNL_REC_START_BNDRY);
	assert((uint4)rlen_with_align < ((uint4)1 << jb->log2_of_alignsize));
	if ((lcl_freeaddr >> jb->log2_of_alignsize) == ((lcl_freeaddr + rlen_with_align - 1) >> jb->log2_of_alignsize))
		rlen_with_align = rlen;
	else
	{
		align_rec.align_str.length = ROUND_UP2(lcl_freeaddr, ((uint4)1 << jb->log2_of_alignsize))
			- lcl_freeaddr - (uint4)MIN_ALIGN_RECLEN;
		align_rec_len = (int4)(MIN_ALIGN_RECLEN + align_rec.align_str.length);
		assert (0 == align_rec_len % JNL_REC_START_BNDRY);
		rlen_with_align = rlen + align_rec_len;
	}
	jnl_wrt_start_mask = JNL_WRT_START_MASK(jb);
	jnl_wrt_start_modulus = JNL_WRT_START_MODULUS(jb);
	cnl = csa->nl;
	/* If we are currently extending the journal file and writing the closing part of journal records,
	 * it better be the records that we expect. This is because we will skip the padding check for these
	 * records. The macro JNL_FILE_TAIL_PRESERVE already takes into account padding space for these.
	 */
	assert(!in_jnl_file_autoswitch
		|| (JRT_PINI == rectype) || (JRT_PFIN == rectype) || (JRT_EPOCH == rectype)
		|| (JRT_INCTN == rectype) || (JRT_EOF == rectype));
	if (rlen_with_align != rlen)
	{
		DO_JNL_WRITE_ATTEMPT_IF_NEEDED(jpc, jb, lcl_dskaddr, lcl_freeaddr, lcl_size,
						jnl_wrt_start_mask, align_rec_len, jnl_wrt_start_modulus);
		DO_JNL_FILE_EXTEND_IF_NEEDED(align_rec_len, jb, lcl_freeaddr, csa, rectype, blk_ptr, jfb, reg, jpc, jnl_rec);
		align_rec.prefix.jrec_type = JRT_ALIGN;
		assert(align_rec_len <= jb->max_jrec_len);
		align_rec.prefix.forwptr = suffix.backptr = align_rec_len;
		align_rec.prefix.time = jnl_rec->prefix.time;
		align_rec.prefix.tn = jnl_rec->prefix.tn;
		/* we have to write an ALIGN record here before writing the PINI record but we do not have a non-zero
		 * pini_addr for the ALIGN since we have not yet written the PINI. we use the pini_addr field of the
		 * first PINI journal record in the journal file which is nothing but JNL_FILE_FIRST_RECORD.
		 */
		align_rec.prefix.pini_addr = (JRT_PINI == rectype) ? JNL_FILE_FIRST_RECORD : jnl_rec->prefix.pini_addr;
		align_rec.prefix.checksum = INIT_CHECKSUM_SEED;
		suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		align_rec.prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)&align_rec, SIZEOF(jrec_prefix));
		ADJUST_CHECKSUM(align_rec.prefix.checksum, lcl_freeaddr, align_rec.prefix.checksum);
		ADJUST_CHECKSUM(align_rec.prefix.checksum, csd->jnl_checksum, align_rec.prefix.checksum);
		assert(lcl_free >= 0 && lcl_free < lcl_size);
		if (lcl_size >= (lcl_free + align_rec_len))
		{	/* before the string for zeroes */
			memcpy(lcl_buff + lcl_free, (uchar_ptr_t)&align_rec, FIXED_ALIGN_RECLEN);
			lcl_free += (int4)(FIXED_ALIGN_RECLEN + align_rec.align_str.length); /* zeroing is not necessary */
		} else
		{
			JNL_PUTSTR(lcl_free, lcl_buff, (uchar_ptr_t)&align_rec, (int4)FIXED_ALIGN_RECLEN, lcl_size);
			if (lcl_size >= (lcl_free + align_rec.align_str.length + SIZEOF(jrec_suffix)))
				lcl_free += align_rec.align_str.length;	/* zeroing is not necessary */
			else
			{
				if (lcl_size >= (lcl_free + align_rec.align_str.length))
				{
					lcl_free += align_rec.align_str.length;	/* zeroing is not necessary */
					if (lcl_size == lcl_free)
						lcl_free = 0;
				} else
					lcl_free = lcl_free + align_rec.align_str.length - lcl_size;
			}
		}
		/* Now copy suffix */
		assert(0 == (UINTPTR_T)(&lcl_buff[0] + lcl_free) % SIZEOF(jrec_suffix));
		*(jrec_suffix *)(lcl_buff + lcl_free) = *(jrec_suffix *)&suffix;
		lcl_free += SIZEOF(jrec_suffix);
		if (lcl_size == lcl_free)
			lcl_free = 0;
		jpc->new_freeaddr = lcl_freeaddr + align_rec_len;
		INCR_GVSTATS_COUNTER(csa, cnl, n_jrec_other, 1);
		INCR_GVSTATS_COUNTER(csa, cnl, n_jbuff_bytes, align_rec_len);
		assert(jgbl.gbl_jrec_time >= align_rec.prefix.time);
		assert(align_rec.prefix.time >= jb->prev_jrec_time);
		jb->prev_jrec_time = align_rec.prefix.time;
		jpc->temp_free = lcl_free; /* set jpc->temp_free BEFORE setting free_update_pid (secshr_db_clnup relies on this) */
		assert(lcl_free == jpc->new_freeaddr % lcl_size);
		/* Note that freeaddr should be updated ahead of free since jnl_output_sp.c does computation of wrtsize
		 * based on free and asserts follow later there which use freeaddr.
		 */
		jb->free_update_pid = process_id;
		lcl_freeaddr = jpc->new_freeaddr;
		jb->freeaddr = lcl_freeaddr;
		/* Write memory barrier here to enforce the fact that freeaddr *must* be seen to be updated before
		   free is updated. It is less important if free is stale so we do not require a 2nd barrier for that
		   and will let the lock release (crit lock required since clustering not currently supported) do the
		   2nd memory barrier for us. This barrier takes care of this process's responsibility to broadcast
		   cache changes. It is up to readers to also specify a read memory barrier if necessary to receive
		   this broadcast.
		*/
		SHM_WRITE_MEMORY_BARRIER;
		jb->free = lcl_free;
		jb->free_update_pid = 0;
		DBG_CHECK_JNL_BUFF_FREEADDR(jb);
		if (JRT_PINI == rectype)
		{
			jnl_rec->prefix.pini_addr = lcl_freeaddr;
			/* Checksum needs to be recomputed since prefix.pini_addr is changed in above statement */
			jnl_rec->prefix.checksum = INIT_CHECKSUM_SEED;
			jnl_rec->prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED,
								(uint4 *)&jnl_rec->jrec_pini, SIZEOF(struct_jrec_pini));
		}
	}
	checksum = jnl_rec->prefix.checksum;
	assert(checksum);
#	ifdef DEBUG
	/* Ensure that the checksum computed earlier in jnl_format or jnl_write_pblk or jnl_write_aimg_rec or fixed-sized records
	 * matches with the block's content.
	 */
	if ((JRT_PBLK == rectype) || (JRT_AIMG == rectype))
	{
		COMPUTE_COMMON_CHECKSUM(tmp_csum2, jnl_rec->prefix);
		tmp_csum1 = jnl_get_checksum((uint4 *)blk_ptr, NULL, jnl_rec->jrec_pblk.bsiz);
		COMPUTE_PBLK_CHECKSUM(tmp_csum1, &jnl_rec->jrec_pblk, tmp_csum2, tmp_csum1);
		assert(checksum == tmp_csum1);
	} else if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))
	{
		COMPUTE_COMMON_CHECKSUM(tmp_csum2, jnl_rec->prefix);
		mumps_node_ptr = jfb->buff + FIXED_UPD_RECLEN;
		mumps_node_sz = jfb->record_size - (FIXED_UPD_RECLEN + JREC_SUFFIX_SIZE);
		tmp_csum1 = jnl_get_checksum((uint4 *)mumps_node_ptr, NULL, mumps_node_sz);
		COMPUTE_LOGICAL_REC_CHECKSUM(tmp_csum1, &jnl_rec->jrec_set_kill, tmp_csum2, tmp_csum1);
		assert(checksum == tmp_csum1);
	}else if (jrt_fixed_size[rectype] || JRT_ALIGN == rectype)
	{
		jnl_rec->prefix.checksum = INIT_CHECKSUM_SEED;
		switch(rectype)
		{
		case JRT_ALIGN:
			tmp_csum1 = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)&jnl_rec->jrec_align, SIZEOF(jrec_prefix));
			break;
		default:
			if(JRT_TRIPLE != rectype && JRT_HISTREC != rectype)
				tmp_csum1 = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)&jnl_rec->jrec_set_kill,
						jnl_rec->prefix.forwptr);
			break;
		}
		assert(checksum == tmp_csum1);
		jnl_rec->prefix.checksum = checksum;
	}
#	endif
	ADJUST_CHECKSUM(checksum, lcl_freeaddr, checksum);
	ADJUST_CHECKSUM(checksum, csd->jnl_checksum, checksum);
	jnl_rec->prefix.checksum = checksum;
	DO_JNL_WRITE_ATTEMPT_IF_NEEDED(jpc, jb, lcl_dskaddr, lcl_freeaddr, lcl_size,
					jnl_wrt_start_mask, rlen, jnl_wrt_start_modulus);
	DO_JNL_FILE_EXTEND_IF_NEEDED(rlen, jb, lcl_freeaddr, csa, rectype, blk_ptr, jfb, reg, jpc, jnl_rec);
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
			jnl_fs_block_size = jb->fs_block_size;
			aligned_lcl_free = ROUND_UP2(lcl_free, jnl_fs_block_size);
			padding_size = aligned_lcl_free - lcl_free;
			assert(0 <= (int4)padding_size);
			if (padding_size)
				memset(lcl_buff + lcl_free, 0, padding_size);
		}
		/* Note: Cannot easily use ? : syntax below as INCR_GVSTATS_COUNTER macro
		 * is not an arithmetic expression but a sequence of statements.
		 */
		if (JRT_EPOCH != rectype)
			INCR_GVSTATS_COUNTER(csa, cnl, n_jrec_other, 1);
		/* else for EPOCH, the increment of JRE or JRI is done after "jnl_write_epoch_rec" in caller */
	} else
	{
		if (NULL != blk_ptr)	/* PBLK and AIMG */
		{
			assert(FIXED_BLK_RECLEN == FIXED_PBLK_RECLEN);
			assert(FIXED_BLK_RECLEN == FIXED_AIMG_RECLEN);
			jrec_blk = (struct_jrec_blk *)jnl_rec;
			if (nowrap)
			{	/* write fixed part of record before the actual gds block image */
				memcpy(lcl_buff + lcl_free, (uchar_ptr_t)jnl_rec, FIXED_BLK_RECLEN);
				lcl_free += (int4)FIXED_BLK_RECLEN;
				/* write actual block */
				memcpy(lcl_buff + lcl_free, (uchar_ptr_t)blk_ptr, jrec_blk->bsiz);
				lcl_free += jrec_blk->bsiz;
				/* Now write trailing characters for 8-bye alignment and then suffix */
				memcpy(lcl_buff + lcl_free, (uchar_ptr_t)jfb->buff, jfb->record_size);
				lcl_free += jfb->record_size;
   				assert(lcl_free <= lcl_size);
				if (lcl_size == lcl_free)
					lcl_free = 0;
			} else
			{	/* write fixed part of record before the actual gds block image */
				JNL_PUTSTR(lcl_free, lcl_buff, (uchar_ptr_t)jnl_rec, (int4)FIXED_BLK_RECLEN, lcl_size);
				/* write actual block */
				JNL_PUTSTR(lcl_free, lcl_buff, (uchar_ptr_t)blk_ptr, jrec_blk->bsiz, lcl_size);
				/* Now write trailing characters for 8-bye alignment and then suffix */
				JNL_PUTSTR(lcl_free, lcl_buff, (uchar_ptr_t)jfb->buff, jfb->record_size, lcl_size);
			}
			INCR_GVSTATS_COUNTER(csa, cnl, n_jrec_pblk, 1);
		} else
		{	/* SET, KILL, ZKILL for TP, ZTP, non-TP */
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
		}
	}
	assert((lcl_free - lcl_orig_free + lcl_size) % lcl_size == rlen);
	assert(lcl_buff[lcl_orig_free] == rectype);
	assert(lcl_orig_free < lcl_free  ||  lcl_free < jb->dsk);
	assert((lcl_freeaddr >= jb->dskaddr)
		|| (gtm_white_box_test_case_enabled && (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));
	jpc->new_freeaddr = lcl_freeaddr + rlen;
	INCR_GVSTATS_COUNTER(csa, cnl, n_jbuff_bytes, rlen);
	assert(lcl_free == jpc->new_freeaddr % lcl_size);
	if (REPL_ENABLED(csa) && is_replicated)
	{	/* If the database is encrypted, then at this point jfb->buff will contain encrypted
		 * data which we don't want to to push into the jnlpool. Instead, we make use of the
		 * alternate alt_buff which is guaranteed to contain the original unencrypted data.
		 */
		if (jrt_fixed_size[rectype])
			ptr = (char *)jnl_rec;
		else
		{
#			ifdef GTM_CRYPT
			if (csd->is_encrypted && IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))
			 	ptr = jfb->alt_buff;
			else
#			endif
				ptr = jfb->buff;
		}
		assert(NULL != jnlpool.jnlpool_ctl && NULL != jnlpool_ctl); /* ensure we haven't yet detached from the jnlpool */
		assert((&FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs)->now_crit);	/* ensure we have the jnl pool lock */
		DEBUG_ONLY(jgbl.cu_jnl_index++;)
		jnlpool_size = temp_jnlpool_ctl->jnlpool_size;
		dstlen = jnlpool_size - temp_jnlpool_ctl->write;
		if (rlen <= dstlen)	/* dstlen >= rlen  (most frequent case) */
			memcpy(jnldata_base + temp_jnlpool_ctl->write, ptr, rlen);
		else			/* dstlen < rlen */
		{
			memcpy(jnldata_base + temp_jnlpool_ctl->write, ptr, dstlen);
			memcpy(jnldata_base, ptr + dstlen, rlen - dstlen);
		}
		temp_jnlpool_ctl->write += rlen;
		if (temp_jnlpool_ctl->write >= jnlpool_size)
			temp_jnlpool_ctl->write -= jnlpool_size;
	}
	assert(jgbl.gbl_jrec_time >= jnl_rec->prefix.time);
	assert(jnl_rec->prefix.time >= jb->prev_jrec_time);
	jb->prev_jrec_time = jnl_rec->prefix.time;
	jpc->temp_free = lcl_free; /* set jpc->temp_free BEFORE setting free_update_pid (secshr_db_clnup relies on this) */
	/* Note that freeaddr should be updated ahead of free since jnl_output_sp.c does computation of wrtsize
	 * based on free and asserts follow later there which use freeaddr.
	 */
	jb->free_update_pid = process_id;
	lcl_freeaddr = jpc->new_freeaddr;
	jb->freeaddr = lcl_freeaddr;
	/* Write memory barrier here to enforce the fact that freeaddr *must* be seen to be updated before
	   free is updated. It is less important if free is stale so we do not require a 2nd barrier for that
	   and will let the lock release (crit lock required since clustering not currently supported) do the
	   2nd memory barrier for us. This barrier takes care of this process's responsibility to broadcast
	   cache changes. It is up to readers to also specify a read memory barrier if necessary to receive
	   this broadcast.
	*/
	SHM_WRITE_MEMORY_BARRIER;
	jb->free = lcl_free;
	jb->free_update_pid = 0;
	DBG_CHECK_JNL_BUFF_FREEADDR(jb);
	VMS_ONLY(
		if (((lcl_freeaddr - jb->dskaddr) > jb->min_write_size)
		    && (SS_NORMAL != (status = jnl_qio_start(jpc))) && (ERR_JNLWRTNOWWRTR != status) && (ERR_JNLWRTDEFER != status))
	        {
			jb->blocked = 0;
			jnl_file_lost(jpc, status);
			DEBUG_ONLY(jnl_write_recursion_depth--);
			return;
		}
	)
	DEBUG_ONLY(jnl_write_recursion_depth--);
}
