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
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"

GBLREF io_pair  io_curr_device;

void iomt_getrec (io_desc *dv)
{
#ifdef __MVS__
#pragma convlit(suspend)
#endif
	error_def (ERR_MTRDTHENWRT);
	error_def (ERR_MTRECTOOBIG);
	error_def (ERR_MTRECTOOSM);
	unsigned char  *cp, *cx, fill_char;
	d_mt_struct    *mt_ptr;
	int             x, ln;

	mt_ptr = (d_mt_struct *) dv->dev_sp;
	cp = mt_ptr->buffptr;
	if (!mt_ptr->stream)
		fill_char = '^';
	ln = (int)(mt_ptr->bufftop - cp);
	if (ln < 0)
		rts_error (VARLSTCNT (1) ERR_MTRECTOOBIG);
	else if (ln == 0)
	{
		iomt_readblk (dv);
		cp = mt_ptr->buffer;
		ln = (int)(mt_ptr->bufftop - cp);
	} else if (*cp == fill_char)
	{
		if (!mt_ptr->fixed || !skpc (fill_char, ln, (char *)cp))
		{
			iomt_readblk (dv);
			cp = mt_ptr->buffer;
			ln = (int)(mt_ptr->bufftop - cp);
		}
	}
	if (mt_ptr->fixed)
	{
		mt_ptr->rec.len = (ln < mt_ptr->record_sz) ? ln : mt_ptr->record_sz;
	} else if (!mt_ptr->stream)
	{
		x = *cp++ - '0';
		x = x * 10 + (*cp++ - '0');
		x = x * 10 + (*cp++ - '0');
		x = x * 10 + (*cp++ - '0') - MT_RECHDRSIZ;
		if (x < 0)
			rts_error (VARLSTCNT (1) ERR_MTRECTOOSM);
		if (x > mt_ptr->record_sz || x > ln)
			rts_error (VARLSTCNT (1) ERR_MTRECTOOBIG);
		mt_ptr->rec.len = x;
	}
	mt_ptr->rec.addr = (char *) cp;
	mt_ptr->buffptr = (unsigned char *) (mt_ptr->rec.addr + mt_ptr->rec.len);
	mt_ptr->last_op = mt_read;
	return;
#ifdef __MVS__
#pragma convlit(resume)
#endif
}
