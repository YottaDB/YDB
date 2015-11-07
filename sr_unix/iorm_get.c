/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_fcntl.h"

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_string.h"

#include "io.h"
#include "iotimer.h"
#include "iormdef.h"
#include "stringpool.h"
#include "gt_timer.h"
#include "gtmio.h"
#include "have_crit.h"
#include "eintr_wrappers.h"
#include "wake_alarm.h"
#include "min_max.h"
#include "outofband.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#include "gtm_conv.h"
#endif

GBLREF	io_pair		io_curr_device;
GBLREF	spdesc		stringpool;
GBLREF	volatile bool	out_of_time;
GBLREF  boolean_t       gtm_utf8_mode;
GBLREF	volatile int4	outofband;
LITREF	mstr		chset_names[];

error_def(ERR_BOMMISMATCH);
error_def(ERR_IOEOF);
error_def(ERR_SYSCALL);

/*	check initial len bytes of buffer for a BOM
 *	if CHSET_UTF16, set ichset to BOM or BE if no BOM
 *	return the number of bytes to skip
 */
int	gtm_utf_bomcheck(io_desc *iod, gtm_chset_t *chset, unsigned char *buffer, int len)
{
	int	bom_bytes = 0;

	switch (*chset)
	{
		case CHSET_UTF8:
			assert(UTF8_BOM_LEN <= len);
			if (!memcmp(buffer, UTF8_BOM, UTF8_BOM_LEN))
				bom_bytes = UTF8_BOM_LEN;
			break;
		case CHSET_UTF16BE:
		case CHSET_UTF16LE:
		case CHSET_UTF16:
			assert(UTF16BE_BOM_LEN <= len);
			assert(UTF16BE_BOM_LEN == UTF16LE_BOM_LEN);
			bom_bytes = UTF16BE_BOM_LEN;
			if (!memcmp(buffer, UTF16BE_BOM, UTF16BE_BOM_LEN))
			{
				if (CHSET_UTF16LE == *chset)
				{
					iod->dollar.za = 9;
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_BOMMISMATCH, 4,
						chset_names[CHSET_UTF16BE].len, chset_names[CHSET_UTF16BE].addr,
						chset_names[CHSET_UTF16LE].len, chset_names[CHSET_UTF16LE].addr);
				}
				else if (CHSET_UTF16 == *chset)
					*chset = CHSET_UTF16BE;
			} else if (!memcmp(buffer, UTF16LE_BOM, UTF16LE_BOM_LEN))
			{
				if (CHSET_UTF16BE == *chset)
				{
					iod->dollar.za = 9;
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_BOMMISMATCH, 4,
						chset_names[CHSET_UTF16LE].len, chset_names[CHSET_UTF16LE].addr,
						chset_names[CHSET_UTF16BE].len, chset_names[CHSET_UTF16BE].addr);
				}
				else if (CHSET_UTF16 == *chset)
					*chset = CHSET_UTF16LE;
			} else if (CHSET_UTF16 == *chset)
			{	/* no BOM so set to BE and read initial bytes */
				*chset = CHSET_UTF16BE;	/* no BOM so set to BE */
				bom_bytes = 0;
			} else
				bom_bytes = 0;		/* no BOM found */
			break;
		default:
			GTMASSERT;
	}
	return bom_bytes;
}

/* When we get to this routine it is guaranteed that rm_ptr->done_1st_read is FALSE. */

