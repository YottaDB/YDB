/****************************************************************
 *								*
 *	Copyright 2003, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>	/* for offsetof macro */
#if defined(UNIX)
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#elif defined(VMS)
#include <rms.h>
#include <iodef.h>
#include <psldef.h>
#include <ssdef.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif

#include "min_max.h"
#include "gtm_string.h"
#include "gtmio.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "mur_read_file.h"
#include "iosp.h"
#include "copy.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "gtmmsg.h"
#include "mur_validate_checksum.h"

error_def(ERR_JNLBADLABEL);
error_def(ERR_BOVTNGTEOVTN);
error_def(ERR_BOVTMGTEOVTM);
error_def(ERR_BEGSEQGTENDSEQ);
error_def(ERR_JNLBADRECFMT);
error_def(ERR_JNLREADBOF);
error_def(ERR_JNLREADEOF);
error_def(ERR_JNLFILOPN);
error_def(ERR_JNLINVALID);
error_def(ERR_NOPREVLINK);
error_def(ERR_JNLREAD);
error_def(ERR_GTMASSERT);
error_def(ERR_PREMATEOF);
error_def(ERR_JNLNOBIJBACK);
error_def(ERR_REPLNOTON);
error_def(ERR_TEXT);
error_def(ERR_JNLUNXPCTERR);

GBLREF	mur_rab_t	mur_rab;
GBLREF	mur_read_desc_t	mur_desc;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	int		mur_regno;
GBLREF	mur_opt_struct	mur_options;
GBLREF	mur_gbls_t	murgbl;
GBLREF	gd_region	*gv_cur_region;
LITREF	int		jrt_update[JRT_RECTYPES];
LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];


/*
 * Function name: mur_prev_rec
 * PreCondition	: mur_prev(offset > 0) must be done once, followed by zero or more calls to mur_prev_rec()
 * Input	: None
 * Output	: Sets mur_rab and  mur_ctl[mur_regno].jctl->rec_offset to appropriate value.
 *			May also change other globals if it changes generation.
 * Return	: SS_NORMAL on success, else error status
 * Description	: This routine reads immediate previous journal record from the last call to this routine or mur_prev()
 * 		  It can open previous generation journal and update mur_ctl[mur_regno].jctl
 * 		  It issues error message here as appropriate, so caller will not need to print the error again.
 */
uint4 mur_prev_rec(void)
{
	jnl_ctl_list	*jctl;
	uint4		status;

	assert(mur_jctl == mur_ctl[mur_regno].jctl);
	if (JNL_HDR_LEN < mur_jctl->rec_offset)
	{
		if (SS_NORMAL == (status = mur_prev(0)))
		{
			assert(mur_rab.jreclen == mur_rab.jnlrec->prefix.forwptr);
			mur_jctl->rec_offset -= mur_rab.jreclen;
			assert(mur_jctl->rec_offset >= mur_desc.cur_buff->dskaddr);
			assert(JNL_HDR_LEN <= mur_jctl->rec_offset);
			if (JRT_EOF != mur_rab.jnlrec->prefix.jrec_type)
				return SS_NORMAL;
			/* unexpected EOF record in the middle of the file */
			gtm_putmsg(VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset,
					ERR_TEXT, 2, LEN_AND_LIT("Unexpected EOF record found [prev_rec]"));
			status = ERR_JNLBADRECFMT;
		}
		if (ERR_JNLBADRECFMT != status)
			return status;
		if (!mur_options.update && !mur_jctl->tail_analysis)
		{	/* For tail_analysis only allow EXTRACT/SHOW/VERIFY to proceed after printing the error,
			 * if error_limit permits */
			if (!mur_report_error(MUR_JNLBADRECFMT))
				return status;
			return mur_valrec_prev(mur_jctl, 0, mur_jctl->rec_offset); /* continue in distress,
										look for other valid records */
		}
		return status; /* This may be in an offset at the tail of a journal file after a crash.
			   	* So, we do not issue any message here. Caller will handle the error */
	}
	assert(JNL_HDR_LEN == mur_jctl->rec_offset);
	/* Go to previous generation */
	if (NULL != mur_jctl->prev_gen)
		mur_ctl[mur_regno].jctl = mur_jctl = mur_jctl->prev_gen;
	else
	{
		if (mur_options.forward) 	/* For forward we have already included all possible journal files, so this	*/
			return ERR_JNLREADBOF;	/* is not really an error, just an indication that we reached beginning of file	*/
		/* open previous generation journal file as specified in journal file header */
		if (0 == mur_jctl->jfh->prev_jnl_file_name_length)
		{
			assert(mur_options.chain);
			return	ERR_NOPREVLINK;
		}
		if (!mur_insert_prev())
			return ERR_JNLFILOPN;
	}
	mur_jctl->rec_offset = mur_jctl->lvrec_off; /* lvrec_off was set in fread_eof that was called when we opened the file(s) */
	return mur_prev(mur_jctl->rec_offset);
}

/*
 * Function Name: mur_next_rec
 * PreCondition	: mur_next(offset > 0) must be done once, followed by zero or more calls to mur_next_rec()
 * Input	: None
 * Output	: Sets mur_rab and  mur_ctl[mur_regno].jctl->rec_offset to appropriate value.
 *			May also change other globals if it changes generation.
 * Return	: SS_NORMAL on success, else error status
 * Description	: This function reads immediate next journal record from last call to this routine or mur_next()
 * 		  It can open next generation journal and update mur_ctl[mur_regno]->jctl
 * 		  It issues error message here when necessary, so caller does not need to print the error again.
 */
