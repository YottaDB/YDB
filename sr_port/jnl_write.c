/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
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
#	ifdef DEBUG
	uint4			lcl_dskaddr, mumps_node_sz;
	char			*mumps_node_ptr;
#	endif

	error_def(ERR_JNLWRTNOWWRTR);
	error_def(ERR_JNLWRTDEFER);

	reg = jpc->region;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	is_replicated = jrt_is_replicated[rectype];
	/* Ensure that no replicated journal record is written by this routine if REPL-WAS_ENABLED(csa) is TRUE */
	assert((JNL_ENABLED(csa) && !REPL_WAS_ENABLED(csa)) || !is_replicated);
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
	if (rlen_with_align != rlen)
	{	/* the calls below to jnl_write_attempt() and jnl_file_extend() are duplicated for the ALIGN record and the
		 * non-ALIGN journal record instead of making it a function. this is purely for performance reasons.
		 */
		assert((!jb->blocked) || (FALSE == is_proc_alive(jb->blocked, 0))
			VMS_ONLY(|| ((jb->blocked == process_id) && lib$ast_in_prog())));
		jb->blocked = process_id;
		/* We should differentiate between a full and an empty journal buffer, hence the pessimism reflected in the <=
		 * check below. Hence also the -1 in lcl_freeaddr - (lcl_size - align_rec_len - 1).
		 * This means that although we have space we might still be invoking jnl_write_attempt (very unlikely).
		 */
		if (JNL_SPACE_AVAILABLE(jb, lcl_dskaddr, lcl_freeaddr, lcl_size, jnl_wrt_start_mask) <= align_rec_len)
		{	/* The fancy ordering of operators/operands in the calculation done below is to avoid overflows. */
			if (SS_NORMAL != jnl_write_attempt(jpc,
					ROUND_UP2(lcl_freeaddr - (lcl_size - align_rec_len- 1), jnl_wrt_start_modulus)))
			{
				assert(NOJNL == jpc->channel); /* jnl file lost */
				return; /* let the caller handle the error */
			}
		}
		jb->blocked = 0;
		if (jb->filesize < DISK_BLOCKS_SUM(lcl_freeaddr, align_rec_len)) /* not enough room in jnl file, extend it. */
		{	/* We should never reach here if we are called from t_end/tp_tend */
			assert(!IS_GTM_IMAGE || csa->ti->early_tn == csa->ti->curr_tn);
			if (SS_NORMAL != jnl_flush(reg))
			{
				assert(NOJNL == jpc->channel); /* jnl file lost */
				return; /* let the caller handle the error */
			}
			assert(lcl_freeaddr == jb->dskaddr);
			if (EXIT_ERR == jnl_file_extend(jpc, align_rec_len))	/* if extension fails, not much we can do */
			{
				assert(FALSE);
				return;
			}
			if (0 == jpc->pini_addr && JRT_PINI != rectype)
			{	/* This can happen only if jnl got switched in jnl_file_extend above.
				 * We can't proceed now since the jnl record that we are writing now contains pini_addr	information
				 * 	pointing to the older journal which is inappropriate if written into the new journal.
				 */
				GTMASSERT;
			}
		}
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
		checksum = ADJUST_CHECKSUM(INIT_CHECKSUM_SEED, lcl_freeaddr);
		checksum = ADJUST_CHECKSUM(checksum, csd->jnl_checksum);
		assert(checksum);
		align_rec.prefix.checksum = checksum;
		suffix.suffix_code = JNL_REC_SUFFIX_CODE;
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
			jnl_rec->prefix.pini_addr = lcl_freeaddr;
	}
	checksum = jnl_rec->prefix.checksum;
	assert(checksum);
#	ifdef DEBUG
	/* Ensure that the checksum computed earlier in jnl_format or jnl_write_pblk matches with the block's content. For fixed
	 * size records and AIMG records, this check is not needed since the checksum is initialized to INIT_CHECKSUM_SEED.
	 */
	if (!jrt_fixed_size[rectype] && (JRT_AIMG != rectype))
	{
		if (JRT_PBLK == rectype)
			assert(checksum == jnl_get_checksum((uint4 *)blk_ptr, NULL, jnl_rec->jrec_pblk.bsiz));
		else
		{
			assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype));
			mumps_node_ptr = jfb->buff + FIXED_UPD_RECLEN;
			mumps_node_sz = jfb->record_size - (FIXED_UPD_RECLEN + JREC_SUFFIX_SIZE);
			assert(checksum == jnl_get_checksum((uint4 *)mumps_node_ptr, NULL, mumps_node_sz));
		}
	}