int	iorm_get_bom_fol(io_desc *io_ptr, int4 *tot_bytes_read, int4 *msec_timeout, boolean_t timed, boolean_t *bom_timeout)
{
	int4		bytes2read, bytes_read, reclen, bom_bytes2read, bom_bytes_read;
	int		status = 0;
	gtm_chset_t	chset;
	d_rm_struct	*rm_ptr;
	int		fildes;
	int4 sleep_left;
	int4 sleep_time;

	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	fildes = rm_ptr->fildes;

	chset = io_ptr->ichset;
	assert(UTF16BE_BOM_LEN == UTF16LE_BOM_LEN);
	bom_bytes2read = (int4)((CHSET_UTF8 == chset) ? UTF8_BOM_LEN : UTF16BE_BOM_LEN);
	PIPE_DEBUG(PRINTF("enter iorm_get_bom_fl: bom_buf_cnt: %d bom_bytes2read: %d bom_read_one_done: %d chset: %d\n",
			  rm_ptr->bom_buf_cnt,bom_bytes2read,rm_ptr->bom_read_one_done,chset); DEBUGPIPEFLUSH;);
	/* rms-file device in follow mode */
	if (timed)
	{
		/* check iorm_get_bom_fol.... for msc_timeout */
		if (0 < *msec_timeout)
		{
			sleep_left = *msec_timeout;
		} else
			sleep_left = 0;
	}
	/* if zeof is set in follow mode then ignore any previous zeof */
	if (TRUE == io_ptr->dollar.zeof)
		io_ptr->dollar.zeof = FALSE;
	do
	{
		status = read(fildes, &rm_ptr->bom_buf[rm_ptr->bom_buf_cnt], bom_bytes2read - rm_ptr->bom_buf_cnt);
		if (0 < status) /* we read some chars */
		{
			rm_ptr->bom_buf_cnt += status;
		} else if (0 == status) /* end of file */
		{
			if ((TRUE == timed) && (0 >= sleep_left))
			{
				*bom_timeout = TRUE;
				*tot_bytes_read = rm_ptr->bom_buf_cnt;
				break;
			}
			/* if a timed read, sleep the minimum of 100 ms and sleep_left.
			   If not a timed read then just sleep 100 ms */
			if (TRUE == timed)
				sleep_time = MIN(100,sleep_left);
			else
				sleep_time = 100;
			SHORT_SLEEP(sleep_time);
			if (TRUE == timed)
				sleep_left -= sleep_time;
			if (outofband)
			{
				return 0;
			}
			continue; /* for now try and read again if eof or no input ready */
		} else		  /* error returned */
		{
			if (errno != EINTR)
				break;
		}
	} while (rm_ptr->bom_buf_cnt < bom_bytes2read);
	PIPE_DEBUG(PRINTF("iorm_get_bom_fl: status: %d, bom_buf_cnt: %d\n", status,rm_ptr->bom_buf_cnt); DEBUGPIPEFLUSH;);
	if (rm_ptr->bom_buf_cnt >= bom_bytes2read)
	{
		PIPE_DEBUG(PRINTF("iorm_get_bom_fl do bomcheck: bom_buf_cnt: %d bom_buf: %o\n",
				  rm_ptr->bom_buf_cnt,rm_ptr->bom_buf[0]); DEBUGPIPEFLUSH;);
		rm_ptr->bom_buf_off = gtm_utf_bomcheck(io_ptr, &io_ptr->ichset, rm_ptr->bom_buf, rm_ptr->bom_buf_cnt);
		rm_ptr->file_pos += rm_ptr->bom_buf_off;  /* If there is BOM bytes increment file position by bom_buf_off */
	}
	else if (CHSET_UTF16 == chset)	/* if UTF16 default to UTF16BE */
		io_ptr->ichset = CHSET_UTF16BE;
	if (chset != io_ptr->ichset)
	{	/* UTF16 changed to UTF16BE or UTF16LE */
		chset = io_ptr->ichset;
		get_chset_desc(&chset_names[chset]);
	}
	/* if outofband is not set then we are done getting the bom */
	if (!outofband)
		rm_ptr->done_1st_read = TRUE;
	return 0;
}

/* If we are in this routine then it is a fixed utf disk read with rm_ptr->follow = TRUE */

