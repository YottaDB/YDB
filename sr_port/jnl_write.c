/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
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

GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
DEBUG_ONLY( GBLREF bool		run_time;)

GBLREF	uint4			process_id;
GBLREF	sm_uc_ptr_t		jnldata_base;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF jnl_gbls_t		jgbl;
LITREF	int			jrt_fixed_size[];
LITREF	int			jrt_is_replicated[];

#ifdef DEBUG
/* The fancy ordering of operators/operands in the JNL_SPACE_AVAILABLE calculation is to avoid overflows. */
#define	JNL_SPACE_AVAILABLE(jb, lcl_dskaddr)							\
(												\
	assert((jb)->dskaddr <= (jb)->freeaddr),						\
	/* the following assert is an || to take care of 4G value overflows or 0 underflows */  \
	assert(((jb)->freeaddr <= (jb)->size) || (jb)->dskaddr >= (jb)->freeaddr - (jb)->size),	\
	((jb)->size - ((jb)->freeaddr - ((lcl_dskaddr = (jb)->dskaddr) & JNL_WRT_START_MASK)))	\
)
#else
#define	JNL_SPACE_AVAILABLE(jb, dummy)	 ((jb)->size - ((jb)->freeaddr - ((jb)->dskaddr & JNL_WRT_START_MASK)))
#endif


#define	JNL_PUTSTR(jpc, jb, src, len)						\
{										\
	int	size_before_wrap;						\
										\
	size_before_wrap = jb->size - jpc->temp_free;				\
	if (len <= size_before_wrap)						\
	{									\
		memcpy(&jb->buff[jpc->temp_free], src, len);			\
		jpc->temp_free += len;						\
		if (len == size_before_wrap)					\
			jpc->temp_free = 0;					\
	} else									\
	{									\
		memcpy(&jb->buff[jpc->temp_free], src, size_before_wrap);	\
		jpc->temp_free = len - size_before_wrap;			\
		memcpy(&jb->buff[0], src + size_before_wrap, jpc->temp_free);	\
	}									\
}

/* jpc 	   : Journal private control
 * rectype : Record type
 * jnl_rec : This contains fixed part of a variable size record or the complete fixed size records.
 * blk_ptr : For JRT_PBLK and JRT_AIMG this has the block image
 * jfb     : For SET/KILL/ZKILL records entire record is formatted in this.
 * 	     For JRT_PBLK and JRT_AIMG it contains partial records
 */