#	endif
	checksum = ADJUST_CHECKSUM(checksum, lcl_freeaddr);
	checksum = ADJUST_CHECKSUM(checksum, csd->jnl_checksum);
	ADJUST_CHECKSUM_WITH_SEQNO(is_replicated, checksum, GET_JNL_SEQNO(jnl_rec)); /* Note: checksum is updated inside macro */
	jnl_rec->prefix.checksum = checksum;
	UNIX_ONLY(assert((!jb->blocked) || (FALSE == is_proc_alive(jb->blocked, 0)));)
	VMS_ONLY(assert(!jb->blocked || (jb->blocked == process_id) && lib$ast_in_prog())); /* wcs_wipchk_ast can set jb->blocked */
	jb->blocked = process_id;
	/* We should differentiate between a full and an empty journal buffer, hence the pessimism reflected in the <= check below.
	 * Hence also the -1 in lcl_freeaddr - (lcl_size - rlen - 1).
	 * This means that although we have space we might still be invoking jnl_write_attempt (very unlikely).
	 */
	if (JNL_SPACE_AVAILABLE(jb, lcl_dskaddr, lcl_freeaddr, lcl_size, jnl_wrt_start_mask) <= rlen)
	{	/* The fancy ordering of operators/operands in the calculation done below is to avoid overflows. */
		if (SS_NORMAL != jnl_write_attempt(jpc, ROUND_UP2(lcl_freeaddr - (lcl_size - rlen - 1), jnl_wrt_start_modulus)))
		{
			assert(NOJNL == jpc->channel); /* jnl file lost */
			return; /* let the caller handle the error */
		}
	}
	jb->blocked = 0;
	if (jb->filesize < DISK_BLOCKS_SUM(lcl_freeaddr, rlen)) /* not enough room in jnl file, extend it. */
	{	/* We should never reach here if we are called from t_end/tp_tend */
		assert(!IS_GTM_IMAGE || csa->ti->early_tn == csa->ti->curr_tn);
		if (SS_NORMAL != jnl_flush(reg))
		{
			assert(NOJNL == jpc->channel); /* jnl file lost */
			return; /* let the caller handle the error */
		}
		assert(lcl_freeaddr == jb->dskaddr);
		if (EXIT_ERR == jnl_file_extend(jpc, rlen))	/* if extension fails, not much we can do */
		{
			assert(FALSE);
			return;
		}
		if (0 == jpc->pini_addr && JRT_PINI != rectype)
		{	/* This can happen only if jnl got switched in jnl_file_extend above.
			 * We can't proceed now since the jnl record that we are writing now contains pini_addr	information
			 * 	pointing to the older journal which is inappropriate if written into the new journal.
			 */
			GTMASSERT;
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
			jnl_fs_block_size = jb->fs_block_size;
			aligned_lcl_free = ROUND_UP2(lcl_free, jnl_fs_block_size);
			padding_size = aligned_lcl_free - lcl_free;
			assert(0 <= (int4)padding_size);
			if (padding_size)
				memset(lcl_buff + lcl_free, 0, padding_size);
		}
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
		}
	}
	assert((lcl_free - lcl_orig_free + lcl_size) % lcl_size == rlen);
	assert(lcl_buff[lcl_orig_free] == rectype);
	assert(lcl_orig_free < lcl_free  ||  lcl_free < jb->dsk);
	assert((lcl_freeaddr >= jb->dskaddr)
		|| (gtm_white_box_test_case_enabled && (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));
	jpc->new_freeaddr = lcl_freeaddr + rlen;
	assert(lcl_free == jpc->new_freeaddr % lcl_size);
	if (REPL_ENABLED(csa) && is_replicated)
	{	/* If the database is encrypted, then at this point jfb->buff will contain encrypted
		 * data which we don't want to to push into the jnlpool. Instead, we make use of the
		 * alternate alt_buff which is guaranteed to contain the original unencrypted data.
		 * */
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
			return;
		}
	)
	if (dba_mm == reg->dyn.addr->acc_meth)
		jnl_mm_timer(csa, reg);
}
