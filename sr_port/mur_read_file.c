/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
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
#include "hashtab.h"
#include "muprec.h"
#include "mur_read_file.h"
#include "iosp.h"
#include "copy.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "gtmmsg.h"
#include "mur_validate_checksum.h"
#include "repl_sp.h"		/* for F_CLOSE (used by JNL_FD_CLOSE) */
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#include "error.h"
#endif

error_def(ERR_BEGSEQGTENDSEQ);
error_def(ERR_BOVTNGTEOVTN);
error_def(ERR_GTMASSERT);
error_def(ERR_JNLBADRECFMT);
error_def(ERR_JNLFILECLOSERR);
error_def(ERR_JNLFILRDOPN);
error_def(ERR_JNLINVALID);
error_def(ERR_JNLNOBIJBACK);
error_def(ERR_JNLREAD);
error_def(ERR_JNLREADBOF);
error_def(ERR_JNLREADEOF);
error_def(ERR_JNLUNXPCTERR);
error_def(ERR_NOPREVLINK);
error_def(ERR_PREMATEOF);
error_def(ERR_REPLNOTON);
error_def(ERR_RLBKJNLNOBIMG);
error_def(ERR_TEXT);

GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_opt_struct	mur_options;
GBLREF	mur_gbls_t	murgbl;
GBLREF	gd_region	*gv_cur_region;
GTMCRYPT_ONLY(
	GBLREF	int	process_exiting;
)

/*
 * Function name: mur_prev_rec
 * PreCondition	: mur_prev(offset > 0) must be done once, followed by zero or more calls to mur_prev_rec()
 * Input	: jjctl (pointer to jctl)
 * Output	: Sets jctl->reg_ctl->mur_desc->jnlrec, jctl->reg_ctl->mur_desc->jreclen and jctl->rec_offset to appropriate value.
 *			Changes jjctl if it changes generation.
 * Return	: SS_NORMAL on success, else error status
 * Description	: This routine reads immediate previous journal record from the last call to this routine or mur_prev()
 * 		  It can open previous generation journal and update rctl->jctl and passed in "jjctl" to reflect new jctl
 * 		  It issues error message here as appropriate, so caller will not need to print the error again.
 */
uint4 mur_prev_rec(jnl_ctl_list **jjctl)
{
	jnl_ctl_list	*jctl;
	uint4		status;
	mur_read_desc_t	*mur_desc;

	jctl = *jjctl;
	if (JNL_HDR_LEN < jctl->rec_offset)
	{
		if (SS_NORMAL == (status = mur_prev(jctl, 0)))
		{
			mur_desc = jctl->reg_ctl->mur_desc;
			assert(mur_desc->jreclen == mur_desc->jnlrec->prefix.forwptr);
			jctl->rec_offset -= mur_desc->jreclen;
			assert(jctl->rec_offset >= mur_desc->cur_buff->dskaddr);
			assert(JNL_HDR_LEN <= jctl->rec_offset);
			if (JRT_EOF != mur_desc->jnlrec->prefix.jrec_type)
				return SS_NORMAL;
			/* unexpected EOF record in the middle of the file */
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					jctl->rec_offset, ERR_TEXT, 2, LEN_AND_LIT("Unexpected EOF record found [prev_rec]"));
			status = ERR_JNLBADRECFMT;
		}
		if (ERR_JNLBADRECFMT != status)
			return status;	/* JNLBADRECFMT message already issued by mur_prev */
		if (!mur_options.update && !jctl->tail_analysis)
		{	/* If not in tail_analysis, allow EXTRACT/SHOW/VERIFY to proceed after printing the error,
			 * if error_limit permits */
			if (!mur_report_error(jctl, MUR_JNLBADRECFMT))
				return status;
			return mur_valrec_prev(jctl, 0, jctl->rec_offset); /* continue in distress, look for other valid records */
		}
		if (jctl->rec_offset < jctl->jfh->end_of_data)
		{	/* This is an offset well before end_of_data so should contain valid records
			 * irrespective of whether this is a backward or forward recovery. Issue error right away.
			 * Notice that the offset jctl->rec_offset points to a good record. The badly formatted
			 * journal record is actually one record BEFORE the printed offset.
			 */
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					jctl->rec_offset, ERR_TEXT, 2, LEN_AND_LIT("Error accessing previous record"));
		} else
		{	/* offset is at tail of journal file after crash. Caller could be backward recovery or
			 * forward recovery or even extract/verify/show if they are in tail_analysis. We expect
			 * caller to handle this situation. So do not issue any message here. Just return status.
			 */
		}
		return status;
	}
	assert(JNL_HDR_LEN == jctl->rec_offset);
	/* Go to previous generation */
	if (NULL != jctl->prev_gen)
	{
		jctl = jctl->prev_gen;
		jctl->reg_ctl->jctl = jctl;
	} else
	{
		if (mur_options.forward) 	/* For forward we have already included all possible journal files, so this	*/
			return ERR_JNLREADBOF;	/* is not really an error, just an indication that we reached beginning of file	*/
		/* open previous generation journal file as specified in journal file header */
		if (0 == jctl->jfh->prev_jnl_file_name_length)
		{
			assert(mur_options.chain);
			return	ERR_NOPREVLINK;
		}
		if (!mur_insert_prev(&jctl))
			return ERR_JNLFILRDOPN;
	}
	jctl->rec_offset = jctl->lvrec_off; /* lvrec_off was set in fread_eof that was called when we opened the file(s) */
	*jjctl = jctl;
	return mur_prev(jctl, jctl->rec_offset);
}

/*
 * Function Name: mur_next_rec
 * PreCondition	: mur_next(offset > 0) must be done once, followed by zero or more calls to mur_next_rec()
 * Input	: Pointer to jctl
 * Output	: Sets jctl->reg_ctl->mur_desc->jnlrec, jctl->reg_ctl->mur_desc->jreclen and jctl->rec_offset to appropriate value.
 *			May also change other globals if it changes generation.
 * Return	: SS_NORMAL on success, else error status
 * Description	: This function reads immediate next journal record from last call to this routine or mur_next()
 * 		  It can open next generation journal and update rctl->jctl and passed in "jjctl" to reflect new jctl
 * 		  It issues error message here when necessary, so caller does not need to print the error again.
 */
