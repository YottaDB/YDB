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

#include "gtm_string.h"

#include "gtm_stdio.h"
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"
#include "copy.h"

#define EBCDIC_LITZERO '\360'
#define ASCII_LITZERO '0'

#ifdef __MVS__
#define MT_LITZERO EBCDIC_LITZERO
#else
#define MT_LITZERO ASCII_LITZERO
#endif

void iomt_wteol (int4 cnt, io_desc *dv)
{
	unsigned char  *cp;
	uint4  *quadbyteptr;
	d_mt_struct    *mt_ptr;
	error_def (ERR_MTRDONLY);
	int             i, x, y;
	static readonly unsigned char litzeros[4] = {MT_LITZERO, MT_LITZERO, MT_LITZERO, MT_LITZERO};

	mt_ptr = (d_mt_struct *) dv->dev_sp;
	if (mt_ptr->read_only)
		rts_error (VARLSTCNT (1) ERR_MTRDONLY);

#ifdef UNIX
	if (mt_ptr->mode != MT_M_WRITE)
	{
		uint4 status;
		status = iomt_reopen (dv, MT_M_WRITE, FALSE);
		if (status != SS_NORMAL)
		{
			/*return(status);*/
			return;
		}
	}
#endif

	if (mt_ptr->last_op != mt_write)
		iomt_wrtinit (dv);
	for (i = 0; i < cnt; i++)
	{
		if (mt_ptr->fixed)
		{
			x = mt_ptr->record_sz - mt_ptr->rec.len;
			if (x > 0)
				memset (mt_ptr->rec.addr + mt_ptr->rec.len, SP, x);
			mt_ptr->rec.addr += mt_ptr->record_sz;
			mt_ptr->rec.len = 0;
			if (mt_ptr->rec.addr >= (char *) mt_ptr->bufftop)
			{
				assert (mt_ptr->rec.addr == (char *) mt_ptr->bufftop);
				iomt_wrtblk (dv);
				mt_ptr->rec.addr = (char *) mt_ptr->buffer;
			}
		} else if (!mt_ptr->stream)
		{
			cp = (unsigned char *) mt_ptr->rec.addr;
			x = mt_ptr->rec.len;
			quadbyteptr = (uint4 *) (cp + x);
			x += MT_RECHDRSIZ;
			y = x / 10;
			*--cp = x - y * 10 + MT_LITZERO;
			if (x = y)
			{
				*--cp = x - (y /= 10) * 10 + MT_LITZERO;
				if (x = y)
				{
					*--cp = x - (y /= 10) * 10 + MT_LITZERO;
					if (x = y)
					{
						*--cp = x - (y / 10) * 10 + MT_LITZERO;
					}
				}
			}
			if (((unsigned char *) quadbyteptr)
			    + MT_RECHDRSIZ > mt_ptr->bufftop)
			{
				cp = (unsigned char *) (mt_ptr->rec.addr + mt_ptr->rec.len);
				x = (int)(mt_ptr->bufftop - cp);
				assert (x >= 0);
				if (x > 0)
					memset (cp, '^', x);
				iomt_wrtblk (dv);
				quadbyteptr = (uint4 *) mt_ptr->buffer;
			}
			GET_LONGP(quadbyteptr, &litzeros[0]);
			quadbyteptr++;
			mt_ptr->rec.addr = (char *) quadbyteptr;
			mt_ptr->rec.len = 0;
		} else
		{
			assert (mt_ptr->stream);
			mt_ptr->rec.len = 0;
			if (mt_ptr->buffptr >= mt_ptr->bufftop)
				iomt_vlflush (dv);

			*mt_ptr->buffptr++ = NATIVE_CR;
			if (mt_ptr->buffptr >= mt_ptr->bufftop)
				iomt_vlflush (dv);

			*mt_ptr->buffptr++ = NATIVE_LF;
			if (mt_ptr->buffptr >= mt_ptr->bufftop)
				iomt_vlflush (dv);
		}
	}
	dv->dollar.x = 0;
	dv->dollar.y += cnt;
	if (dv->length)
		dv->dollar.y %= dv->length;
	return;
}
