/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"

GBLREF io_pair  io_curr_device;

void
iomt_write (mstr *v)
{
	error_def (ERR_MTRDONLY);
	io_desc        *io_ptr;
	unsigned char  *inpt;
	int             inlen, outlen, n;
	unsigned char  *outcp;
	d_mt_struct    *mt_ptr;

	io_ptr = io_curr_device.out;
	mt_ptr = (d_mt_struct *) io_ptr->dev_sp;
	if (mt_ptr->read_only)
		rts_error (VARLSTCNT (1) ERR_MTRDONLY);

#ifdef UNIX
	if (mt_ptr->mode != MT_M_WRITE)
	{
		uint4  status;
		status = iomt_reopen (io_ptr, MT_M_WRITE, FALSE);
	}
#endif
	if (mt_ptr->last_op != mt_write)
		iomt_wrtinit (io_ptr);
	inlen = v->len;
	outlen = io_ptr->width - mt_ptr->rec.len;
	if (!mt_ptr->wrap && inlen > outlen)
		inlen = outlen;
	if (!inlen)
		return;
	for (inpt = (unsigned char *) v->addr;; inpt += n)
	{
		if (mt_ptr->stream)
		{
			outcp = mt_ptr->buffptr;
			n = (outcp + inlen > mt_ptr->bufftop) ?
				(int)(mt_ptr->bufftop - outcp) : inlen;
			if (n <= 0)
			{
				iomt_vlflush (io_ptr);
				outcp = (unsigned char *) mt_ptr->rec.addr;
			}
		} else
		{
			outcp = (unsigned char *) mt_ptr->rec.addr + mt_ptr->rec.len;
			n = (inlen > outlen) ? outlen : inlen;
			if (mt_ptr->fixed)
				assert (outcp + n <= mt_ptr->bufftop);
			else
			{
				if (outcp + n >= mt_ptr->bufftop)
				{
					iomt_vlflush (io_ptr);
					outcp = (unsigned char *) (mt_ptr->rec.addr + mt_ptr->rec.len);
				}
			}
		}
		memcpy (outcp, inpt, n);
		mt_ptr->buffptr = outcp + n;
		mt_ptr->rec.len += n;
		io_ptr->dollar.x += n;
		if ((inlen -= n) <= 0)
			break;
		if (!mt_ptr->stream)
		{
			iomt_wteol (1, io_ptr);
			outlen = io_ptr->width - mt_ptr->rec.len;
		}
	}
	if (mt_ptr->stream && io_ptr->dollar.x >= io_ptr->width && io_ptr->wrap)
	{
		io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
		if (io_ptr->length)
			io_ptr->dollar.y %= io_ptr->length;
		io_ptr->dollar.x %= io_ptr->width;
	}

	return;
}