uint4 mur_next_rec(jnl_ctl_list **jjctl)
{
	jnl_ctl_list	*jctl;
	int		rec_size;
	uint4		status;
	mur_read_desc_t	*mur_desc;

	jctl = *jjctl;
	if (jctl->rec_offset < jctl->lvrec_off)
	{
		mur_desc = jctl->reg_ctl->mur_desc;
		assert(mur_desc->jreclen == mur_desc->jnlrec->prefix.forwptr);
		rec_size = mur_desc->jreclen;
		if (SS_NORMAL == (status = mur_next(jctl, 0)))
		{
			jctl->rec_offset += rec_size;
			assert(jctl->rec_offset <= jctl->lvrec_off);
			if (JRT_EOF != mur_desc->jnlrec->prefix.jrec_type || jctl->rec_offset == jctl->lvrec_off)
				return SS_NORMAL;
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					jctl->rec_offset, ERR_TEXT, 2, LEN_AND_LIT("Unexpected EOF record found [next_rec]"));
			status = ERR_JNLBADRECFMT;
		}
		if (ERR_JNLBADRECFMT != status)
			return status;
		if (!mur_options.update)
		{	/* only allow EXTRACT/SHOW/VERIFY to proceed after printing the error if error_limit permits */
			if (!mur_report_error(jctl, MUR_JNLBADRECFMT)) /* Issue error because mur_next_rec is called  from
								  * mur_forward(), from which errors are unexpected */
				return status;
			/* continue in distress, look for other valid records */
			return mur_valrec_next(jctl, jctl->rec_offset + rec_size);
		}
		gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len, jctl->jnl_fn,
				jctl->rec_offset, ERR_TEXT, 2, LEN_AND_LIT("Error accessing next record"));
		return status;
	}
	assert(jctl->rec_offset == jctl->lvrec_off);
	if (NULL != jctl->next_gen)
	{
		jctl = jctl->next_gen;
		jctl->reg_ctl->jctl = jctl;
		*jjctl = jctl;
		jctl->rec_offset = JNL_HDR_LEN;
		return mur_next(jctl, JNL_HDR_LEN);
	}
	return ERR_JNLREADEOF;
}

/*
 * Routine name	: mur_prev
 * Input	: jctl & dskaddr
 * Output	: Sets jctl->reg_ctl->mur_desc->jnlrec, jctl->reg_ctl->mur_desc->jreclen and jctl->rec_offset to appropriate value.
 * Return	: SS_NORMAL on success
 * Pre-Condition: To read backward , first call must be with non_zero dskaddr
 *       	  subsequent call will be with 0 as dskaddr
 * Pre-Condition: Always first call to a journal file is mur_prev(jctl, n > 0).
 *		  Then all following calls are mur_prev(jctl, 0) to read records sequentially backward.
 */