uint4 mur_next_rec(void)
{
	int	rec_size;
	uint4	status;

	assert(mur_jctl == mur_ctl[mur_regno].jctl);
	if (mur_jctl->rec_offset < mur_jctl->lvrec_off)
	{
		assert(mur_rab.jreclen == mur_rab.jnlrec->prefix.forwptr);
		rec_size = mur_rab.jreclen;
		if (SS_NORMAL == (status = mur_next(0)))
		{
			mur_jctl->rec_offset += rec_size;
			assert(mur_jctl->rec_offset <= mur_jctl->lvrec_off);
			if (JRT_EOF != mur_rab.jnlrec->prefix.jrec_type || mur_jctl->rec_offset == mur_jctl->lvrec_off)
				return SS_NORMAL;
			gtm_putmsg(VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset,
					ERR_TEXT, 2, LEN_AND_LIT("Unexpected EOF record found [next_rec]"));
			status = ERR_JNLBADRECFMT;
		}
		if (ERR_JNLBADRECFMT != status)
			return status;
		if (!mur_options.update)
		{	/* only allow EXTRACT/SHOW/VERIFY to proceed after printing the error if error_limit permits */
			if (!mur_report_error(MUR_JNLBADRECFMT)) /* Issue error because mur_next_rec is called  from
								  * mur_forward(), from which errors are unexpected */
				return status;
			/* continue in distress, look for other valid records */
			return mur_valrec_next(mur_jctl, mur_jctl->rec_offset + rec_size);
		}
		rts_error(VARLSTCNT(9) ERR_JNLBADRECFMT, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset,
					ERR_TEXT, 2, LEN_AND_LIT("mur_next_rec"));
		return status;
	}
	assert(mur_jctl->rec_offset == mur_jctl->lvrec_off);
	if (NULL != mur_jctl->next_gen)
	{
		mur_ctl[mur_regno].jctl = mur_jctl = mur_jctl->next_gen;
		mur_jctl->rec_offset = JNL_HDR_LEN;
		return mur_next(JNL_HDR_LEN);
	}
	return ERR_JNLREADEOF;
}

/*
 * Routine name	: mur_prev
 * Input	: dskaddr
 * Output	: Sets mur_rab and  mur_ctl[mur_regno].jctl->rec_offset to appropriate value
 * Return	: SS_NORMAL on success
 * Pre-Condition: To read backward , first call must be with non_zero dskaddr
 *       	  subsequent call will be with 0 as dskaddr
 * Pre-Condition: Always first call to a journal file is mur_prev(n > 0).
 *		  Then all following calls are mur_prev(0) to read records sequentially backward.
 */