void	jnl_write(jnl_private_control *jpc, enum jnl_record_type rectype, jnl_record *jnl_rec, blk_hdr_ptr_t blk_ptr,
	jnl_format_buffer *jfb)
{
	int4			align_rec_len, rlen, rlen_with_align, srclen, dstlen;
	jnl_buffer_ptr_t	jb;
	sgmnt_addrs		*csa;
	struct_jrec_align	align_rec;
	uint4 			status;
	jrec_suffix		suffix;
	boolean_t		nowrap;
	struct_jrec_blk		*jrec_blk;
	uint4			checksum;
	DEBUG_ONLY(uint4	lcl_dskaddr;)

	error_def(ERR_JNLWRTNOWWRTR);
	error_def(ERR_JNLWRTDEFER);

	csa = &FILE_INFO(jpc->region)->s_addrs;
	assert(csa->now_crit  ||  (csa->hdr->clustered  &&  csa->nl->ccp_state == CCST_CLOSED));
	assert(rectype > JRT_BAD  &&  rectype < JRT_RECTYPES && JRT_ALIGN != rectype);
	jb = jpc->jnl_buff;
	assert(jb->freeaddr >= jb->dskaddr);
	++jb->reccnt[rectype];
	assert(NULL != jnl_rec);
	rlen = jnl_rec->prefix.forwptr;
	jb->bytcnt += rlen;
	assert (0 == rlen % JNL_REC_START_BNDRY);
	rlen_with_align = rlen + MIN_ALIGN_RECLEN;
	assert(0 == rlen_with_align % JNL_REC_START_BNDRY);
	assert((uint4)rlen_with_align < ((uint4)1 << jb->log2_of_alignsize));
	if ((jb->freeaddr >> jb->log2_of_alignsize) == ((jb->freeaddr + rlen_with_align - 1) >> jb->log2_of_alignsize))
		rlen_with_align = rlen;
	else
	{
		align_rec.align_str.length = ROUND_UP2(jb->freeaddr, ((uint4)1 << jb->log2_of_alignsize))
						- jb->freeaddr - MIN_ALIGN_RECLEN;
		align_rec_len = MIN_ALIGN_RECLEN + align_rec.align_str.length;
		assert (0 == align_rec_len % JNL_REC_START_BNDRY);
		rlen_with_align = rlen + align_rec_len;
	}
	if (rlen_with_align != rlen)
	{	/* the calls below to jnl_write_attempt() and jnl_file_extend() are duplicated for the ALIGN record and the
		 * non-ALIGN journal record instead of making it a function. this is purely for performance reasons.
		 */
		UNIX_ONLY(assert(!jb->blocked));
		VMS_ONLY(assert(!jb->blocked || jb->blocked == process_id && lib$ast_in_prog()));
							/* wcs_wipchk_ast can set jb->blocked */
		jb->blocked = process_id;
		/* We should differentiate between a full and an empty journal buffer, hence the pessimism reflected in the <=
		 * check below. Hence also the -1 in jb->freeaddr - (jb->size - align_rec_len - 1).
		 * This means that although we have space we might still be invoking jnl_write_attempt (very unlikely).
		 */
		if (JNL_SPACE_AVAILABLE(jb, lcl_dskaddr) <= align_rec_len)
		{	/* The fancy ordering of operators/operands in the calculation done below is to avoid overflows. */
			if (SS_NORMAL != jnl_write_attempt(jpc,
						ROUND_UP2(jb->freeaddr - (jb->size - align_rec_len- 1), JNL_WRT_START_MODULUS)))
			{
				assert(NOJNL == jpc->channel); /* jnl file lost */
				return; /* let the caller handle the error */
			}
		}
		jb->blocked = 0;
		if (jb->filesize < DISK_BLOCKS_SUM(jb->freeaddr, align_rec_len)) /* not enough room in jnl file, extend it. */
		{	/* We should never reach here if we are called from t_end/tp_tend */
			assert(!run_time || csa->ti->early_tn == csa->ti->curr_tn);
			jnl_flush(jpc->region);
			assert(jb->freeaddr == jb->dskaddr);
			if (-1 == jnl_file_extend(jpc, align_rec_len))	/* if extension fails, not much we can do */
			{
				assert(FALSE);
				return;
			}
			if (0 == csa->jnl->pini_addr && JRT_PINI != rectype)
			{	/* This can happen only if jnl got switched in jnl_file_extend above.
				 * We can't proceed now since the jnl record that we are writing now contains pini_addr	information
				 * 	pointing to the older journal which is inappropriate if written into the new journal.
				 */
				GTMASSERT;
			}
		}
		jpc->temp_free = jb->free;
		align_rec.prefix.jrec_type = JRT_ALIGN;
		align_rec.prefix.forwptr = suffix.backptr = align_rec_len;
		align_rec.prefix.time = jnl_rec->prefix.time;
		align_rec.prefix.tn = jnl_rec->prefix.tn;
		/* we have to write an ALIGN record here before writing the PINI record but we do not have a non-zero
		 * pini_addr for the ALIGN since we have not yet written the PINI. we use the pini_addr field of the
		 * first PINI journal record in the journal file which is nothing but JNL_FILE_FIRST_RECORD.
		 */
		align_rec.prefix.pini_addr = (JRT_PINI == rectype) ? JNL_FILE_FIRST_RECORD : jnl_rec->prefix.pini_addr;
		checksum = ADJUST_CHECKSUM(INIT_CHECKSUM_SEED, jb->freeaddr);
		checksum = ADJUST_CHECKSUM(checksum, csa->hdr->jnl_checksum);
		assert(checksum);
		align_rec.prefix.checksum = checksum;
		suffix.suffix_code = JNL_REC_SUFFIX_CODE;
		assert(jpc->temp_free >= 0 && jpc->temp_free < jpc->jnl_buff->size);
		if (jpc->jnl_buff->size >= (jpc->temp_free + align_rec_len))
		{	/* before the string for zeroes */
			memcpy(jpc->jnl_buff->buff + jpc->temp_free, (uchar_ptr_t)&align_rec, FIXED_ALIGN_RECLEN);
			jpc->temp_free += (FIXED_ALIGN_RECLEN + align_rec.align_str.length); /* zeroing is not necessary */
		} else
		{
			JNL_PUTSTR(jpc, jb, (uchar_ptr_t)&align_rec, FIXED_ALIGN_RECLEN);
			if (jpc->jnl_buff->size >= (jpc->temp_free + align_rec.align_str.length + sizeof(jrec_suffix)))
				jpc->temp_free += align_rec.align_str.length;	/* zeroing is not necessary */
			else
			{
				if (jpc->jnl_buff->size >= (jpc->temp_free + align_rec.align_str.length))
				{
					jpc->temp_free += align_rec.align_str.length;	/* zeroing is not necessary */
					if (jpc->jnl_buff->size == jpc->temp_free)
						jpc->temp_free = 0;
				} else
					jpc->temp_free = jpc->temp_free + align_rec.align_str.length - jpc->jnl_buff->size;
			}
		}
		/* Now copy suffix */
		assert(0 == (uint4)(&jpc->jnl_buff->buff[0] + jpc->temp_free) % sizeof(jrec_suffix));
		*(jrec_suffix *)(jpc->jnl_buff->buff + jpc->temp_free) = *(jrec_suffix *)&suffix;
		jpc->temp_free += sizeof(jrec_suffix);
		if (jpc->jnl_buff->size == jpc->temp_free)
			jpc->temp_free = 0;
		jpc->new_freeaddr = jb->freeaddr + align_rec_len;
		assert(jpc->temp_free == jpc->new_freeaddr % jb->size);
		/* Note that freeaddr should be updated ahead of free since jnl_output_sp.c does computation of wrtsize
		 * based on free and asserts follow later there which use freeaddr.
		 */
		jpc->free_update_inprog = TRUE; /* The following assignments must be protected against termination */
		jb->freeaddr = jpc->new_freeaddr;
		jb->free = jpc->temp_free;
		jpc->free_update_inprog = FALSE;
		if (JRT_PINI == rectype)
			jnl_rec->prefix.pini_addr = jb->freeaddr;
	}
	checksum = jnl_rec->prefix.checksum;
	assert(checksum);
	checksum = ADJUST_CHECKSUM(checksum, jb->freeaddr);
	checksum = ADJUST_CHECKSUM(checksum, csa->hdr->jnl_checksum);
	jnl_rec->prefix.checksum = checksum;
	UNIX_ONLY(assert(!jb->blocked));
	VMS_ONLY(assert(!jb->blocked || jb->blocked == process_id && lib$ast_in_prog())); /* wcs_wipchk_ast can set jb->blocked */
	jb->blocked = process_id;
	/* We should differentiate between a full and an empty journal buffer, hence the pessimism reflected in the <= check below.
	 * Hence also the -1 in jb->freeaddr - (jb->size - rlen - 1).
	 * This means that although we have space we might still be invoking jnl_write_attempt (very unlikely).
	 */
	if (JNL_SPACE_AVAILABLE(jb, lcl_dskaddr) <= rlen)
	{	/* The fancy ordering of operators/operands in the calculation done below is to avoid overflows. */
		if (SS_NORMAL != jnl_write_attempt(jpc,
					ROUND_UP2(jb->freeaddr - (jb->size - rlen - 1), JNL_WRT_START_MODULUS)))
		{
			assert(NOJNL == jpc->channel); /* jnl file lost */
			return; /* let the caller handle the error */
		}
	}
	jb->blocked = 0;
	if (jb->filesize < DISK_BLOCKS_SUM(jb->freeaddr, rlen)) /* not enough room in jnl file, extend it. */
	{	/* We should never reach here if we are called from t_end/tp_tend */
		assert(!run_time || csa->ti->early_tn == csa->ti->curr_tn);
		jnl_flush(jpc->region);
		assert(jb->freeaddr == jb->dskaddr);
		if (-1 == jnl_file_extend(jpc, rlen))	/* if extension fails, not much we can do */
		{
			assert(FALSE);
			return;
		}
		if (0 == csa->jnl->pini_addr && JRT_PINI != rectype)
		{	/* This can happen only if jnl got switched in jnl_file_extend above.
			 * We can't proceed now since the jnl record that we are writing now contains pini_addr	information
			 * 	pointing to the older journal which is inappropriate if written into the new journal.
			 */
			GTMASSERT;
		}
	}
	jpc->temp_free = jb->free;
	nowrap = (jpc->jnl_buff->size >= (jpc->temp_free + rlen));
	if (jrt_fixed_size[rectype])
	{
		if (nowrap)
		{
			memcpy(jpc->jnl_buff->buff + jpc->temp_free, (uchar_ptr_t)jnl_rec, rlen);
			jpc->temp_free += rlen;
			if (jpc->jnl_buff->size == jpc->temp_free)
				jpc->temp_free = 0;
		} else
			JNL_PUTSTR(jpc, jb, (uchar_ptr_t)jnl_rec, rlen);
	} else
	{
		if (NULL != blk_ptr)	/* PBLK and AIMG */
		{
			assert(FIXED_BLK_RECLEN == FIXED_PBLK_RECLEN);
			assert(FIXED_BLK_RECLEN == FIXED_AIMG_RECLEN);
			jrec_blk = (struct_jrec_blk *)jnl_rec;
			if (nowrap)
			{	/* write fixed part of record before the actual gds block image */
				memcpy(jpc->jnl_buff->buff + jpc->temp_free, (uchar_ptr_t)jnl_rec, FIXED_BLK_RECLEN);
				jpc->temp_free += FIXED_BLK_RECLEN;
				/* write actual block */
				memcpy(jpc->jnl_buff->buff + jpc->temp_free, (uchar_ptr_t)blk_ptr, jrec_blk->bsiz);
				jpc->temp_free += jrec_blk->bsiz;
				/* Now write trailing characters for 8-bye alignment and then suffix */
				memcpy(jpc->jnl_buff->buff + jpc->temp_free, (uchar_ptr_t)jfb->buff, jfb->record_size);
				jpc->temp_free += jfb->record_size;
   				assert(jpc->temp_free <= jb->size);
				if (jpc->jnl_buff->size == jpc->temp_free)
					jpc->temp_free = 0;
			} else
			{	/* write fixed part of record before the actual gds block image */
				JNL_PUTSTR(jpc, jb, (uchar_ptr_t)jnl_rec, FIXED_BLK_RECLEN);
				/* write actual block */
				JNL_PUTSTR(jpc, jb, (uchar_ptr_t)blk_ptr, jrec_blk->bsiz);
				/* Now write trailing characters for 8-bye alignment and then suffix */
				JNL_PUTSTR(jpc, jb, (uchar_ptr_t)jfb->buff, jfb->record_size);
			}
		} else
		{	/* SET, KILL, ZKILL for TP, ZTP, non-TP */
			if (nowrap)
			{
				memcpy(jpc->jnl_buff->buff + jpc->temp_free, (uchar_ptr_t)jfb->buff, rlen);
				jpc->temp_free += rlen;
				if (jpc->jnl_buff->size == jpc->temp_free)
					jpc->temp_free = 0;
			} else
				JNL_PUTSTR(jpc, jb, (uchar_ptr_t)jfb->buff, rlen);
		}
	}
	assert((jpc->temp_free - jb->free + jb->size) % jb->size == rlen);
	assert(jb->buff[jb->free] == rectype);
	assert(jb->free < jpc->temp_free  ||  jpc->temp_free < jb->dsk);
	assert(jb->freeaddr >= jb->dskaddr);
	jpc->new_freeaddr = jb->freeaddr + rlen;
	assert(jpc->temp_free == jpc->new_freeaddr % jb->size);
	if (REPL_ENABLED(csa) && jrt_is_replicated[rectype])
	{
		assert(NULL != jnlpool.jnlpool_ctl && NULL != jnlpool_ctl); /* ensure we haven't yet detached from the jnlpool */
		assert((&FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs)->now_crit);	/* ensure we have the jnl pool lock */
		DEBUG_ONLY(jgbl.cu_jnl_index++;)
		srclen = jpc->jnl_buff->size - jb->free;
		dstlen = temp_jnlpool_ctl->jnlpool_size - temp_jnlpool_ctl->write;
		if (rlen <= srclen)
		{
			if (rlen <= dstlen)	/* dstlen & srclen >= rlen  (most frequent case) */
				memcpy(jnldata_base + temp_jnlpool_ctl->write, jb->buff + jb->free, rlen);
			else			/* dstlen < rlen <= srclen */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write, jb->buff + jb->free, dstlen);
				memcpy(jnldata_base, jb->buff + jb->free + dstlen, rlen - dstlen);
			}
		} else
		{
			if (rlen <= dstlen)		/* srclen < rlen <= dstlen */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write, jb->buff + jb->free, srclen);
				memcpy(jnldata_base + temp_jnlpool_ctl->write + srclen, jb->buff, rlen - srclen);
			} else if (dstlen == srclen)	/* dstlen = srclen < rlen */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write, jb->buff + jb->free, dstlen);
				memcpy(jnldata_base, jb->buff, rlen - dstlen);
			} else if (dstlen < srclen)	/* dstlen < srclen < rlen */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write, jb->buff + jb->free, dstlen);
				memcpy(jnldata_base, jb->buff + jb->free + dstlen, srclen - dstlen);
				memcpy(jnldata_base + srclen - dstlen, jb->buff, rlen - srclen);
			} else				/* srclen < dstlen < rlen */
			{
				memcpy(jnldata_base + temp_jnlpool_ctl->write, jb->buff + jb->free, srclen);
				memcpy(jnldata_base + temp_jnlpool_ctl->write + srclen, jb->buff, dstlen - srclen);
				memcpy(jnldata_base, jb->buff + dstlen - srclen, rlen - dstlen);
			}
		}
		temp_jnlpool_ctl->write += rlen;
		if (temp_jnlpool_ctl->write >= temp_jnlpool_ctl->jnlpool_size)
			temp_jnlpool_ctl->write -= temp_jnlpool_ctl->jnlpool_size;
	}
	/* Note that freeaddr should be updated ahead of free since jnl_output_sp.c does computation of wrtsize
	 * based on free and asserts follow later there which use freeaddr.
	 */
	jpc->free_update_inprog = TRUE; /* The following assignments must be protected against termination */
	jb->freeaddr = jpc->new_freeaddr;
	/* Write memory barrier here to enforce the fact that freeaddr *must* be seen to be updated before
	   free is is updated. It is less important if free is stale so we do not require a 2nd barrier for that
	   and will let the lock release (crit lock required since clustering not currently supported) do the
	   2nd memory barrier for us. This barrier takes care of this process's responsibility to broadcast
	   cache changes. It is up to readers to also specify a read memory barrier if necessary to receive
	   this broadcast.
	*/
	SHM_WRITE_MEMORY_BARRIER;
	jb->free = jpc->temp_free;
	jpc->free_update_inprog = FALSE;
	VMS_ONLY(
		if (((jb->freeaddr - jb->dskaddr) > jb->min_write_size)
		    && (SS_NORMAL != (status = jnl_qio_start(jpc))) && (ERR_JNLWRTNOWWRTR != status) && (ERR_JNLWRTDEFER != status))
	        {
			jb->blocked = 0;
			jnl_file_lost(jpc, status);
			return;
		}
	)
	if (dba_mm == jpc->region->dyn.addr->acc_meth)
		jnl_mm_timer(csa, jpc->region);
}