uint4 mur_prev(jnl_ctl_list *jctl, off_jnl_t dskaddr)
{
	off_jnl_t	buff_offset;
	uint4		status, partial_reclen;
	jrec_suffix	*suffix;
	mur_buff_desc_t	*swap_buff;
	boolean_t	good_suffix, good_prefix;
	mur_read_desc_t	*mur_desc;

	mur_desc = jctl->reg_ctl->mur_desc;
	if (0 != dskaddr)
	{ /* read record at dskaddr */
		assert(dskaddr < jctl->eof_addr);
		assert(dskaddr >= JNL_HDR_LEN);
		if (dskaddr >= jctl->eof_addr || dskaddr < JNL_HDR_LEN)
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					dskaddr, ERR_TEXT, 2, LEN_AND_LIT("Requested offset out of range [prev]"));
			return (dskaddr >= jctl->eof_addr ? ERR_JNLREADEOF : ERR_JNLREADBOF);
		}
		assert(dskaddr == ROUND_UP2(dskaddr, JNL_REC_START_BNDRY)); /* dskaddr must be aligned at JNL_REC_START_BNDRY */
		MUR_FREAD_CANCEL(jctl, mur_desc, status);
		if (SS_NORMAL != status)
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					dskaddr, ERR_TEXT, 2, LEN_AND_LIT("Could not cancel prior read [prev]"), jctl->status);
			return jctl->status;
		}
		mur_desc->buff_index = 1;
		mur_desc->cur_buff = &mur_desc->seq_buff[mur_desc->buff_index];
		mur_desc->sec_buff = &mur_desc->seq_buff[1 - mur_desc->buff_index];
		mur_desc->cur_buff->dskaddr = ROUND_DOWN2(dskaddr, MUR_BUFF_SIZE);
		mur_desc->cur_buff->blen = MIN(MUR_BUFF_SIZE, jctl->eof_addr - mur_desc->cur_buff->dskaddr);
		buff_offset = dskaddr - mur_desc->cur_buff->dskaddr;
		assert(JREC_PREFIX_UPTO_LEN_SIZE <= mur_desc->cur_buff->blen - buff_offset);
			/* we rely on reading at least up to the record length field (forwptr) */
		if (SS_NORMAL != (status = mur_freadw(jctl, mur_desc->cur_buff)))
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					mur_desc->cur_buff->dskaddr, ERR_TEXT, 2,
					LEN_AND_LIT("Error from synchronous read into cur_buff [prev]"), status);
			return status;
		}
		mur_desc->jnlrec = (jnl_record *)(mur_desc->cur_buff->base + buff_offset);
		good_suffix = TRUE;
		if (FALSE != (good_prefix = IS_VALID_LEN_FROM_PREFIX(mur_desc->jnlrec, jctl->jfh)))
		{
			mur_desc->jreclen = mur_desc->jnlrec->prefix.forwptr;
			if (MUR_BUFF_SIZE <= mur_desc->cur_buff->dskaddr)
			{ /* while we process the just read chunk, post a read for the immediately preceding chunk */
				mur_desc->sec_buff->dskaddr = mur_desc->cur_buff->dskaddr - MUR_BUFF_SIZE;
				MUR_FREAD_START(jctl, mur_desc->sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len,
						jctl->jnl_fn, mur_desc->sec_buff->dskaddr, ERR_TEXT, 2,
					   	LEN_AND_LIT("Could not initiate read into sec_buff in [prev] (dskaddr > 0)"),
						status);
					return status;
				}
			} else
			{ /* we read the beginning MUR_BUFF_SIZE (or smaller) chunk from file, no previous chunk exists */
				assert(0 == mur_desc->cur_buff->dskaddr);
			}
			if (buff_offset + mur_desc->jreclen > mur_desc->cur_buff->blen)
			{ /* Journal record straddles MUR_BUFF_SIZE boundary, did not read the entire record, read what's left into
			   * aux_buff2 which is located at the end of seq_buff[1], the current buffer */
				mur_desc->aux_buff2.dskaddr = mur_desc->cur_buff->dskaddr + mur_desc->cur_buff->blen;
				mur_desc->aux_buff2.blen = (buff_offset + mur_desc->jreclen - mur_desc->cur_buff->blen);
				if (FALSE != (good_prefix =
					(jctl->eof_addr - mur_desc->aux_buff2.dskaddr >= mur_desc->aux_buff2.blen)))
				{
					if (SS_NORMAL != (status = mur_freadw(jctl, &mur_desc->aux_buff2)))
					{
						gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3,
							jctl->jnl_fn_len, jctl->jnl_fn, mur_desc->aux_buff2.dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Error in synchronous read into aux_buff [prev]"), status);
						return status;
					}
				} else
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLUNXPCTERR, 3,
						jctl->jnl_fn_len, jctl->jnl_fn, dskaddr, ERR_TEXT, 2,
						LEN_AND_LIT("Requested offset beyond end of file [prev] (dskaddr > 0)"));
					return ERR_JNLBADRECFMT;
				}
			}
		} /* end good_prefix */
	} else
	{ /* dskaddr == 0, locate the previous record in the buffer, reading from disk if necessary */
		assert(JNL_HDR_LEN <= jctl->rec_offset);
		suffix = (jrec_suffix *)((char *)mur_desc->jnlrec - JREC_SUFFIX_SIZE);
		/* ok to test with possibly invalid backptr, we test for validity below */
		if (((unsigned char *)suffix > mur_desc->cur_buff->base)
			&& (((unsigned char *)mur_desc->jnlrec - suffix->backptr) >= mur_desc->cur_buff->base))
		{	/* prev record is contained completely in the current buffer */
			if ((0 == mur_desc->buff_index)
				&& ((unsigned char *)mur_desc->jnlrec + mur_desc->jreclen > mur_desc->cur_buff->top)
				&& (0 < mur_desc->cur_buff->dskaddr)) /* end of rec in sec_buff and there is data to be read */
			{ /* we just finished processing the journal record that straddled seq_buff[0] and seq_buff[1],
			   * start read in the now free secondary buffer (seq_buff[1]) to overlap with processing */
				assert(MUR_BUFF_SIZE <= mur_desc->cur_buff->dskaddr);
				assert(!mur_desc->sec_buff->read_in_progress);
				mur_desc->sec_buff->dskaddr = mur_desc->cur_buff->dskaddr - MUR_BUFF_SIZE;
				MUR_FREAD_START(jctl, mur_desc->sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len,
						jctl->jnl_fn, mur_desc->sec_buff->dskaddr, ERR_TEXT, 2,
						LEN_AND_LIT("Could not initiate read into sec_buff [prev] (dskaddr == 0)"), status);
					return status;
				}
			}
		} else
		{ /* prev record completely in sec_buff or overlaps cur_buff and sec_buff */
			if (0 == mur_desc->buff_index)
			{ /* copy partial record to just past the end of seq_buff[1], i.e., aux_seq_buff[1] to make the record
			   * available in contiguous memory */
				partial_reclen = (uint4)((unsigned char *)mur_desc->jnlrec - mur_desc->seq_buff[0].base);
				if (0 < partial_reclen)
					memcpy(mur_desc->seq_buff[1].top, mur_desc->seq_buff[0].base, partial_reclen);
				suffix = (jrec_suffix *)(mur_desc->seq_buff[1].top + partial_reclen - JREC_SUFFIX_SIZE);
			}
			/* before switching the buffers, wait for completion of pending I/O */
			if (mur_desc->sec_buff->read_in_progress)
			{
				MUR_FREAD_WAIT(jctl, mur_desc->sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len,
							jctl->jnl_fn, mur_desc->sec_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Error waiting for sec_buff read to complete [prev]"),
							status);
					return status;
				}
			}
			/* If possible, overlap I/O with processing, read into available buffer */
			if ((0 == mur_desc->buff_index || /* we just copied partial record (if any), OR */
			    (unsigned char *)mur_desc->jnlrec == mur_desc->cur_buff->base) /* we completely processed cur_buff */
				&& 0 < mur_desc->sec_buff->dskaddr) /* there is data to be read */
			{
				assert(mur_desc->sec_buff->dskaddr >= MUR_BUFF_SIZE);
				mur_desc->cur_buff->dskaddr = mur_desc->sec_buff->dskaddr - MUR_BUFF_SIZE;
				MUR_FREAD_START(jctl, mur_desc->cur_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len,
							jctl->jnl_fn, mur_desc->cur_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Could not initiate read into cur_buff [prev]"), status);
					return status;
				}
			}
			/* Make the buffer that has data that was previously read current */
			mur_desc->buff_index = 1 - mur_desc->buff_index;
			swap_buff = mur_desc->cur_buff;
			mur_desc->cur_buff = mur_desc->sec_buff;
			mur_desc->sec_buff = swap_buff;
		}
		good_prefix = TRUE;
		if (FALSE != (good_suffix = IS_VALID_LEN_FROM_SUFFIX(suffix, jctl->jfh)))
		{
			mur_desc->jnlrec = (jnl_record *)((char *)suffix + JREC_SUFFIX_SIZE - suffix->backptr);
			assert((unsigned char *)mur_desc->jnlrec >= mur_desc->cur_buff->base);
			assert((unsigned char *)mur_desc->jnlrec <  mur_desc->cur_buff->top);
			mur_desc->jreclen = suffix->backptr;
			if (jctl->rec_offset < mur_desc->jreclen + JNL_HDR_LEN)
			{
				gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, jctl->jnl_fn_len,
						jctl->jnl_fn, jctl->rec_offset, ERR_TEXT, 2,
						LEN_AND_LIT("Requested offset beyond beginning of file [prev]"));
				return ERR_JNLBADRECFMT;
			}
		}
	} /* end of dskaddr == 0 */
	if (good_prefix && good_suffix && IS_VALID_JNLREC(mur_desc->jnlrec, jctl->jfh))
		return SS_NORMAL;
	return ERR_JNLBADRECFMT;
}