uint4 mur_prev(off_jnl_t dskaddr)
{
	off_jnl_t	buff_offset;
	uint4		status, partial_reclen;
	jrec_suffix	*suffix;
	mur_buff_desc_t	*swap_buff;
	boolean_t	good_suffix, good_prefix;

	if (0 != dskaddr)
	{ /* read record at dskaddr */
		assert(dskaddr < mur_jctl->eof_addr);
		assert(dskaddr >= JNL_HDR_LEN);
		if (dskaddr >= mur_jctl->eof_addr || dskaddr < JNL_HDR_LEN)
		{
			gtm_putmsg(VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, dskaddr,
					ERR_TEXT, 2, LEN_AND_LIT("Requested offset out of range [prev]"));
			return (dskaddr >= mur_jctl->eof_addr ? ERR_JNLREADEOF : ERR_JNLREADBOF);
		}
		assert(dskaddr == ROUND_UP2(dskaddr, JNL_REC_START_BNDRY)); /* dskaddr must be aligned at JNL_REC_START_BNDRY */
		MUR_FREAD_CANCEL(status);
		if (SS_NORMAL != status)
		{
			gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, dskaddr,
					ERR_TEXT, 2, LEN_AND_LIT("Could not cancel prior read [prev]"), mur_jctl->status);
			return mur_jctl->status;
		}
		mur_desc.index = 1;
		mur_desc.cur_buff = &mur_desc.seq_buff[mur_desc.index];
		mur_desc.sec_buff = &mur_desc.seq_buff[1 - mur_desc.index];
		mur_desc.cur_buff->dskaddr = ROUND_DOWN2(dskaddr, MUR_BUFF_SIZE);
		mur_desc.cur_buff->blen = MIN(MUR_BUFF_SIZE, mur_jctl->eof_addr - mur_desc.cur_buff->dskaddr);
		buff_offset = dskaddr - mur_desc.cur_buff->dskaddr;
		assert(JREC_PREFIX_UPTO_LEN_SIZE <= mur_desc.cur_buff->blen - buff_offset); /* we rely on reading at least up to
											     * the record length field (forwptr) */
		if (SS_NORMAL != (status = mur_freadw(mur_desc.cur_buff)))
		{
			gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_desc.cur_buff->dskaddr,
					ERR_TEXT, 2, LEN_AND_LIT("Error from synchronous read into cur_buff [prev]"), status);
			return status;
		}
		mur_rab.jnlrec = (jnl_record *)(mur_desc.cur_buff->base + buff_offset);
		good_suffix = TRUE;
		if (FALSE != (good_prefix = IS_VALID_LEN_FROM_PREFIX(mur_rab.jnlrec, mur_jctl->jfh)))
		{
			mur_rab.jreclen = mur_rab.jnlrec->prefix.forwptr;
			if (MUR_BUFF_SIZE <= mur_desc.cur_buff->dskaddr)
			{ /* while we process the just read chunk, post a read for the immediately preceding chunk */
				mur_desc.sec_buff->dskaddr = mur_desc.cur_buff->dskaddr - MUR_BUFF_SIZE;
				MUR_FREAD_START(mur_desc.sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
					   	mur_desc.sec_buff->dskaddr, ERR_TEXT, 2,
					   	LEN_AND_LIT("Could not initiate read into sec_buff in [prev] (dskaddr > 0)"),
						status);
					return status;
				}
			} else
			{ /* we read the beginning MUR_BUFF_SIZE (or smaller) chunk from file, no previous chunk exists */
				assert(0 == mur_desc.cur_buff->dskaddr);
			}
			if (buff_offset + mur_rab.jreclen > mur_desc.cur_buff->blen)
			{ /* Journal record straddles MUR_BUFF_SIZE boundary, did not read the entire record, read what's left into
			   * aux_buff2 which is located at the end of seq_buff[1], the current buffer */
				mur_desc.aux_buff2.dskaddr = mur_desc.cur_buff->dskaddr + mur_desc.cur_buff->blen;
				mur_desc.aux_buff2.blen = (buff_offset + mur_rab.jreclen - mur_desc.cur_buff->blen);
				if (FALSE != (good_prefix =
						(mur_jctl->eof_addr - mur_desc.aux_buff2.dskaddr >= mur_desc.aux_buff2.blen)))
				{
					if (SS_NORMAL != (status = mur_freadw(&mur_desc.aux_buff2)))
					{
						gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
							   mur_desc.aux_buff2.dskaddr, ERR_TEXT, 2,
							   LEN_AND_LIT("Error in synchronous read into aux_buff [prev]"), status);
						return status;
					}
				} else
				{
					gtm_putmsg(VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, mur_jctl->jnl_fn_len,
						mur_jctl->jnl_fn, dskaddr, ERR_TEXT, 2,
						LEN_AND_LIT("Requested offset beyond end of file [prev] (dskaddr > 0)"));
					return ERR_JNLBADRECFMT;
				}
			}
		} /* end good_prefix */
	} else
	{ /* dskaddr == 0, locate the previous record in the buffer, reading from disk if necessary */
		assert(JNL_HDR_LEN <= mur_jctl->rec_offset);
		suffix = (jrec_suffix *)((char *)mur_rab.jnlrec - JREC_SUFFIX_SIZE);
		if ((unsigned char *)suffix > mur_desc.cur_buff->base  && /* ok to test with possibly invalid backptr, we test */
		    (unsigned char *)mur_rab.jnlrec - suffix->backptr >= mur_desc.cur_buff->base) /* for validity below */
		{ /* prev record is contained wholely in the current buffer */
			if ((0 == mur_desc.index) &&
			    ((unsigned char *)mur_rab.jnlrec + mur_rab.jreclen > mur_desc.cur_buff->top)/* end of rec in sec_buff */
			    && (0 < mur_desc.cur_buff->dskaddr)) /* there is data to be read */
			{ /* we just finished processing the journal record that straddled seq_buff[0] and seq_buff[1],
			   * start read in the now free secondary buffer (seq_buff[1]) to overlap with processing */
				assert(MUR_BUFF_SIZE <= mur_desc.cur_buff->dskaddr);
				assert(!mur_desc.sec_buff->read_in_progress);
				mur_desc.sec_buff->dskaddr = mur_desc.cur_buff->dskaddr - MUR_BUFF_SIZE;
				MUR_FREAD_START(mur_desc.sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
						   mur_desc.sec_buff->dskaddr, ERR_TEXT, 2,
						   LEN_AND_LIT("Could not initiate read into sec_buff [prev] (dskaddr == 0)"),
						   status);
					return status;
				}
			}
		} else
		{ /* prev record completely in sec_buff or overlaps cur_buff and sec_buff */
			if (0 == mur_desc.index)
			{ /* copy partial record to just past the end of seq_buff[1], i.e., aux_seq_buff[1] to make the record
			   * available in contiguous memory */
				partial_reclen = (unsigned char *)mur_rab.jnlrec - mur_desc.seq_buff[0].base;
				if (0 < partial_reclen)
					memcpy(mur_desc.seq_buff[1].top, mur_desc.seq_buff[0].base, partial_reclen);
				suffix = (jrec_suffix *)(mur_desc.seq_buff[1].top + partial_reclen - JREC_SUFFIX_SIZE);
			}
			/* before switching the buffers, wait for completion of pending I/O */
			if (mur_desc.sec_buff->read_in_progress)
			{
				MUR_FREAD_WAIT(mur_desc.sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
							mur_desc.sec_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Error waiting for sec_buff read to complete [prev]"),
							status);
					return status;
				}
			}
			/* If possible, overlap I/O with processing, read into available buffer */
			if ((0 == mur_desc.index || /* we just copied partial record (if any), OR */
			    (unsigned char *)mur_rab.jnlrec == mur_desc.cur_buff->base) /* we completely processed cur_buff */
				&& 0 < mur_desc.sec_buff->dskaddr) /* there is data to be read */
			{
				assert(mur_desc.sec_buff->dskaddr >= MUR_BUFF_SIZE);
				mur_desc.cur_buff->dskaddr = mur_desc.sec_buff->dskaddr - MUR_BUFF_SIZE;
				MUR_FREAD_START(mur_desc.cur_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
							mur_desc.cur_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Could not initiate read into cur_buff [prev]"), status);
					return status;
				}
			}
			/* Make the buffer that has data that was previously read current */
			mur_desc.index = 1 - mur_desc.index;
			swap_buff = mur_desc.cur_buff;
			mur_desc.cur_buff = mur_desc.sec_buff;
			mur_desc.sec_buff = swap_buff;
		}
		good_prefix = TRUE;
		if (FALSE != (good_suffix = IS_VALID_LEN_FROM_SUFFIX(suffix, mur_jctl->jfh)))
		{
			mur_rab.jnlrec = (jnl_record *)((char *)suffix + JREC_SUFFIX_SIZE - suffix->backptr);
			assert((unsigned char *)mur_rab.jnlrec >= mur_desc.cur_buff->base);
			assert((unsigned char *)mur_rab.jnlrec <  mur_desc.cur_buff->top);
			mur_rab.jreclen = suffix->backptr;
			if (mur_jctl->rec_offset < mur_rab.jreclen + JNL_HDR_LEN)
			{
				gtm_putmsg(VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
					   mur_jctl->rec_offset, ERR_TEXT, 2,
					   LEN_AND_LIT("Requested offset beyond beginning of file [prev]"));
				return ERR_JNLBADRECFMT;
			}
		}
	} /* end of dskaddr == 0 */
	if (good_prefix && good_suffix && IS_VALID_JNLREC(mur_rab.jnlrec, mur_jctl->jfh))
		return SS_NORMAL;
	return ERR_JNLBADRECFMT;
}

/*
 * Routine name	: mur_next
 * Input	: dskaddr
 * Output	: Sets mur_rab and  mur_ctl[mur_regno].jctl->rec_offset to appropriate value
 * Return	: SS_NORMAL on success
 * Pre-Condition: Always first call to a journal file is mur_next(n > 0).
 *		  Then all following calls are mur_next(0) to read records sequentially.
 */