int	iorm_get_fol(io_desc *io_ptr, int4 *tot_bytes_read, int4 *msec_timeout, boolean_t timed,
		     boolean_t zint_restart, boolean_t *follow_timeout)
{
	boolean_t	ret;
	char		inchar, *temp;
	unsigned char	*pad_ptr, *nextmb, padchar, padcharray[2];
	int		fcntl_res, save_errno;
	int4		bytes2read, bytes_read, char_bytes_read, add_bytes, reclen, bytes_already_read, tmp_bytes_read;
	wint_t		utf_code;
	d_rm_struct	*rm_ptr;
	int4		status, from_bom;
	gtm_chset_t	chset;
	int		fildes;
	int		bytes_count = 0;
	int4		sleep_left;
	int4		sleep_time;
	boolean_t	bom_timeout = FALSE;

	assert (io_ptr->state == dev_open);
	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	fildes = rm_ptr->fildes;
	assert(gtm_utf8_mode ? (IS_UTF_CHSET(io_ptr->ichset)) : FALSE);
	assert(rm_ptr->fixed);
	if (!zint_restart)
	{
		bytes2read = rm_ptr->recordsize;
		bytes_already_read = 0;
		rm_ptr->inbuf_pos = rm_ptr->inbuf_top = rm_ptr->inbuf_off = rm_ptr->inbuf;
	}
	else
	{
		bytes_already_read = rm_ptr->inbuf_top - rm_ptr->inbuf;
		bytes2read = rm_ptr->recordsize - bytes_already_read;
		/* skip past if bom already read */
		if (rm_ptr->done_1st_read)
			rm_ptr->inbuf_pos = rm_ptr->inbuf_top;
		else
			rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;
	}
	PIPE_DEBUG(PRINTF("iorm_get_fol: bytes2read: %d, zint_restart: %d\n", bytes2read,zint_restart); DEBUGPIPEFLUSH;);
	bytes_read = 0;
	assert(rm_ptr->bufsize >= rm_ptr->recordsize);
	errno = status = 0;
	/* don't reset this if continuing from an interrupt unless we haven't read the bom yet */
/*	if (!rm_ptr->done_1st_read || FALSE == zint_restart)
	rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;*/
	chset = io_ptr->ichset;
	if (!rm_ptr->done_1st_read)
	{
		PIPE_DEBUG(PRINTF("do iorm_get_bom_fol: bytes2read: %d\n", bytes2read); DEBUGPIPEFLUSH;)
			status = iorm_get_bom_fol(io_ptr, tot_bytes_read, msec_timeout, timed, &bom_timeout);
		if (!rm_ptr->done_1st_read && outofband)
		{
			PIPE_DEBUG(PRINTF("return since iorm_get_bom_fol went outofband\n"); DEBUGPIPEFLUSH;);
			return 0;
		}
		if (TRUE == bom_timeout)
			*follow_timeout = TRUE;
		chset = io_ptr->ichset;	/* UTF16 will have changed to UTF16BE or UTF16LE */
	}
	assert(CHSET_UTF16 != chset);
	PIPE_DEBUG(PRINTF("iorm_get_fol: bom_buf_cnt: %d bom_buf_off: %d\n",rm_ptr->bom_buf_cnt,rm_ptr->bom_buf_off );
		   DEBUGPIPEFLUSH;);
	if (0 <= status && rm_ptr->bom_buf_cnt > rm_ptr->bom_buf_off)
	{
		PIPE_DEBUG(PRINTF("move bom: status: %d\n", status); DEBUGPIPEFLUSH;);
		from_bom = MIN((rm_ptr->bom_buf_cnt - rm_ptr->bom_buf_off), bytes2read);
		memcpy(rm_ptr->inbuf, &rm_ptr->bom_buf[rm_ptr->bom_buf_off], from_bom);
		rm_ptr->bom_buf_off += from_bom;
		bytes2read -= from_bom;		/* now in buffer */
		rm_ptr->inbuf_pos += from_bom;
		bytes_read = from_bom;
		rm_ptr->file_pos += from_bom;
		status = 0;
	}
	/* if outofband then we didn't finish so return 0 */
	if (outofband)
	{
		PIPE_DEBUG(PRINTF("iorm_get_fol: bytes2read: %d bytes_already_read: %d, zint_restart: %d\n",
				  bytes2read,bytes_already_read,zint_restart); DEBUGPIPEFLUSH;);
		return 0;
	}
	if (0 <= status && 0 < bytes2read)
	{
		PIPE_DEBUG(PRINTF("iorm_get_fol: bytes2read after bom: %d\n", bytes2read); DEBUGPIPEFLUSH;);
		if (timed)
		{
			if (0 < *msec_timeout)
			{
				sleep_left = *msec_timeout;
			} else
				sleep_left = 0;
		}
		/* if zeof is set in follow mode then ignore any previous zeof */
		if (TRUE == io_ptr->dollar.zeof)
			io_ptr->dollar.zeof = FALSE;
		temp = (char *)rm_ptr->inbuf_pos;
		do
		{
			status = read(fildes, temp, (int)bytes2read - bytes_count);
			if (0 < status) /* we read some chars */
			{
				tot_bytes_read += status;
				bytes_count += status;
				temp = temp + status;
			} else if (0 == status) /* end of file */
			{
				if ((TRUE == timed) && (0 >= sleep_left))
				{
					/* need to set tot_bytes_read and status for timeout */
					*follow_timeout = TRUE;
					break;
				}
				/* if a timed read, sleep the minimum of 100 ms and sleep_left.
				   If not a timed read then just sleep 100 ms */
				if (TRUE == timed)
					sleep_time = MIN(100,sleep_left);
				else
					sleep_time = 100;
				SHORT_SLEEP(sleep_time);
				if (TRUE == timed)
					sleep_left -= sleep_time;
				if (outofband)
					break;
				continue; /* for now try and read again if eof or no input ready */
			} else		  /* error returned */
			{
				break;
			}
		} while (bytes_count < bytes2read);
		status = bytes_count;
	}
	/* if outofband then we didn't finish so just adjust inbuf_top and inbuf_pos and return 0 */
	if (outofband)
	{
		PIPE_DEBUG(PRINTF("iorm_get_fol: outofband: bytes2read: %d status: %d tot_bytes_read: %d\n",
				  bytes2read, status, *tot_bytes_read); DEBUGPIPEFLUSH;);
		if (0 > status)
		{
			rm_ptr->inbuf_top = rm_ptr->inbuf_pos += *tot_bytes_read;
			return(0);
		}
		else
		{
			rm_ptr->inbuf_top = rm_ptr->inbuf_pos += status;
			if ((rm_ptr->inbuf_pos - rm_ptr->inbuf_off) < rm_ptr->recordsize)
				return(0);
		}
	}
	/* if some bytes were read prior to timeout then process them as if no timeout occurred */
	if (0 > status && *tot_bytes_read && ((FALSE == timed) || (TRUE == *follow_timeout)))
		status = *tot_bytes_read;
	if (0 > status)
	{
		bytes_read = 0;
		if (TRUE == *follow_timeout)
			status = -2;
	} else if (bytes_read || status)
	{
		bytes_read += status;
		rm_ptr->file_pos += status;
		padchar = rm_ptr->padchar;
		if ((CHSET_UTF16LE == chset) || (CHSET_UTF16BE == chset))
		{	/* strip 2-byte PADCHAR in UTF-16LE or UTF-16BE from tail of line */
			/* It's possible that only one byte is read if this is an interrupt restart one byte from the width
			 * In that case it's not an error if already_read is non-zero, but we have to adjust bytes_read differently.
			 */
			PIPE_DEBUG(PRINTF("pipeget: bytes_read: %d bytes_already_read: %d, zint_restart: %d\n",
					  bytes_read,bytes_already_read,zint_restart); DEBUGPIPEFLUSH;);
			if (zint_restart && bytes_already_read)
			{
				tmp_bytes_read = bytes_read + bytes_already_read;
			} else
			{
				tmp_bytes_read = bytes_read;
			}
			assert(tmp_bytes_read >= 2);
			if (CHSET_UTF16LE == chset)
			{
				padcharray[0] = padchar;
				padcharray[1] = '\0';
			} else
			{
				padcharray[0] = '\0';
				padcharray[1] = padchar;
			}
			for (pad_ptr = rm_ptr->inbuf + tmp_bytes_read - 2;
			     0 < tmp_bytes_read && rm_ptr->inbuf <= pad_ptr; pad_ptr-=2)
			{
				PIPE_DEBUG(PRINTF("pad 16 loop: bytes_read: %d pad_ptr: %sx\n",
						  bytes_read,pad_ptr); DEBUGPIPEFLUSH;);
				if ((padcharray[0] == pad_ptr[0]) && (padcharray[1] == pad_ptr[1]))
					tmp_bytes_read -= 2;
				else
					break;
			}
			bytes_read = tmp_bytes_read;
		} else
		{	/* strip 1-byte PADCHAR in UTF-8 from tail of line */
			if (zint_restart && bytes_already_read)
				bytes_read = bytes_read + bytes_already_read;
			assert(CHSET_UTF8 == chset);
			for (pad_ptr = rm_ptr->inbuf + bytes_read - 1; 0 < bytes_read && rm_ptr->inbuf <= pad_ptr; pad_ptr--)
			{
				PIPE_DEBUG(PRINTF("pad 8 loop: bytes_read: %d pad_ptr: %sx\n",
						  bytes_read,pad_ptr); DEBUGPIPEFLUSH;);
				if (*pad_ptr == padchar)
					bytes_read--;
				else
					break;
			}
		}
	}
	rm_ptr->inbuf_top = rm_ptr->inbuf_pos = rm_ptr->inbuf + bytes_read;
	rm_ptr->inbuf_off = rm_ptr->inbuf;
	return (0 <= status ? bytes_read : status);
}