/*
 * Routine name	: mur_next
 * Input	: jctl & dskaddr
 * Output	: Sets jctl->reg_ctl->mur_desc->jnlrec, jctl->reg_ctl->mur_desc->jreclen and jctl->rec_offset to appropriate value.
 * Return	: SS_NORMAL on success
 * Pre-Condition: Always first call to a journal file is mur_next(jctl, n > 0).
 *		  Then all following calls are mur_next(jctl, 0) to read records sequentially.
 */
uint4 mur_next(jnl_ctl_list *jctl, off_jnl_t dskaddr)
{
	jrec_prefix	*prefix;
	off_jnl_t	buff_offset;
	uint4		status, partial_reclen;
	mur_buff_desc_t	*swap_buff;
	boolean_t	good_prefix;
	unsigned char	*buf_top;
	mur_read_desc_t	*mur_desc;

	mur_desc = jctl->reg_ctl->mur_desc;
	if (0 != dskaddr)
	{ /* read record at dskaddr */
		assert(jctl->tail_analysis || (dskaddr >= JNL_HDR_LEN && dskaddr <= jctl->lvrec_off));
		assert(dskaddr < jctl->eof_addr);
		assert(dskaddr >= JNL_HDR_LEN);
		if (dskaddr >= jctl->eof_addr || dskaddr < JNL_HDR_LEN)
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					dskaddr, ERR_TEXT, 2, LEN_AND_LIT("Requested offset out of range [next]"));
			return (dskaddr >= jctl->eof_addr ? ERR_JNLREADEOF : ERR_JNLREADBOF);
		}
		assert(dskaddr == ROUND_UP2(dskaddr, JNL_REC_START_BNDRY)); /* dskaddr must be aligned at JNL_REC_START_BNDRY */
		MUR_FREAD_CANCEL(jctl, mur_desc, status);
		if (SS_NORMAL != status)
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					dskaddr, ERR_TEXT, 2, LEN_AND_LIT("Could not cancel prior read [next]"), jctl->status);
			return jctl->status;
		}
		mur_desc->buff_index = 0;
		mur_desc->cur_buff = &mur_desc->seq_buff[mur_desc->buff_index];
		mur_desc->sec_buff = &mur_desc->seq_buff[1 - mur_desc->buff_index];
		mur_desc->cur_buff->dskaddr = ROUND_DOWN2(dskaddr, MUR_BUFF_SIZE);
		mur_desc->cur_buff->blen = MIN(MUR_BUFF_SIZE, jctl->eof_addr - mur_desc->cur_buff->dskaddr);
		buff_offset = dskaddr - mur_desc->cur_buff->dskaddr;
		assert(JREC_PREFIX_UPTO_LEN_SIZE <= mur_desc->cur_buff->blen - buff_offset); /* we rely on reading at least up to
											     * the record length field (forwptr) */
		if (SS_NORMAL != (status = mur_freadw(jctl, mur_desc->cur_buff)))
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					mur_desc->cur_buff->dskaddr, ERR_TEXT, 2,
					LEN_AND_LIT("Error from synchronous read into cur_buff [next]"), status);
			return status;
		}
		mur_desc->jnlrec = (jnl_record *)(mur_desc->cur_buff->base + buff_offset);
		if (FALSE != (good_prefix = IS_VALID_LEN_FROM_PREFIX(mur_desc->jnlrec, jctl->jfh)))
		{
			mur_desc->jreclen = mur_desc->jnlrec->prefix.forwptr;
			if (MUR_BUFF_SIZE < jctl->eof_addr - mur_desc->cur_buff->dskaddr) /* data available to be read */
			{ /* while we process the just read chunk, post a read for the immediately succeeding chunk */
				assert(MUR_BUFF_SIZE == mur_desc->cur_buff->blen);
				mur_desc->sec_buff->dskaddr = mur_desc->cur_buff->dskaddr + MUR_BUFF_SIZE;
				MUR_FREAD_START(jctl, mur_desc->sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len,
							jctl->jnl_fn, mur_desc->sec_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Could not initiate read into sec_buff [next] (dskaddr > 0)"),
							status);
					return status;
				}
				if (((unsigned char *)mur_desc->jnlrec + mur_desc->jreclen) > mur_desc->cur_buff->top)
				{ /* Journal record straddles MUR_BUFF_SIZE boundary, did not read the record in its entirey, wait
				   * for the rest of the record to be read into seq_buff[1], the secondary buffer */
					MUR_FREAD_WAIT(jctl, mur_desc->sec_buff, status);
					if (SS_NORMAL != status)
					{
						gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3,
							jctl->jnl_fn_len, jctl->jnl_fn, mur_desc->sec_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Error waiting for sec_buff read to complete [next] "
								    "(dskaddr > 0)"),
							status);
						return status;
					}
					/* is the record available in its entirety? */
					good_prefix = (buff_offset + mur_desc->jreclen <=
							mur_desc->cur_buff->blen + mur_desc->sec_buff->blen);
				}
			} else /* we just read the last chunk, nothing more to read */
				good_prefix = (buff_offset + mur_desc->jreclen <= mur_desc->cur_buff->blen);
					/* is the record available in its entirety? */
			if (!good_prefix)
			{
				gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, jctl->jnl_fn_len,
						jctl->jnl_fn, dskaddr, ERR_TEXT, 2,
						LEN_AND_LIT("Requested offset beyond end of file [next] (dskaddr > 0)"));
				return ERR_JNLBADRECFMT;
			}
		} /* end good_prefix */
	} else
	{ /* dskaddr == 0, locate the next record in the buffer, reading from disk if necessary */
		prefix = (jrec_prefix *)((char *)mur_desc->jnlrec + mur_desc->jreclen); /* position to the next rec */
		buf_top = mur_desc->cur_buff->base + mur_desc->cur_buff->blen;
			/* we might not have read a full MUR_BUFF_SIZE chunk */
		good_prefix = ((unsigned char *)prefix + JREC_PREFIX_UPTO_LEN_SIZE < buf_top && /* ok to test with possibly 	*/
		    	       (unsigned char *)prefix + prefix->forwptr <= buf_top);		/* invalid forwptr, we test for	*/
												/*  validity below		*/
		if (good_prefix)
		{ /* next record is contained wholely in the current buffer */
			if ((1 == mur_desc->buff_index) &&
			    ((unsigned char *)mur_desc->jnlrec < mur_desc->cur_buff->base) && /* beginning of rec in sec_buff */
			    (MUR_BUFF_SIZE < jctl->eof_addr - mur_desc->cur_buff->dskaddr)) /* there is data to be read */
			{ /* we just finished processing the journal record that straddled seq_buff[0] and seq_buff[1],
			   * start read in the now free secondary buffer (seq_buff[0]) to overlap with processing */
				assert(MUR_BUFF_SIZE == mur_desc->cur_buff->blen);
				assert(!mur_desc->sec_buff->read_in_progress);
				mur_desc->sec_buff->dskaddr = mur_desc->cur_buff->dskaddr + MUR_BUFF_SIZE;
				MUR_FREAD_START(jctl, mur_desc->sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len,
							jctl->jnl_fn, mur_desc->sec_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Could not initiate read into sec_buff [next] (dskaddr == 0)"),
							status);
					return status;
				}
			}
		} else if (jctl->eof_addr - jctl->rec_offset > mur_desc->jreclen + JREC_PREFIX_UPTO_LEN_SIZE)/* within eof? */
		{ /* next record completely in sec_buff or overlaps cur_buff and sec_buff */
			if (1 == mur_desc->buff_index)
			{ /* copy partial record to just prior to the beginning of seq_buf[0], i.e., aux_buff1
			   * to make the record available in contiguous memory */
				partial_reclen = (uint4)(buf_top - (unsigned char *)prefix);
				if (0 < partial_reclen)
					memcpy(mur_desc->seq_buff[0].base - partial_reclen, (char *)prefix, partial_reclen);
				prefix = (jrec_prefix *)(mur_desc->seq_buff[0].base - partial_reclen);
			}
			/* before switching the buffers, wait for completion of pending I/O */
			if (mur_desc->sec_buff->read_in_progress)
			{
				MUR_FREAD_WAIT(jctl, mur_desc->sec_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len,
						jctl->jnl_fn, mur_desc->sec_buff->dskaddr, ERR_TEXT, 2,
						LEN_AND_LIT("Error waiting for sec_buff read to complete [next] (dskaddr == 0)"),
						status);
					return status;
				}
			}
			/* If possible, overlap I/O with processing, read into available buffer */
			if ((1 == mur_desc->buff_index || /* we just copied partial record (if any), OR */
			    (unsigned char *)prefix >= mur_desc->cur_buff->top) && /* we completely processed cur_buff */
			    MUR_BUFF_SIZE < jctl->eof_addr - MAX(mur_desc->sec_buff->dskaddr, mur_desc->cur_buff->dskaddr))
			/* there is data to be read; MAX magic is for when no read was posted to sec_buff (last chunk in file) */
			{
				mur_desc->cur_buff->dskaddr = mur_desc->sec_buff->dskaddr + MUR_BUFF_SIZE;
				MUR_FREAD_START(jctl, mur_desc->cur_buff, status);
				if (SS_NORMAL != status)
				{
					gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLREAD, 3, jctl->jnl_fn_len,
							jctl->jnl_fn, mur_desc->cur_buff->dskaddr, ERR_TEXT, 2,
							LEN_AND_LIT("Could not initiate read into cur_buff [next]"), status);
					return status;
				}
			}
			/* Make the buffer that has data that was previously read current */
			mur_desc->buff_index = 1 - mur_desc->buff_index;
			swap_buff = mur_desc->cur_buff;
			mur_desc->cur_buff = mur_desc->sec_buff;
			mur_desc->sec_buff = swap_buff;
			/* record available in its entirety? */
			good_prefix = (jctl->eof_addr - jctl->rec_offset >= mur_desc->jreclen + prefix->forwptr);
		}
		if (good_prefix)
		{
			if (FALSE != (good_prefix = IS_VALID_LEN_FROM_PREFIX((jnl_record *)prefix, jctl->jfh)))
			{
				mur_desc->jnlrec = (jnl_record *)prefix;
				mur_desc->jreclen = prefix->forwptr;
			}
		} else
		{
			if (!jctl->tail_analysis)
			{
				gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLUNXPCTERR, 3, jctl->jnl_fn_len,
					jctl->jnl_fn, jctl->rec_offset,
					ERR_TEXT, 2, LEN_AND_LIT("Requested offset beyond end of file [next] (dskaddr == 0)"));
				return ERR_JNLBADRECFMT;
			}
			return ERR_JNLREADEOF;
		}
	} /* end of dskaddr == 0 */
	if (good_prefix && IS_VALID_JNLREC(mur_desc->jnlrec, jctl->jfh))
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
	mur_buff_desc_t	*random_buff;

	random_buff = &jctl->reg_ctl->mur_desc->random_buff;
	assert(0 < random_buff->blen);
	assert(random_buff->blen <= MUR_BUFF_SIZE);
	assert(random_buff->dskaddr == ROUND_UP2(random_buff->dskaddr, DISK_BLOCK_SIZE));
	DO_FILE_READ(jctl->channel, random_buff->dskaddr, random_buff->base, random_buff->blen, jctl->status, jctl->status2);
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
uint4 mur_freadw(jnl_ctl_list *jctl, mur_buff_desc_t *buff)
{
	assert(jctl->eof_addr > buff->dskaddr); /* should never be reading at or beyond end of file */
	assert(!buff->read_in_progress);
	buff->read_in_progress = FALSE;
	DO_FILE_READ(jctl->channel, buff->dskaddr, buff->base, buff->blen, jctl->status, jctl->status2);
	return jctl->status;
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
uint4	mur_fread_eof(jnl_ctl_list *jctl, reg_ctl_list *rctl)
{
	jnl_record	*rec;
	jnl_file_header	*jfh;
	uint4		status, lvrec_off;

	jctl->reg_ctl = rctl;	/* fill in reg_ctl backpointer from jctl to rctl */
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
		if (SS_NORMAL != (status = mur_prev(jctl, lvrec_off)))
			return status;
		jctl->lvrec_off = lvrec_off;
		rec = rctl->mur_desc->jnlrec;
		jctl->eof_addr = lvrec_off + rec->prefix.forwptr;
		jctl->lvrec_time = rec->prefix.time;
		jctl->properly_closed = TRUE;
		jctl->tail_analysis = FALSE;
		return SS_NORMAL;
	}
	if (!jfh->crash && (SS_NORMAL == mur_prev(jctl, jfh->end_of_data)))
	{	/* Even though the jfh->crash field is FALSE, we still need to read the EOF record due to a window in
		 * cre_jnl_file_common between writing the file header (with crash bit set to FALSE) and writing the
		 * EOF record. If a process gets killed in such a window, we will end up with a journal file that does
		 * not have an EOF but yet has the crash bit set to FALSE. So, read the EOF anyways and if we couldn't
		 * find one, tag this journal file as NOT properly_closed
		 */
		rec = rctl->mur_desc->jnlrec;
		if (JRT_EOF == rec->prefix.jrec_type && rec->prefix.tn >= jfh->bov_tn
			&& (!mur_options.rollback || ((struct_jrec_eof *)rec)->jnl_seqno >= jfh->end_seqno))
		{ /* valid EOF */
			jctl->lvrec_off = jfh->end_of_data;
			jctl->eof_addr = jctl->lvrec_off + EOF_RECLEN;
			jctl->lvrec_time = rec->prefix.time;
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
	mur_read_desc_t	*mur_desc;

	assert(JNL_HDR_LEN <= jctl->jfh->alignsize);
	if (lo_off < jctl->jfh->end_of_data || lo_off > hi_off)
		GTMASSERT;
	jctl->properly_closed = FALSE;
	if (SS_NORMAL != (status = mur_valrec_prev(jctl, lo_off, hi_off)))
		return status;
	jctl->lvrec_off = jctl->rec_offset;
	mur_desc = jctl->reg_ctl->mur_desc;
	jctl->eof_addr = jctl->rec_offset + mur_desc->jreclen;
	jctl->lvrec_time = mur_desc->jnlrec->prefix.time;
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
 * 	"mur_fread_eof" and "mur_prev" call this function
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
	mur_read_desc_t	*mur_desc;

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
	mur_desc = jctl->reg_ctl->mur_desc;
	for (mid_off = ROUND_DOWN2(((lo_off >> 1) + (hi_off >> 1)), jfh->alignsize), jctl->rec_offset = 0; ; )
	{
		assert(lo_off <= hi_off);
		assert(mid_off < hi_off);	/* Note : It is also possible that, mid_off < lo_off */
		assert(MUR_BUFF_SIZE >= jfh->max_jrec_len);
		/* max_jrec_len size read enough to verify valid record */
		if (0 != mid_off)
		{
			blen = MIN(jfh->max_jrec_len, jctl->eof_addr - mid_off);
			mur_desc->random_buff.dskaddr = mid_off;
		} else
		{
			blen = MIN(JNL_HDR_LEN + jfh->max_jrec_len, jctl->eof_addr) - JNL_HDR_LEN;
			mur_desc->random_buff.dskaddr = JNL_HDR_LEN;
		}
		assert(MUR_BUFF_SIZE >= blen);
		mur_desc->random_buff.blen = blen;
		if (SS_NORMAL != (status = mur_read(jctl)))
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(6) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					mur_desc->random_buff.dskaddr, status);
			return status;
		}
		rec = (jnl_record *)mur_desc->random_buff.base;
		/* check the validity of the record */
		this_rec_valid = (blen > JREC_PREFIX_UPTO_LEN_SIZE && blen >= rec->prefix.forwptr && IS_VALID_JNLREC(rec, jfh));
		if (this_rec_valid)
		{
			assert(mid_off >= jctl->rec_offset || 0 == mid_off); /* are we by any chance going backwards? */
			rec_offset = jctl->rec_offset;	/* save this in case we need to restore it */
			jctl->rec_offset = MAX(mid_off, JNL_HDR_LEN);
			mur_desc->jnlrec = rec;
			mur_desc->jreclen = rec->prefix.forwptr;
			this_rec_valid = (jctl->rec_offset + mur_desc->jreclen <= hi_off) && mur_validate_checksum(jctl);
			if (!this_rec_valid)
			{	/* Initial validity checks were good but second check of this_rec_valid returned FALSE
				 * (most likely checksum did not match). So restore jctl->rec_offset to what it was before.
				 * The reason why it is set above before confirming the second validation check is that
				 * mur_validate_checksum relies on jctl->rec_offset being set for its calculations.
				 */
				jctl->rec_offset = rec_offset;
			}
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
	if (SS_NORMAL != (status = mur_next(jctl, rec_offset)))
	{
		gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn, rec_offset,
			ERR_TEXT, 2, LEN_AND_LIT("Error received from mur_valrec_prev calling mur_next(rec_offset)"));
		return status;
	}
	/* Note: though currently it is required not to change system time when journaling is on,
	 *       we do not check time continuity here. We hope that in future we will remove the restriction */
	rec_len = mur_desc->jnlrec->prefix.forwptr;
	rec_tn = mur_desc->jnlrec->prefix.tn;
	rec_seqno = 0;
	assert(rec_offset + rec_len <= hi_off);	/* We get inside for loop below at least once */
	for ( ; jctl->rec_offset + rec_len <= hi_off;
				rec_len = mur_desc->jnlrec->prefix.forwptr, rec_tn = mur_desc->jnlrec->prefix.tn)
	{
		rec_offset = jctl->rec_offset;
		if ((SS_NORMAL != mur_next(jctl, 0))
				|| ((mur_desc->jnlrec->prefix.tn != rec_tn) && (mur_desc->jnlrec->prefix.tn != (rec_tn + 1))))
			break;
		rec = mur_desc->jnlrec;
		if (mur_options.rollback && REC_HAS_TOKEN_SEQ(rec->prefix.jrec_type))
		{
			if (GET_JNL_SEQNO(rec) < rec_seqno) /* if jnl seqno is not in sequence */
				break;
			rec_seqno = GET_JNL_SEQNO(rec);
		}
		jctl->rec_offset += rec_len;	/* jctl->rec_offset is used in checksum calculation */
		if (!mur_validate_checksum(jctl))
			break;
	}
	jctl->rec_offset = rec_offset;
	/* now set mur_desc fields to point to valid record */
	assert(rec_offset);
	if (SS_NORMAL != mur_prev(jctl, rec_offset))
	{	/* Since valid record may have been overwritten we must do this.
		 * We call mur_prev, not mur_next, although both position mur_desc to the same
		 * record (when called with non zero arg) to make sure no assumptions in
		 * successive calls to mur_prev(jctl, 0) are violated.
		 */
		GTMASSERT;
	}
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
	mur_read_desc_t	*mur_desc;

	jfh = jctl->jfh;
	jctl->rec_offset = ROUND_UP2(lo_off, jfh->alignsize);
	if (jctl->rec_offset == lo_off)
		jctl->rec_offset += jfh->alignsize;
	if (jctl->rec_offset > jctl->lvrec_off)
		jctl->rec_offset = jctl->lvrec_off;
	for (; jctl->rec_offset <= jctl->lvrec_off; jctl->rec_offset = MIN(jctl->lvrec_off, jctl->rec_offset + jfh->alignsize))
	{ /* a linear search for the alignsize block beginning with valid record is sufficient since we believe the *next* valid
	   * record is located nearby */
		if (SS_NORMAL == (status = mur_prev(jctl, jctl->rec_offset)))
			break;
		if (ERR_JNLBADRECFMT != status)
			return status; /* I/O error or unexpected failure */
		if (jctl->rec_offset >= jctl->lvrec_off) /* lvrec_off must have a valid record */
			GTMASSERT;
	}
	assert(SS_NORMAL == status);
	/* now work backwards to find the earliest valid record in the immediately preceding alignsize chunk */
	mur_desc = jctl->reg_ctl->mur_desc;
	for ( ; SS_NORMAL == mur_prev(jctl, 0); jctl->rec_offset -= mur_desc->jreclen)
		;
	assert(JNL_FILE_FIRST_RECORD <= jctl->rec_offset);
	/* now set mur_desc fields to point to valid record */
	assert(jctl->rec_offset > lo_off);
	return mur_next(jctl, jctl->rec_offset); /* A conservative and safe approach since valid record may have been overwritten.
						  * We call mur_next, not mur_prev, although both position mur_desc to the same
						  * record (when called with non zero arg) to make sure no assumptions in a
						  * following call to mur_next(jctl,0) are violated. We leave the buffers in a
						  * state suitable for following mur_next(jctl,0) call. */
}

/*
 * Function name: mur_fopen
 * Input: jnl_ctl_list *
 * Return value : TRUE or False
 * This function opens the journal file , checks JNLLABEL in header, endianness etc.
 */
boolean_t mur_fopen(jnl_ctl_list *jctl)
{
	jnl_file_header	*jfh;
	char		jrecbuf[PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN];
	jnl_record	*jrec;
	int		cre_jnl_rec_size;
#	ifdef GTM_CRYPT
	int		gtmcrypt_errno;
#	endif

	if (!mur_fopen_sp(jctl))
		return FALSE;
	jctl->eof_addr = jctl->os_filesize;
	assert(NULL == jctl->jfh);
	jfh = jctl->jfh = (jnl_file_header *)malloc(REAL_JNL_HDR_LEN);
	if (REAL_JNL_HDR_LEN < jctl->os_filesize)
	{
		DO_FILE_READ(jctl->channel, 0, jfh, REAL_JNL_HDR_LEN, jctl->status, jctl->status2);
		if (SS_NORMAL != jctl->status)
			jctl->status = ERR_JNLINVALID;
	} else
		jctl->status = ERR_JNLINVALID;
	if (SS_NORMAL != jctl->status) /* read failed or size of jnl file less than minimum expected */
	{	/* error while reading journal file header implies we cannot rely on fields inside the file header
		 * so reset db file name length to 0 before printing JNLINVALID message.
		 */
		jfh->data_file_name_length = 0;
		if (ERR_JNLINVALID == jctl->status)
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLINVALID, 4, jctl->jnl_fn_len, jctl->jnl_fn,
					jfh->data_file_name_length, jfh->data_file_name, ERR_TEXT, 2,
					LEN_AND_LIT("Journal file does not have complete file header"));
		else if (SS_NORMAL != jctl->status2)
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT1(7) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn, 0,
					jctl->status, PUT_SYS_ERRNO(jctl->status2));
		else
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(6) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn, 0,
					jctl->status);
		return FALSE;
	}
	if (SS_NORMAL == jctl->status)
	{
		CHECK_JNL_FILE_IS_USABLE(jfh, jctl->status, TRUE, jctl->jnl_fn_len, jctl->jnl_fn);
		if (SS_NORMAL != jctl->status)
			return FALSE;	/* gtm_putmsg would have already been done by CHECK_JNL_FILE_IS_USABLE macro */
	}
	/* Now that we know for sure, jfh is of the format we expect it to be, we can safely access fields inside it */
	cre_jnl_rec_size = JNL_HAS_EPOCH(jfh)
			? PINI_RECLEN + EPOCH_RECLEN + PFIN_RECLEN + EOF_RECLEN
			: PINI_RECLEN + PFIN_RECLEN + EOF_RECLEN;
	if ((cre_jnl_rec_size + JNL_HDR_LEN) <= jctl->os_filesize)
	{
		DO_FILE_READ(jctl->channel, JNL_HDR_LEN, jrecbuf, cre_jnl_rec_size, jctl->status, jctl->status2);
		if (SS_NORMAL != jctl->status)
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(6) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					JNL_HDR_LEN, jctl->status);
			return FALSE;
		}
	} else
	{
		gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(10) ERR_JNLINVALID, 4, jctl->jnl_fn_len, jctl->jnl_fn,
			jfh->data_file_name_length, jfh->data_file_name,
			ERR_TEXT, 2, LEN_AND_LIT("File size is less than minimum expected for a valid journal file"));
		return FALSE;
	}
	if (!mur_options.forward && !jfh->before_images)
	{
		VMS_ONLY(assert(!mur_options.rollback_losttnonly);)
		if (mur_options.rollback_losttnonly)
		{	/* Already prepared for a LOSTTNONLY rollback. Allow NOBEFORE_IMAGE journal file but issue a warning. */
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(4) ERR_RLBKJNLNOBIMG, 2, jctl->jnl_fn_len,
					jctl->jnl_fn);
		} else
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(4) ERR_JNLNOBIJBACK, 2, jctl->jnl_fn_len,
					jctl->jnl_fn);
			return FALSE;
		}
	}
	assert(!REPL_WAS_ENABLED(jfh));	/* a journal file can never be created if replication is in WAS_ON state */
	assert(REPL_ALLOWED(jfh) == REPL_ENABLED(jfh));
	if (!REPL_ENABLED(jfh) && mur_options.rollback)
	{
		gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(4) ERR_REPLNOTON, 2, jctl->jnl_fn_len, jctl->jnl_fn);
		return FALSE;
	}
	jrec = (jnl_record *)jrecbuf;
	if (!IS_VALID_JNLREC(jrec, jfh) || JRT_PINI != jrec->prefix.jrec_type)
	{
		gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len, jctl->jnl_fn,
				JNL_HDR_LEN, ERR_TEXT, 2, LEN_AND_LIT("Invalid or no PINI record found"));
		return FALSE;
	}
	/* We have at least one good record */
	if (JNL_HAS_EPOCH(jfh))
	{
		jrec = (jnl_record *)((char *)jrec + PINI_RECLEN);
		if (!IS_VALID_JNLREC(jrec, jfh) || JRT_EPOCH != jrec->prefix.jrec_type)
		{
			gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(9) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len, jctl->jnl_fn,
					JNL_HDR_LEN + PINI_RECLEN, ERR_TEXT, 2, LEN_AND_LIT("Invalid or no EPOCH record found"));
			return FALSE;
		}
		/* We have at least one valid EPOCH */
	}
	if (mur_options.update && jfh->bov_tn > jfh->eov_tn)
	{
		gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(6) ERR_BOVTNGTEOVTN, 4, jctl->jnl_fn_len, jctl->jnl_fn,
				&jfh->bov_tn, &jfh->eov_tn);
		return FALSE;
	}
	if (jfh->bov_timestamp > jfh->eov_timestamp)
	{	/* This is not a severe error to exit, may be user changed system time which we do not allow now.
		 * But we can still try to continue recovery. We already removed time continuity check from mur_fread_eof().
		 * So if error limit allows, we will continue recovery  */
		if (!mur_report_error(jctl, MUR_BOVTMGTEOVTM))
			return FALSE;
	}
	if (mur_options.rollback && (jfh->start_seqno > jfh->end_seqno))
	{
		gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(6) ERR_BEGSEQGTENDSEQ, 4, jctl->jnl_fn_len, jctl->jnl_fn,
				&jfh->start_seqno, &jfh->end_seqno);
		return FALSE;
	}
	init_hashtab_int4(&jctl->pini_list, MUR_PINI_LIST_INIT_ELEMS, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);
	/* Please investigate if murgbl.max_extr_record_length is more than what a VMS record (in a line) can handle ??? */
	if (murgbl.max_extr_record_length < ZWR_EXP_RATIO(jctl->jfh->max_jrec_len))
		murgbl.max_extr_record_length = ZWR_EXP_RATIO(jctl->jfh->max_jrec_len);