uint4 mur_next(off_jnl_t dskaddr)
{
	jrec_prefix	*prefix;
	off_jnl_t	buff_offset;
	uint4		status, partial_reclen;
	mur_buff_desc_t	*swap_buff;
	boolean_t	good_prefix;
	unsigned char	*buf_top;

	if (0 != dskaddr)
	{ /* read record at dskaddr */
		assert(mur_jctl->tail_analysis || (dskaddr >= JNL_HDR_LEN && dskaddr <= mur_jctl->lvrec_off));
		assert(dskaddr < mur_jctl->eof_addr);
		assert(dskaddr >= JNL_HDR_LEN);
		if (dskaddr >= mur_jctl->eof_addr || dskaddr < JNL_HDR_LEN)
		{
			gtm_putmsg(VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, dskaddr,
					ERR_TEXT, 2, LEN_AND_LIT("Requested offset out of range [next]"));
			return (dskaddr >= mur_jctl->eof_addr ? ERR_JNLREADEOF : ERR_JNLREADBOF);
		}
		assert(dskaddr == ROUND_UP2(dskaddr, JNL_REC_START_BNDRY)); /* dskaddr must be aligned at JNL_REC_START_BNDRY */
		MUR_FREAD_CANCEL(status);
		if (SS_NORMAL != status)
		{
			gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, dskaddr,
					ERR_TEXT, 2, LEN_AND_LIT("Could not cancel prior read [next]"), mur_jctl->status);
			return mur_jctl->status;
		}
		mur_desc.index = 0;
		mur_desc.cur_buff = &mur_desc.seq_buff[mur_desc.index];
		mur_desc.sec_buff = &mur_desc.seq_buff[1 - mur_desc.index];
		mur_desc.cur_buff->dskaddr = ROUND_DOWN2(dskaddr, MUR_BUFF_SIZE);
		mur_desc.cur_buff->blen = MIN(MUR_BUFF_SIZE, mur_jctl->eof_addr - mur_desc.cur_buff->dskaddr);
		buff_offset = dskaddr - mur_desc.cur_buff->dskaddr;
		assert(JREC_PREFIX_UPTO_LEN_SIZE <= mur_desc.cur_buff->blen - buff_offset); /* we rely on reading at least up to
											     * the record length field (forwptr) */
		if (SS_NORMAL != (status = mur_freadw(mur_desc.cur_buff)))
		{
			gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_desc.cur_buff->dskaddr,
					ERR_TEXT, 2, LEN_AND_LIT("Error from synchronous read into cur_buff [next]"),
					status);
			return status;
		}
		mur_rab.jnlrec = (jnl_record *)(mur_desc.cur_buff->base + buff_offset);
		if (FALSE != (good_prefix = IS_VALID_LEN_FROM_PREFIX(mur_rab.jnlrec, mur_jctl->jfh)))
		{
			mur_rab.jreclen = mur_rab.jnlrec->prefix.forwptr;
			if (MUR_BUFF_SIZE < mur_jctl->eof_addr - mur_desc.cur_buff->dskaddr) /* data available to be read */
			{ /* while we process the just read chunk, post a read for the immediately succeeding chunk */
				assert(MUR_BUFF_SIZE == mur_desc.cur_buff->blen);
				mur_desc.sec_buff->dskaddr = mur_desc.cur_buff->dskaddr + MUR_BUFF_SIZE;
				MUR_FREAD_START(mur_desc.sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
							mur_desc.sec_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Could not initiate read into sec_buff [next] (dskaddr > 0)"),
							status);
					return status;
				}
				if (((unsigned char *)mur_rab.jnlrec + mur_rab.jreclen) > mur_desc.cur_buff->top)
				{ /* Journal record straddles MUR_BUFF_SIZE boundary, did not read the record in its entirey, wait
				   * for the rest of the record to be read into seq_buff[1], the secondary buffer */
					MUR_FREAD_WAIT(mur_desc.sec_buff, status);
					if (SS_NORMAL != status)
					{
						gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
						    mur_desc.sec_buff->dskaddr, ERR_TEXT, 2,
						    LEN_AND_LIT("Error waiting for sec_buff read to complete [next] (dskaddr > 0)"),
						    status);
						return status;
					}
					/* is the record available in its entirety? */
					good_prefix = (buff_offset + mur_rab.jreclen <=
							mur_desc.cur_buff->blen + mur_desc.sec_buff->blen);
				}
			} else /* we just read the last chunk, nothing more to read */
				good_prefix = (buff_offset + mur_rab.jreclen <= mur_desc.cur_buff->blen); /* is the record available
													   * in its entirety? */
			if (!good_prefix)
			{
				gtm_putmsg(VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, dskaddr,
					   ERR_TEXT, 2, LEN_AND_LIT("Requested offset beyond end of file [next] (dskaddr > 0)"));
				return ERR_JNLBADRECFMT;
			}
		} /* end good_prefix */
	} else
	{ /* dskaddr == 0, locate the next record in the buffer, reading from disk if necessary */
		prefix = (jrec_prefix *)((char *)mur_rab.jnlrec + mur_rab.jreclen); /* position to the next rec */
		buf_top = mur_desc.cur_buff->base + mur_desc.cur_buff->blen; /* we might not have read a full MUR_BUFF_SIZE chunk */
		good_prefix = ((unsigned char *)prefix + JREC_PREFIX_UPTO_LEN_SIZE < buf_top && /* ok to test with possibly 	*/
		    	       (unsigned char *)prefix + prefix->forwptr <= buf_top);		/* invalid forwptr, we test for	*/
												/*  validity below		*/
		if (good_prefix)
		{ /* next record is contained wholely in the current buffer */
			if ((1 == mur_desc.index) &&
			    ((unsigned char *)mur_rab.jnlrec < mur_desc.cur_buff->base) && /* beginning of rec in sec_buff */
			    (MUR_BUFF_SIZE < mur_jctl->eof_addr - mur_desc.cur_buff->dskaddr)) /* there is data to be read */
			{ /* we just finished processing the journal record that straddled seq_buff[0] and seq_buff[1],
			   * start read in the now free secondary buffer (seq_buff[0]) to overlap with processing */
				assert(MUR_BUFF_SIZE == mur_desc.cur_buff->blen);
				assert(!mur_desc.sec_buff->read_in_progress);
				mur_desc.sec_buff->dskaddr = mur_desc.cur_buff->dskaddr + MUR_BUFF_SIZE;
				MUR_FREAD_START(mur_desc.sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
						   mur_desc.sec_buff->dskaddr, ERR_TEXT, 2,
						   LEN_AND_LIT("Could not initiate read into sec_buff [next] (dskaddr == 0)"),
						   status);
					return status;
				}
			}
		} else if (mur_jctl->eof_addr - mur_jctl->rec_offset > mur_rab.jreclen + JREC_PREFIX_UPTO_LEN_SIZE)/* within eof? */
		{ /* next record completely in sec_buff or overlaps cur_buff and sec_buff */
			if (1 == mur_desc.index)
			{ /* copy partial record to just prior to the beginning of seq_buf[0], i.e., aux_buff1
			   * to make the record available in contiguous memory */
				partial_reclen = buf_top - (unsigned char *)prefix;
				if (0 < partial_reclen)
					memcpy(mur_desc.seq_buff[0].base - partial_reclen, (char *)prefix, partial_reclen);
				prefix = (jrec_prefix *)(mur_desc.seq_buff[0].base - partial_reclen);
			}
			/* before switching the buffers, wait for completion of pending I/O */
			if (mur_desc.sec_buff->read_in_progress)
			{
				MUR_FREAD_WAIT(mur_desc.sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
						   mur_desc.sec_buff->dskaddr, ERR_TEXT, 2,
						   LEN_AND_LIT("Error waiting for sec_buff read to complete [next] (dskaddr == 0)"),
						   status);
					return status;
				}
			}
			/* If possible, overlap I/O with processing, read into available buffer */
			if ((1 == mur_desc.index || /* we just copied partial record (if any), OR */
			    (unsigned char *)prefix >= mur_desc.cur_buff->top) && /* we completely processed cur_buff */
			    MUR_BUFF_SIZE < mur_jctl->eof_addr - MAX(mur_desc.sec_buff->dskaddr, mur_desc.cur_buff->dskaddr))
			/* there is data to be read; MAX magic is for when no read was posted to sec_buff (last chunk in file) */
			{
				mur_desc.cur_buff->dskaddr = mur_desc.sec_buff->dskaddr + MUR_BUFF_SIZE;
				MUR_FREAD_START(mur_desc.cur_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg(VARLSTCNT(10) ERR_JNLREAD, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
							mur_desc.cur_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Could not initiate read into cur_buff [next]"), status);
					return status;
				}
			}
			/* Make the buffer that has data that was previously read current */
			mur_desc.index = 1 - mur_desc.index;
			swap_buff = mur_desc.cur_buff;
			mur_desc.cur_buff = mur_desc.sec_buff;
			mur_desc.sec_buff = swap_buff;
			/* record available in its entirety? */
			good_prefix = (mur_jctl->eof_addr - mur_jctl->rec_offset >= mur_rab.jreclen + prefix->forwptr);
		}
		if (good_prefix)
		{
			if (FALSE != (good_prefix = IS_VALID_LEN_FROM_PREFIX((jnl_record *)prefix, mur_jctl->jfh)))
			{
				mur_rab.jnlrec = (jnl_record *)prefix;
				mur_rab.jreclen = prefix->forwptr;
			}
		} else
		{
			if (!mur_jctl->tail_analysis)
			{
				gtm_putmsg(VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, mur_jctl->jnl_fn_len,
					mur_jctl->jnl_fn, mur_jctl->rec_offset,
					ERR_TEXT, 2, LEN_AND_LIT("Requested offset beyond end of file [next] (dskaddr == 0)"));
				return ERR_JNLBADRECFMT;
			}
			return ERR_JNLREADEOF;
		}
	} /* end of dskaddr == 0 */
	if (good_prefix && IS_VALID_JNLREC(mur_rab.jnlrec, mur_jctl->jfh))
		return SS_NORMAL;
	return ERR_JNLBADRECFMT;
}

