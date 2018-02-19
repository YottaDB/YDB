/****************************************************************
 *								*
 * Copyright (c) 2008-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iopi_iocontrol.c */

#include "mdef.h"

#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "iormdef.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "gtm_caseconv.h"
#include "min_max.h"
#include "arit.h"
#include "gtmio.h"
/* define max strings for $zkey */
#define MAX_FIXED_STRING (2 * NUM_DEC_DG_2L) + 2
#define MAX_VAR_STRING NUM_DEC_DG_2L + 1

GBLREF io_pair		io_curr_device;

error_def(ERR_INVCTLMNE);
error_def(ERR_IOERROR);

void	iopi_iocontrol(mstr *mn, int4 argcnt, va_list args)
{
	char		action[MAX_DEVCTL_LENGTH];
	d_rm_struct	*d_rm;
	int		rc;

	d_rm = (d_rm_struct *) io_curr_device.out->dev_sp;
	/* WRITE /EOF only applies to PIPE devices.  Sequential file and FIFO devices should be closed with CLOSE.*/
	if (!d_rm->is_pipe)
		return;
	/* we should not get here unless there is some string length after write / */
	assertpro((int)mn->len);
	if (0 == mn->len)
		return;
	lower_to_upper((uchar_ptr_t)&action[0], (uchar_ptr_t)mn->addr, MIN(mn->len, SIZEOF(action)));
	if (0 == memcmp(&action[0], "EOF", MIN(mn->len, SIZEOF(action))))
	{	/* Implement the write /EOF action. Close the output stream to force any blocked output to complete.
		 * Doing a write /EOF closes the output file descriptor for the pipe device but does not close the
		 * device. Since the M program could attempt this command more than once, check if the file descriptor
		 * is already closed before the actual close.
		 * Ignore the /EOF action if the device is read-only as this is a nop
		 */
		if (d_rm->read_only)
			return;
		if (FD_INVALID != d_rm->fildes)
		{	/* A new line will be inserted by iorm_cond_wteol() if $X is non-zero, just like it is done in iorm_close.c.
			 * After this call returns, * $X will be zero, which will keep iorm_readfl() from attempting an iorm_wteol()
			 * in the fixed mode after the file descriptor has been closed.
			 */
			iorm_cond_wteol(io_curr_device.out);
			IORM_FCLOSE(d_rm, fildes, filstr);
			assert(FD_INVALID == d_rm->fildes);
			assert(NULL == d_rm->filstr);
		}
	} else
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVCTLMNE);
	return;
}

void	iopi_dlr_device(mstr *d)
{
	io_desc		*iod;

	/* We will default to the input device for setting $device, since pipe uses both */
	iod = io_curr_device.in;
	PUT_DOLLAR_DEVICE_INTO_MSTR(iod, d);
	return;
}

void	iopi_dlr_key(mstr *d)
{
	io_desc		*iod;
        int		len;

	iod = io_curr_device.in;

	len = STRLEN(iod->dollar.key);
	/* verify internal buffer has enough space for $KEY string value */
	assertpro((int)d->len > len);
	if (len > 0)
		memcpy(d->addr, iod->dollar.key, MIN(len, d->len));
	d->len = len;
	return;
}

void	iopi_dlr_zkey(mstr *d)
{
	io_desc		*iod;
	int		len, save_errno;
	d_rm_struct	*d_rm;
	char		tname[MAX_FIXED_STRING];
	gtm_int64_t	record_num;	/* record offset in fixed record file */
	uint4		record_byte;	/* byte offset in fixed record block */
	boolean_t	utf_active;
	off_t		cur_position;


	iod = io_curr_device.in;
	d_rm = (d_rm_struct *)(iod->dev_sp);
	if (d_rm->fifo || d_rm->is_pipe || (2 >= d_rm->fildes))
		d->len = 0;
	else
	{
		/* if last operation was a write we need to get the current location into file_pos */
		if (RM_WRITE == d_rm->lastop)
		{
			/* need to do an lseek to get current location in file */
			cur_position = lseek(d_rm->fildes, 0, SEEK_CUR);
			if ((off_t)-1 == cur_position)
			{
				save_errno = errno;
				SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7, RTS_ERROR_LITERAL("lseek"),
					      RTS_ERROR_LITERAL("iopi_dlr_zkey()"), CALLFROM, save_errno);
			} else
				d_rm->file_pos = cur_position;
		}
		if (d_rm->fixed)
		{
			utf_active = gtm_utf8_mode ? (IS_UTF_CHSET(iod->ichset)) : FALSE;
			if (!utf_active)
			{
				/* for M mode the actual recordsize is in iod->width set by open with
				   recordsize or use with width */
				record_num = d_rm->file_pos / iod->width;
				record_byte = d_rm->file_pos % iod->width;
			} else
			{
				record_num = d_rm->file_pos / d_rm->recordsize;
				/* temporarily save bytes remaining to be read from record */
				record_byte = (int4)(d_rm->inbuf_top - d_rm->inbuf_off);
				/* if partially empty then the offset is based on recordsize */
				if (record_byte)
				{
					/* bytes read from the record is recordsize minus bytes remaining to be
					   read from record */
					record_byte = d_rm->recordsize - record_byte;
					/* decrement record_num since only partially filled */
					record_num--;
				}
			}
			SNPRINTF(tname, MAX_FIXED_STRING, "%lld,%ld", record_num, record_byte);
		} else
		{
			record_num = d_rm->file_pos;
			SNPRINTF(tname, MAX_VAR_STRING, "%lld", record_num);
		}
		len = STRLEN(tname);
		/* verify internal buffer has enough space for $ZKEY string value */
		assertpro((int)d->len > len);
		if (0 < len)
			memcpy(d->addr, tname, MIN(len,d->len));
		d->len = len;
	}
	return;
}