#	ifdef GTM_CRYPT
	jctl->is_same_hash_as_db = TRUE;
	if (!process_exiting && jfh->is_encrypted)
	{
		INIT_PROC_ENCRYPTION(cs_addrs, gtmcrypt_errno);
		if (0 == gtmcrypt_errno)
			GTMCRYPT_GETKEY(cs_addrs, jfh->encryption_hash, jctl->encr_key_handle, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
			GTMCRYPT_REPORT_ERROR(MAKE_MSG_WARNING(gtmcrypt_errno), gtm_putmsg, jctl->jnl_fn_len, jctl->jnl_fn);
		if (NULL != mur_ctl->csd && (0 != memcmp(mur_ctl->csd->encryption_hash, jfh->encryption_hash, GTMCRYPT_HASH_LEN)))
			jctl->is_same_hash_as_db = FALSE;
	}
#	endif
	return TRUE;
}

boolean_t mur_fclose(jnl_ctl_list *jctl)
{
	if (NOJNL == jctl->channel) /* possible if mur_fopen() errored out */
		return TRUE;
	if (NULL != jctl->jfh)
	{
		free(jctl->jfh);
		jctl->jfh = NULL;
	}
	free_hashtab_int4(&jctl->pini_list);
	JNL_FD_CLOSE(jctl->channel, jctl->status);	/* sets jctl->channel to NOJNL */
	if (SS_NORMAL == jctl->status)
		return TRUE;
	UNIX_ONLY(jctl->status = errno;)
	assert(FALSE);
	gtm_putmsg_csa(CSA_ARG(JCTL2CSA(jctl)) VARLSTCNT(5) ERR_JNLFILECLOSERR, 2, jctl->jnl_fn_len, jctl->jnl_fn,
			jctl->status);
	return FALSE;
}