/*
 * Function Name: mur_read
 * Output: SS_NORMAL on successful
 *         error status on unsuccessful
 * This function is called when reads are not sequential and double buffering will not help.
 * Also this only reads raw data without doing any record validation or processing.
 */
uint4 mur_read(jnl_ctl_list *jctl)
{
	assert(0 < mur_desc.random_buff.blen);
	assert(mur_desc.random_buff.blen <= MUR_BUFF_SIZE);
	assert(mur_desc.random_buff.dskaddr == ROUND_UP2(mur_desc.random_buff.dskaddr, DISK_BLOCK_SIZE));
	DO_FILE_READ(jctl->channel, mur_desc.random_buff.dskaddr, mur_desc.random_buff.base, mur_desc.random_buff.blen,
			jctl->status, jctl->status2);
	return jctl->status;
}

/*
 * Function name: mur_freadw
 * Input  : struct mur_buffer_desc * buff
 * Output : SS_NORMAL on successful
 *          error status on unsuccessful
 * This function reads synchronously
 * The caller function will set buff->blen, buff->dskaddr and check error status
 */
uint4 mur_freadw(mur_buff_desc_t *buff)
{
	assert(mur_jctl->eof_addr > buff->dskaddr); /* should never be reading at or beyond end of file */
	buff->read_in_progress = FALSE;
	DO_FILE_READ(mur_jctl->channel, buff->dskaddr, buff->base, buff->blen, mur_jctl->status, mur_jctl->status2);
	return mur_jctl->status;
}

/*
 * Function name: mur_fread_eof
 * Input: jctl
 * Return: SS_NORMAL on success else failure status
 * Description:
 * 	This routine sets following fields (already mur_fopen must be called) :
 * 		jctl->properly_closed
 * 		jctl->lvrec_off
 * 		jctl->eof_addr
 * 		jctl->lvrec_time
 */