/* When we get to this routine it is guaranteed that rm_ptr->done_1st_read is FALSE. */

int	iorm_get_bom(io_desc *io_ptr, int *blocked_in, boolean_t ispipe, int flags, int4 *tot_bytes_read,
		     TID timer_id, int4 *msec_timeout, boolean_t pipe_zero_timeout)
{
	int4		bytes2read, bytes_read, reclen, bom_bytes2read, bom_bytes_read;
	int		status = 0;
	gtm_chset_t	chset;
	d_rm_struct	*rm_ptr;
	int		fildes;
	boolean_t	pipe_or_fifo = FALSE;

	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	if (rm_ptr->pipe || rm_ptr->fifo)
		pipe_or_fifo = TRUE;
	/* If it is a pipe and it's the stdout returned then we need to get the read file descriptor from rm_ptr->read_fildes.
	 * Additionally, z/OS saves its FIFO read file descriptors in read_fildes, so retrieve it.
	 */
	if ((rm_ptr->pipe ZOS_ONLY(|| rm_ptr->fifo)) && (FD_INVALID != rm_ptr->read_fildes))
		fildes = rm_ptr->read_fildes;
	else
		fildes = rm_ptr->fildes;

	chset = io_ptr->ichset;
	assert(UTF16BE_BOM_LEN == UTF16LE_BOM_LEN);
	bom_bytes2read = (int4)((CHSET_UTF8 == chset) ? UTF8_BOM_LEN : UTF16BE_BOM_LEN);

	PIPE_DEBUG(PRINTF("enter iorm_get_bom: bom_buf_cnt: %d bom_bytes2read: %d bom_read_one_done: %d chset: %d\n",
			  rm_ptr->bom_buf_cnt,bom_bytes2read,rm_ptr->bom_read_one_done,chset); DEBUGPIPEFLUSH;);
	for (; rm_ptr->bom_buf_cnt < bom_bytes2read; )
	{
		PIPE_DEBUG(PRINTF("loop iorm_get_bom: bom_buf_cnt: %d\n", rm_ptr->bom_buf_cnt); DEBUGPIPEFLUSH;);
		/* Last argument is passed as FALSE(UTF_VAR_PF) since we are not doing CHUNK_SIZE read here */
		/* read the first byte only if these conditions are met.  Disk will still read bom size or eof. */

		if (pipe_or_fifo && (chset == CHSET_UTF8) && (FALSE == rm_ptr->bom_read_one_done))
		{
			DOREADRLTO2(fildes, &rm_ptr->bom_buf[rm_ptr->bom_buf_cnt], 1,
				    out_of_time, blocked_in, ispipe, flags, status, tot_bytes_read,
				    timer_id, msec_timeout, pipe_zero_timeout, FALSE, pipe_or_fifo);
			PIPE_DEBUG(PRINTF("iorm_get_bom UTF8 DOREADRLTO2: status: %d\n", status); DEBUGPIPEFLUSH;);
			/* if status is gt 0 we got one char so see if it's a bom */
			if (0 < status)
			{
				rm_ptr->bom_read_one_done = TRUE;
				/* unless there are 2 characters to follow then it can't be a utf8 bom */
				if (2 != UTF8_MBFOLLOW(&rm_ptr->bom_buf[rm_ptr->bom_buf_cnt]))
				{
					rm_ptr->bom_buf_cnt += status;
					break;
				}
			}
		} else
		{
			PIPE_DEBUG(PRINTF("DOREADRLTO2: bom_bytes2read: %d, bom_buf_cnt: %d toread: %d\n", bom_bytes2read,
					  rm_ptr->bom_buf_cnt,bom_bytes2read - rm_ptr->bom_buf_cnt); DEBUGPIPEFLUSH;);
			DOREADRLTO2(fildes, &rm_ptr->bom_buf[rm_ptr->bom_buf_cnt], bom_bytes2read - rm_ptr->bom_buf_cnt,
				    out_of_time, blocked_in, ispipe, flags, status, tot_bytes_read,
				    timer_id, msec_timeout, pipe_zero_timeout, FALSE, pipe_or_fifo);
		}
		if (0 > status)
		{
			if (errno == EINTR  &&  out_of_time)
				status = -2;
			if (pipe_or_fifo && outofband)
			{
				PIPE_DEBUG(PRINTF("iorm_get_bom: status: %d, bom_buf_cnt: %d tot_bytes_read: %d\n", status,
						  rm_ptr->bom_buf_cnt,tot_bytes_read); DEBUGPIPEFLUSH;);
				rm_ptr->bom_buf_cnt += *tot_bytes_read;
			}

			return status;
		} else
		{
			if (0 == status)
				break;
			rm_ptr->bom_buf_cnt += status;
		}
	}

	PIPE_DEBUG(PRINTF("iorm_get_bom: status: %d, bom_buf_cnt: %d\n", status,rm_ptr->bom_buf_cnt); DEBUGPIPEFLUSH;);

	if (rm_ptr->bom_buf_cnt >= bom_bytes2read)
	{
		PIPE_DEBUG(PRINTF("iorm_get_bom do bomcheck: bom_buf_cnt: %d bom_buf: %o\n",
				  rm_ptr->bom_buf_cnt,rm_ptr->bom_buf[0]); DEBUGPIPEFLUSH;);
		rm_ptr->bom_buf_off = gtm_utf_bomcheck(io_ptr, &io_ptr->ichset, rm_ptr->bom_buf, rm_ptr->bom_buf_cnt);
		rm_ptr->file_pos += rm_ptr->bom_buf_off;  /* If there is BOM bytes increment file position by bom_buf_off */
	}
	else if (CHSET_UTF16 == chset)	/* if UTF16 default to UTF16BE */
		io_ptr->ichset = CHSET_UTF16BE;
	if (chset != io_ptr->ichset)
	{	/* UTF16 changed to UTF16BE or UTF16LE */
		chset = io_ptr->ichset;
		get_chset_desc(&chset_names[chset]);
	}
	/* if outofband is not set or its a disk read then we are done with getting the bom */
	if (!(pipe_or_fifo && outofband))
		rm_ptr->done_1st_read = TRUE;
	return 0;
}

