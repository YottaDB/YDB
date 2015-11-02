/****************************************************************
 *								*
 *	Copyright 2006, 2007 Fidelity Information Services, Inc	*
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
#include "eintr_wrappers.h"
#include "wake_alarm.h"
#include "min_max.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#include "gtm_conv.h"
#endif

GBLREF	io_pair		io_curr_device;
GBLREF	spdesc		stringpool;
GBLREF	bool		out_of_time;
GBLREF  boolean_t       gtm_utf8_mode;
LITREF	mstr		chset_names[];

/*	check initial len bytes of buffer for a BOM
 *	if CHSET_UTF16, set ichset to BOM or BE if no BOM
 *	return the number of bytes to skip
 */
int	gtm_utf_bomcheck(io_desc *iod, gtm_chset_t *chset, unsigned char *buffer, int len)
{
	int	bom_bytes = 0;
	error_def(ERR_BOMMISMATCH);

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
					rts_error(VARLSTCNT(6) ERR_BOMMISMATCH, 4,
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
					rts_error(VARLSTCNT(6) ERR_BOMMISMATCH, 4,
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

int	iorm_get_bom(io_desc *io_ptr)
{
	int4		bytes2read, bytes_read, chars_read, reclen, bom_bytes2read, bom_bytes_read;
	int		status = 0;
	gtm_chset_t	chset;
	d_rm_struct	*rm_ptr;

	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	chset = io_ptr->ichset;
	assert(UTF16BE_BOM_LEN == UTF16LE_BOM_LEN);
	bom_bytes2read = (int4)((CHSET_UTF8 == chset) ? UTF8_BOM_LEN : UTF16BE_BOM_LEN);
	for (; rm_ptr->bom_buf_cnt < bom_bytes2read; )
	{
		DOREADRLTO(rm_ptr->fildes, &rm_ptr->bom_buf[rm_ptr->bom_buf_cnt], bom_bytes2read - rm_ptr->bom_buf_cnt,
				out_of_time, status);
		if (0 > status)
		{
			rm_ptr->bom_buf_cnt = 0;
			if (errno == EINTR  &&  out_of_time)
				status = -2;
			return status;
		} else
		{
			if (0 == status)
				break;
			rm_ptr->bom_buf_cnt += status;
		}
	}
	if (rm_ptr->bom_buf_cnt >= bom_bytes2read)
		rm_ptr->bom_buf_off = gtm_utf_bomcheck(io_ptr, &io_ptr->ichset, rm_ptr->bom_buf, rm_ptr->bom_buf_cnt);
	else if (CHSET_UTF16 == chset)	/* if UTF16 default to UTF16BE */
		io_ptr->ichset = CHSET_UTF16BE;
	if (chset != io_ptr->ichset)
	{	/* UTF16 changed to UTF16BE or UTF16LE */
		chset = io_ptr->ichset;
		get_chset_desc(&chset_names[chset]);
	}
	rm_ptr->done_1st_read = TRUE;
	return 0;
}

int	iorm_get(io_desc *io_ptr)
{
	boolean_t	ret;
	char		inchar, *temp;
	unsigned char	*pad_ptr, *nextmb, padchar, padcharray[2];
	int		flags;
	int		fcntl_res, save_errno;
	int4		msec_timeout;	/* timeout in milliseconds */
	int4		bytes2read, bytes_read, char_bytes_read, add_bytes, chars_read, reclen;
	wint_t		utf_code;
	d_rm_struct	*rm_ptr;
	int4		status, from_bom;
	gtm_chset_t	chset;
	TID		timer_id;

	error_def(ERR_IOEOF);

	assert (io_ptr->state == dev_open);
	rm_ptr = (d_rm_struct *)(io_ptr->dev_sp);
	assert(gtm_utf8_mode ? (CHSET_M != io_ptr->ichset) : FALSE);
	assert(rm_ptr->fixed);
	bytes2read = rm_ptr->recordsize;
	bytes_read = chars_read = 0;
	assert(rm_ptr->bufsize >= rm_ptr->recordsize);
	errno = status = 0;
	rm_ptr->inbuf_pos = rm_ptr->inbuf_off = rm_ptr->inbuf;
	chset = io_ptr->ichset;
	assert(CHSET_M != chset);
	if (!rm_ptr->done_1st_read)
	{
		status = iorm_get_bom(io_ptr);	/* need to check for BOM */ /* smw do this later perhaps or first */
		chset = io_ptr->ichset;	/* UTF16 will have changed to UTF16BE or UTF16LE */
	}
	assert(CHSET_UTF16 != chset);
	if (0 <= status && rm_ptr->bom_buf_cnt > rm_ptr->bom_buf_off)
	{
		from_bom = MIN((rm_ptr->bom_buf_cnt - rm_ptr->bom_buf_off), bytes2read);
		memcpy(rm_ptr->inbuf, &rm_ptr->bom_buf[rm_ptr->bom_buf_off], from_bom);
		rm_ptr->bom_buf_off += from_bom;
		bytes2read -= from_bom;		/* now in buffer */
		rm_ptr->inbuf_pos += from_bom;
		bytes_read = from_bom;
		status = 0;
	}
	if (0 <= status && 0 < bytes2read)
		DOREADRLTO(rm_ptr->fildes, rm_ptr->inbuf_pos, (int)bytes2read, out_of_time, status);
	if (0 > status)
	{
		bytes_read = 0;
		if (errno == EINTR  &&  out_of_time)
			status = -2;
	} else
	{
		bytes_read += status;
		padchar = rm_ptr->padchar;
		if ((CHSET_UTF16LE == chset) || (CHSET_UTF16BE == chset))
		{	/* strip 2-byte PADCHAR in UTF-16LE or UTF-16BE from tail of line */
			assert(bytes_read >= 2);
			if (CHSET_UTF16LE == chset)
			{
				padcharray[0] = padchar;
				padcharray[1] = '\0';
			} else
			{
				padcharray[0] = '\0';
				padcharray[1] = padchar;
			}
			for (pad_ptr = rm_ptr->inbuf + bytes_read - 2; 0 < bytes_read && rm_ptr->inbuf <= pad_ptr; pad_ptr-=2)
			{
				if ((padcharray[0] == pad_ptr[0]) && (padcharray[1] == pad_ptr[1]))
					bytes_read -= 2;
				else
					break;
			}
		} else
		{	/* strip 1-byte PADCHAR in UTF-8 from tail of line */
			assert(CHSET_UTF8 == chset);
			for (pad_ptr = rm_ptr->inbuf + bytes_read - 1; 0 < bytes_read && rm_ptr->inbuf <= pad_ptr; pad_ptr--)
			{
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