uint4	mur_fread_eof(jnl_ctl_list *jctl)
{
	jnl_record	*rec;
	jnl_file_header	*jfh;
	uint4		status, lvrec_off;

	if (mur_options.show_head_only) /* only SHOW HEADER, no need for time consuming search for valid eof */
		return SS_NORMAL;
	jctl->tail_analysis = TRUE;
	jfh = jctl->jfh;
	if (0 != jfh->prev_recov_end_of_data)
	{ 	/* regardless of jfh->crash, prev_recov_end_of_data and end_of_data must point to valid records since
		 * earlier recovery processed the file to determine these values */
		assert(!jfh->recover_interrupted);
		assert(!jfh->crash);
		lvrec_off = !mur_options.extract_full ? jfh->end_of_data : jfh->prev_recov_end_of_data;
		if (SS_NORMAL != (status = mur_prev(lvrec_off)))
			return status;
		jctl->lvrec_off = lvrec_off;
		jctl->eof_addr = lvrec_off + mur_rab.jnlrec->prefix.forwptr;
		jctl->lvrec_time = mur_rab.jnlrec->prefix.time;
		jctl->properly_closed = TRUE;
		jctl->tail_analysis = FALSE;
		return SS_NORMAL;
	}
	if (!jfh->crash && (SS_NORMAL == mur_prev(jfh->end_of_data)))
	{
		rec = mur_rab.jnlrec;
		if (JRT_EOF == rec->prefix.jrec_type && rec->prefix.tn >= jfh->bov_tn &&
			(!mur_options.rollback || ((struct_jrec_eof *)rec)->jnl_seqno >= jfh->end_seqno))
		{ /* valid EOF */
			jctl->lvrec_off = jfh->end_of_data;
			jctl->eof_addr = jctl->lvrec_off + EOF_RECLEN;
			jctl->lvrec_time = mur_rab.jnlrec->prefix.time;
			jctl->properly_closed = TRUE;
			jctl->tail_analysis = FALSE;
			return SS_NORMAL;
		}
	}
	/* crash or invalid end */
	return mur_fread_eof_crash(jctl, jfh->end_of_data, jctl->eof_addr);
}

/*
 * Function name: mur_fread_eof_crash
 * Input: jctl, lo_off, hi_off
 * Return: SS_NORMAL on success else failure status
 * Description: This routine is same as mur_fread_eof except it is called when journal is improper for a crash
 */
uint4 mur_fread_eof_crash(jnl_ctl_list *jctl, off_jnl_t lo_off, off_jnl_t hi_off)
{
	uint4		status;

	assert(JNL_HDR_LEN <= jctl->jfh->alignsize);
	if (lo_off < jctl->jfh->end_of_data || lo_off > hi_off)
		GTMASSERT;
	jctl->properly_closed = FALSE;
	if (SS_NORMAL != (status = mur_valrec_prev(jctl, lo_off, hi_off)))
		return status;
	jctl->lvrec_off = jctl->rec_offset;
	jctl->eof_addr = jctl->rec_offset + mur_rab.jreclen;
	jctl->lvrec_time = mur_rab.jnlrec->prefix.time;
	return SS_NORMAL;
}


/*
 * module name: mur_valrec_prev
 * input:jnl_clt_list *jctl
 *	 off_jnl_t lo_off
 *	 off_jnl_t hi_off (hi_off is the end of the last record, not the start of last record)
 * Return: SS_NORMAL on success else failure status
 * Description:
 * 	This function searches for a valid record at align offset, between hi_off and lo_off
 * 	After finding a valid journal record at any mid_off at a aligned boundary,
 * 	it scans forward to find the last valid record in that align_sized block and sets
 * 	jctl->rec_offset as the offset of the valid record of that journal file
 * 	mur_fread_eof() and mur_prev() call this function
 */