int	iorm_get(io_desc *io_ptr, int *blocked_in, boolean_t ispipe, int flags, int4 *tot_bytes_read,
		 TID timer_id, int4 *msec_timeout, boolean_t pipe_zero_timeout, boolean_t zint_restart)
{
	boolean_t	ret;
	char		inchar, *temp;
	unsigned char	*pad_ptr, *nextmb, padchar, padcharray[2];
	int		fcntl_res, save_errno;
	int4		bytes2read, bytes_read, char_bytes_read, add_bytes, reclen, bytes_already_read, tmp_bytes_read;
	wint_t		utf_code;
	d_rm_struct	*rm_ptr;
	int4		status, from_bom;
	gtm_chset_t	chset;
	int		fildes;
	boolean_t	pipe_or_fifo = FALSE;

	assert (io_ptr->state == dev_open);
	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	if (rm_ptr->pipe || rm_ptr->fifo)
		pipe_or_fifo = TRUE;
	/* If it is a pipe and it's the stdout returned then we need to get the read file descriptor from rm_ptr->read_fildes.
	 * Additionally, z/OS saves its FIFO read file descriptors in read_fildes, so retrieve it.
	 */
	if ((rm_ptr->pipe ZOS_ONLY(|| rm_ptr->fifo)) && (FD_INVALID != rm_ptr->read_fildes))
		fildes = rm_ptr->read_fildes;
	else
		fildes = rm_ptr->fildes;

	assert(gtm_utf8_mode ? (IS_UTF_CHSET(io_ptr->ichset)) : FALSE);
	assert(rm_ptr->fixed);
	if (!zint_restart)
	{
		bytes2read = rm_ptr->recordsize;
		bytes_already_read = 0;
		rm_ptr->inbuf_pos = rm_ptr->inbuf_top = rm_ptr->inbuf_off = rm_ptr->inbuf;
	}
	else
	{
		bytes_already_read = rm_ptr->inbuf_top - rm_ptr->inbuf;
		bytes2read = rm_ptr->recordsize - bytes_already_read;
		/* skip past if bom already read */
		if (rm_ptr->done_1st_read)
			rm_ptr->inbuf_pos = rm_ptr->inbuf_top;
		else
			rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;
	}
	PIPE_DEBUG(PRINTF("pipeget: bytes2read: %d, zint_restart: %d\n", bytes2read,zint_restart); DEBUGPIPEFLUSH;);
	bytes_read = 0;
	assert(rm_ptr->bufsize >= rm_ptr->recordsize);
	errno = status = 0;
	/* don't reset this if continuing from an interrupt unless we haven't read the bom yet */
/*	if (!rm_ptr->done_1st_read || FALSE == zint_restart)
	rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;*/
	chset = io_ptr->ichset;
	if (!rm_ptr->done_1st_read)
	{
		PIPE_DEBUG(PRINTF("do iorm_get_bom: bytes2read: %d\n", bytes2read); DEBUGPIPEFLUSH;)
		/* need to check for BOM *//* smw do this later perhaps or first */
		status = iorm_get_bom(io_ptr, blocked_in, ispipe, flags, tot_bytes_read,
				      timer_id, msec_timeout, pipe_zero_timeout);
		if (!rm_ptr->done_1st_read && (pipe_or_fifo && outofband))
		{
			PIPE_DEBUG(PRINTF("return since iorm_get_bom went outofband\n"); DEBUGPIPEFLUSH;);
			return 0;
		}
		chset = io_ptr->ichset;	/* UTF16 will have changed to UTF16BE or UTF16LE */
	}
	assert(CHSET_UTF16 != chset);
	PIPE_DEBUG(PRINTF("iorm_get: bom_buf_cnt: %d bom_buf_off: %d\n",rm_ptr->bom_buf_cnt,rm_ptr->bom_buf_off ); DEBUGPIPEFLUSH;);
	if (0 <= status && rm_ptr->bom_buf_cnt > rm_ptr->bom_buf_off)
	{
		PIPE_DEBUG(PRINTF("move bom: status: %d\n", status); DEBUGPIPEFLUSH;);
		from_bom = MIN((rm_ptr->bom_buf_cnt - rm_ptr->bom_buf_off), bytes2read);
		memcpy(rm_ptr->inbuf, &rm_ptr->bom_buf[rm_ptr->bom_buf_off], from_bom);
		rm_ptr->bom_buf_off += from_bom;
		bytes2read -= from_bom;		/* now in buffer */
		rm_ptr->inbuf_pos += from_bom;
		bytes_read = from_bom;
		rm_ptr->file_pos += from_bom;
		status = 0;
	}
	/* if pipe or fifo and outofband then we didn't finish so return 0 */
	if (pipe_or_fifo && outofband)
	{
		PIPE_DEBUG(PRINTF("pipeget: bytes2read: %d bytes_already_read: %d, zint_restart: %d\n",
				  bytes2read,bytes_already_read,zint_restart); DEBUGPIPEFLUSH;);
		return 0;
	}
	if (0 <= status && 0 < bytes2read)
	{
		/* If it is a pipe and at least one character is read, a timer with timer_id
		   passed in from iorm_readfl.c will be started.  It is canceled in that
		   routine if not expired. Last argument is passed as FALSE(UTF_VAR_PF) since we
		   are not doing CHUNK_SIZE read here
 		 */
		PIPE_DEBUG(PRINTF("pipeget: bytes2read after bom: %d\n", bytes2read); DEBUGPIPEFLUSH;);
		DOREADRLTO2(fildes, rm_ptr->inbuf_pos, (int)bytes2read, out_of_time, blocked_in, ispipe,
			    flags, status, tot_bytes_read, timer_id, msec_timeout, pipe_zero_timeout, FALSE, pipe_or_fifo);
	}

	/* if pipe or fifo and outofband then we didn't finish so just adjust inbuf_top and inbuf_pos and return 0 */
	if (pipe_or_fifo && outofband)
	{
		PIPE_DEBUG(PRINTF("pipeget outofband: bytes2read: %d status: %d tot_bytes_read: %d\n",
				  bytes2read, status, *tot_bytes_read); DEBUGPIPEFLUSH;);
		if (0 > status)
		{
			rm_ptr->inbuf_top = rm_ptr->inbuf_pos += *tot_bytes_read;
			return(0);
		}
		else
		{
			rm_ptr->inbuf_top = rm_ptr->inbuf_pos += status;
			if ((rm_ptr->inbuf_pos - rm_ptr->inbuf_off) < rm_ptr->recordsize)
				return(0);
		}
	}

	/* if some bytes were read prior to timeout then process them as if no timeout occurred */
	if (0 > status && *tot_bytes_read && (!*msec_timeout || (errno == EINTR && out_of_time)))
		status = *tot_bytes_read;

	if (0 > status)
	{
		bytes_read = 0;
		if (errno == EINTR  &&  out_of_time)
			status = -2;
	} else if (bytes_read || status)
	{
		bytes_read += status;
		rm_ptr->file_pos += status;
		padchar = rm_ptr->padchar;
		if ((CHSET_UTF16LE == chset) || (CHSET_UTF16BE == chset))
		{	/* strip 2-byte PADCHAR in UTF-16LE or UTF-16BE from tail of line */
			/* It's possible that only one byte is read if this is an interrupt restart one byte from the width
			 * In that case it's not an error if already_read is non-zero, but we have to adjust bytes_read differently.
			 */
			PIPE_DEBUG(PRINTF("pipeget: bytes_read: %d bytes_already_read: %d, zint_restart: %d\n",
					  bytes_read,bytes_already_read,zint_restart); DEBUGPIPEFLUSH;);
			if (zint_restart && bytes_already_read)
			{
				tmp_bytes_read = bytes_read + bytes_already_read;
			} else
			{
				tmp_bytes_read = bytes_read;
			}

			assert(tmp_bytes_read >= 2);
			if (CHSET_UTF16LE == chset)
			{
				padcharray[0] = padchar;
				padcharray[1] = '\0';
			} else
			{
				padcharray[0] = '\0';
				padcharray[1] = padchar;
			}
			for (pad_ptr = rm_ptr->inbuf + tmp_bytes_read - 2;
			     0 < tmp_bytes_read && rm_ptr->inbuf <= pad_ptr; pad_ptr-=2)
			{
				PIPE_DEBUG(PRINTF("pad 16 loop: bytes_read: %d pad_ptr: %sx\n",
						  bytes_read,pad_ptr); DEBUGPIPEFLUSH;);
				if ((padcharray[0] == pad_ptr[0]) && (padcharray[1] == pad_ptr[1]))
					tmp_bytes_read -= 2;
				else
					break;
			}
			bytes_read = tmp_bytes_read;

		} else
		{	/* strip 1-byte PADCHAR in UTF-8 from tail of line */
			if (zint_restart && bytes_already_read)
				bytes_read = bytes_read + bytes_already_read;
			assert(CHSET_UTF8 == chset);
			for (pad_ptr = rm_ptr->inbuf + bytes_read - 1; 0 < bytes_read && rm_ptr->inbuf <= pad_ptr; pad_ptr--)
			{
				PIPE_DEBUG(PRINTF("pad 8 loop: bytes_read: %d pad_ptr: %sx\n",
						  bytes_read,pad_ptr); DEBUGPIPEFLUSH;);
				if (*pad_ptr == padchar)
					bytes_read--;
				else
					break;
			}
		}
	}
	rm_ptr->inbuf_top = rm_ptr->inbuf_pos = rm_ptr->inbuf + bytes_read;
	rm_ptr->inbuf_off = rm_ptr->inbuf;
	return (0 <= status ? bytes_read : status);
}