uint4 mur_valrec_prev(jnl_ctl_list *jctl, off_jnl_t lo_off, off_jnl_t hi_off)
{
	off_jnl_t	mid_off, new_mid_off, rec_offset, mid_further;
	boolean_t	this_rec_valid;
	jnl_file_header	*jfh;
	jrec_prefix	*prefix;
	uint4		status, rec_len, blen;
	jnl_record	*rec;
	trans_num	rec_tn;
	seq_num		rec_seqno;

	jfh = jctl->jfh;
	assert(jctl->tail_analysis || jctl->after_end_of_data);
	assert(lo_off <= jctl->eof_addr);
	assert(hi_off >= lo_off);
	assert(hi_off <= jctl->eof_addr);
	if (lo_off == jctl->eof_addr)
	{ /* special case when system crashes immediately after jnl file extension, back off by not more than an alignsize chunk */
		assert(hi_off == jctl->eof_addr);
		if (lo_off > jfh->alignsize)
			lo_off -= jfh->alignsize;
		else
			lo_off = 0; /* start from the beginning of the file */
	}
	for (mid_off = ROUND_DOWN2(((lo_off >> 1) + (hi_off >> 1)), jfh->alignsize), jctl->rec_offset = 0; ; )
	{
		assert(lo_off <= hi_off);
		assert(mid_off < hi_off);	/* Note : It is also possible that, mid_off < lo_off */
		assert(MUR_BUFF_SIZE >= jfh->max_phys_reclen);
		/* max_phys_reclen size read enough to verify valid record */
		blen = (0 != mid_off) ? MIN(jfh->max_phys_reclen, jctl->eof_addr - mid_off) :
					MIN(JNL_HDR_LEN + jfh->max_phys_reclen, jctl->eof_addr);
		assert(MUR_BUFF_SIZE >= blen);
		mur_desc.random_buff.blen = blen;
		mur_desc.random_buff.dskaddr = mid_off;
		if (SS_NORMAL != (status = mur_read(jctl)))
		{
			gtm_putmsg(VARLSTCNT(6) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn, mur_desc.random_buff.dskaddr,
					status);
			return status;
		}
		rec = (jnl_record *)mur_desc.random_buff.base;
		if (0 == mid_off)
			rec = (jnl_record *)((unsigned char *)rec + JNL_HDR_LEN);
		/* check the validity of the record */
		this_rec_valid = (blen > JREC_PREFIX_UPTO_LEN_SIZE && blen >= rec->prefix.forwptr && IS_VALID_JNLREC(rec, jfh));
		if (this_rec_valid)
		{
			assert(mid_off >= jctl->rec_offset || 0 == mid_off); /* are we by any chance going backwards? */
			jctl->rec_offset = MAX(mid_off, JNL_HDR_LEN);
			mur_rab.jnlrec = rec;
			mur_rab.jreclen = rec->prefix.forwptr;
			this_rec_valid = (jctl->rec_offset + mur_rab.jreclen <= hi_off) && mur_validate_checksum();
		}
		if (mid_off <= lo_off)
			break;
		if (this_rec_valid)
			lo_off = mid_off;
		else
			hi_off = mid_off;
		new_mid_off = ROUND_DOWN2(((lo_off >> 1) + (hi_off >> 1)), jfh->alignsize);
		mid_further = (new_mid_off != mid_off) ? 0 : jfh->alignsize; /* if necessary, move further to avoid repeat search */
		if (hi_off - new_mid_off <= mid_further)
			break;
		mid_off = new_mid_off + mid_further;
	}
	if (0 == jctl->rec_offset)
	{	/* Unexpected condition:
		 * a) If lo_off was 0 while entering this routine, this means there was no good record at or after the beginning
		 * 	of the journal file (i.e. offset of JNL_HDR_LEN). This is impossible since a PINI record was written at
		 * 	journal file creation time and at least that should have been seen as a good record.
		 * b) If lo_off was non-zero while entering this routine, this implies there was no good record anywhere at or
		 * 	after an offset of lo_off in the journal file. But this is impossible since a non-zero value of lo_off
		 * 	implies it is equal to jfh->end_of_data which means that an EPOCH/EOF record written at that offset
		 * 	and the journal file was synced so we should at least see that good record.
		 */
		GTMASSERT;
	}
	/* We found a valid record at mid_off, go forward to find the last valid record.  Before the for loop
	 * of mur_next(0) following call is necessary to initialize buffers and for the assumptions in mur_next() */
	rec_offset = jctl->rec_offset;
	if (SS_NORMAL != (status = mur_next(rec_offset)))
	{
		gtm_putmsg(VARLSTCNT(9) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn, rec_offset,
			ERR_TEXT, 2, LEN_AND_LIT("Error received from mur_valrec_prev calling mur_next(rec_offset)"));
		return status;
	}
	/* Note: though currently it is required not to change system time when journaling is on,
	 *       we do not check time continuity here. We hope that in future we will remove the restriction */
	rec_len = mur_rab.jnlrec->prefix.forwptr;
	rec_tn = mur_rab.jnlrec->prefix.tn;
	rec_seqno = 0;
	assert(rec_offset + rec_len <= hi_off);	/* We get inside for loop below at least once */
	assert(jctl == mur_jctl);
	for ( ; jctl->rec_offset + rec_len <= hi_off;
				rec_len = mur_rab.jnlrec->prefix.forwptr, rec_tn = mur_rab.jnlrec->prefix.tn)
	{
		rec_offset = jctl->rec_offset;
		if (SS_NORMAL != mur_next(0) || (mur_rab.jnlrec->prefix.tn != rec_tn && mur_rab.jnlrec->prefix.tn != rec_tn + 1))
			break;
		if (mur_options.rollback && REC_HAS_TOKEN_SEQ(mur_rab.jnlrec->prefix.jrec_type))
		{
			if (GET_JNL_SEQNO(mur_rab.jnlrec) < rec_seqno) /* if jnl seqno is not in sequence */
				break;
			rec_seqno = GET_JNL_SEQNO(mur_rab.jnlrec);
		}
		jctl->rec_offset += rec_len;	/* jctl->rec_offset is used in checksum calculation */
		if (!mur_validate_checksum())
			break;
	}
	jctl->rec_offset = rec_offset;
	/* now set mur_rab fields to point to valid record */
	assert(rec_offset);
	if (SS_NORMAL != mur_prev(rec_offset)) /* Since valid record may have been overwritten we must do this.
				       		* We call mur_prev, not mur_next, although both position mur_rab to the same
				       		* record (when called with non zero arg) to make sure no assumptions in
				       		* successive calls to mur_prev(0) are violated. */
		GTMASSERT;
	return SS_NORMAL;
}

/*
 * Function Name: mur_valrec_next()
 * Input:jnl_ctl_list *jctl
 *        off_jnl_t lo_off
 * Return: SS_NORMAL on succees, error status if unsuccessful
 * mur_next() calls this function
 */
uint4 mur_valrec_next(jnl_ctl_list *jctl, off_jnl_t lo_off)
{
	jnl_file_header	*jfh;
	uint4		status;

	jfh = jctl->jfh;
	jctl->rec_offset = ROUND_UP2(lo_off, jfh->alignsize);
	if (jctl->rec_offset == lo_off)
		jctl->rec_offset += jfh->alignsize;
	if (jctl->rec_offset > jctl->lvrec_off)
		jctl->rec_offset = jctl->lvrec_off;
	for (; jctl->rec_offset <= jctl->lvrec_off; jctl->rec_offset = MIN(jctl->lvrec_off, jctl->rec_offset + jfh->alignsize))
	{ /* a linear search for the alignsize block beginning with valid record is sufficient since we believe the *next* valid
	   * record is located nearby */
		if (SS_NORMAL == (status = mur_prev(jctl->rec_offset)))
			break;
		if (ERR_JNLBADRECFMT != status)
			return status; /* I/O error or unexpected failure */
		if (jctl->rec_offset >= jctl->lvrec_off) /* lvrec_off must have a valid record */
			GTMASSERT;
	}
	assert(SS_NORMAL == status);
	/* now work backwards to find the earliest valid record in the immediately preceding alignsize chunk */
	for ( ; SS_NORMAL == mur_prev(0); jctl->rec_offset -= mur_rab.jreclen)
		;
	assert(JNL_FILE_FIRST_RECORD <= jctl->rec_offset);
	/* now set mur_rab fields to point to valid record */
	assert(jctl->rec_offset > lo_off);
	return (mur_next(jctl->rec_offset)); /* A conservative and safe approach since valid record may have been overwritten.
					      * We call mur_next, not mur_prev, although both position mur_rab to the same
					      * record (when called with non zero arg) to make sure no assumptions in a following
					      * call to mur_next(0) are violated. We leave the buffers in a state suitable
					      * for following mur_next(0) call. */
}

/*
 * Function name: mur_fopen
 * Input: jnl_ctl_list *
 * Return value : TRUE or False
 * This function opens the journal file , checks JNLLABEL in header
 */
boolean_t mur_fopen(jnl_ctl_list *jctl)
{
	jnl_file_header	*jfh;
	char		jrecbuf[PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN];
	jnl_record	*jrec;
	int		cre_jnl_rec_size;

	if (!mur_fopen_sp(jctl))
		return FALSE;
	jctl->eof_addr = jctl->os_filesize;
	assert(NULL == jctl->jfh);
	jfh = jctl->jfh = (jnl_file_header *)malloc(JNL_HDR_LEN);
	if (JNL_HDR_LEN < jctl->os_filesize)
	{
		DO_FILE_READ(jctl->channel, 0, jfh, JNL_HDR_LEN, jctl->status, jctl->status2);
		if (SS_NORMAL == jctl->status)
		{
			cre_jnl_rec_size = jfh->before_images ? PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN :
								PINI_RECLEN + PFIN_RECLEN + EOF_RECLEN;
			if (cre_jnl_rec_size + JNL_HDR_LEN <= jctl->os_filesize)
			{
				DO_FILE_READ(jctl->channel, JNL_HDR_LEN, jrecbuf, cre_jnl_rec_size, jctl->status, jctl->status2);
			} else
				jctl->status = ERR_JNLINVALID;
		}
	} else
		jctl->status = ERR_JNLINVALID;
	if (SS_NORMAL != jctl->status) /* read failed or size of jnl file less than minimum expected */
	{
		if (ERR_JNLINVALID == jctl->status)
			gtm_putmsg(VARLSTCNT(10) ERR_JNLINVALID, 4, jctl->jnl_fn_len, jctl->jnl_fn, jfh->data_file_name_length,
					jfh->data_file_name, ERR_TEXT, 2,
					LEN_AND_LIT("File size is less than minimum expected for a valid journal file"));
		else if (SS_NORMAL != jctl->status2)
			gtm_putmsg(VARLSTCNT1(7) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn, 0, jctl->status,
					PUT_SYS_ERRNO(jctl->status2));
		else
			gtm_putmsg(VARLSTCNT(6) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn, 0, jctl->status);
		return FALSE;
	}
	if (0 != MEMCMP_LIT(jfh->label, JNL_LABEL_TEXT))
	{
		gtm_putmsg(VARLSTCNT(4) ERR_JNLBADLABEL, 2, jctl->jnl_fn_len, jctl->jnl_fn);
		return FALSE;
	}
	if (!mur_options.forward && !jfh->before_images)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_JNLNOBIJBACK, 2, jctl->jnl_fn_len, jctl->jnl_fn);
		return FALSE;
	}
	if (!REPL_ALLOWED(jfh) && mur_options.rollback)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_REPLNOTON, 2, jctl->jnl_fn_len, jctl->jnl_fn);
		return FALSE;
	}
	jrec = (jnl_record *)jrecbuf;
	if (!IS_VALID_JNLREC(jrec, jfh) || JRT_PINI != jrec->prefix.jrec_type)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len, jctl->jnl_fn, JNL_HDR_LEN, ERR_TEXT, 2,
				LEN_AND_LIT("Invalid or no PINI record found"));
		return FALSE;
	}
	/* We have at least one good record */
	if (jfh->before_images)
	{
		jrec = (jnl_record *)((char *)jrec + PINI_RECLEN);
		if (!IS_VALID_JNLREC(jrec, jfh) || JRT_EPOCH != jrec->prefix.jrec_type)
		{
			gtm_putmsg(VARLSTCNT(9) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len, jctl->jnl_fn, JNL_HDR_LEN + PINI_RECLEN,
					ERR_TEXT, 2, LEN_AND_LIT("Invalid or no EPOCH record found"));
			return FALSE;
		}
		/* We have at least one valid EPOCH */
	}
	if (mur_options.update && jfh->bov_tn > jfh->eov_tn)
	{
		gtm_putmsg(VARLSTCNT(6) ERR_BOVTNGTEOVTN, 4, jctl->jnl_fn_len, jctl->jnl_fn, &jfh->bov_tn, &jfh->eov_tn);
		return FALSE;
	}
	if (jfh->bov_timestamp > jfh->eov_timestamp)
	{	/* This is not a severe error to exit, may be user changed system time which we do not allow now.
		 * But we can still try to continue recovery. We already removed time continuty check from mur_fread_eof().
		 * So if error limit allows, we will continue recovery  */
		gtm_putmsg(VARLSTCNT(6) ERR_BOVTMGTEOVTM, 4, jctl->jnl_fn_len, jctl->jnl_fn,
							jfh->bov_timestamp, jfh->eov_timestamp);
		/* since mur_jctl global is not ready yet mur_report_error does not print message */
		if (!mur_report_error(MUR_BOVTMGTEOVTM))
			return FALSE;
	}
	if (mur_options.rollback && jfh->start_seqno > jfh->end_seqno)
	{
		gtm_putmsg(VARLSTCNT(6) ERR_BEGSEQGTENDSEQ, 4, jctl->jnl_fn_len, jctl->jnl_fn, &jfh->start_seqno, &jfh->end_seqno);
		return FALSE;
	}
	init_hashtab_int4(&jctl->pini_list, MUR_PINI_LIST_INIT_ELEMS);
	/* Please investigate if murgbl.max_extr_record_length is more than what a VMS record (in a line) can handle ??? */
	if (murgbl.max_extr_record_length < ZWR_EXP_RATIO(jctl->jfh->max_logi_reclen))
		murgbl.max_extr_record_length = ZWR_EXP_RATIO(jctl->jfh->max_logi_reclen);
	return TRUE;
}

boolean_t mur_fclose(jnl_ctl_list *jctl)
{
	error_def(ERR_JNLFILECLOSERR);

	if (NOJNL == jctl->channel) /* possible if mur_fopen() errored out */
		return TRUE;
	if (NULL != jctl->jfh)
	{
		free(jctl->jfh);
		jctl->jfh = NULL;
	}
	free_hashtab_int4(&jctl->pini_list);
	if (SS_NORMAL == (jctl->status = UNIX_ONLY(close)VMS_ONLY(sys$dassgn)(jctl->channel)))
	{
		jctl->channel = NOJNL;
		return TRUE;
	}
	UNIX_ONLY(jctl->status = errno;)
	gtm_putmsg(VARLSTCNT(5) ERR_JNLFILECLOSERR, 2, jctl->jnl_fn_len, jctl->jnl_fn, jctl->status);
	return FALSE;
}
