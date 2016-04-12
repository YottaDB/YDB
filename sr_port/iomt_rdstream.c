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

#define NL 0

void iomt_rdstream (uint4 len, void *str, io_desc *dv)
{
	unsigned char  *cp, *cx;
	uint4   maxreclen;
	bool            endrec;
	d_mt_struct    *mt_ptr;

	mt_ptr = (d_mt_struct *) dv->dev_sp;
	maxreclen = (len < mt_ptr->record_sz) ? len : mt_ptr->record_sz;
	cp = cx = (unsigned char *)str;
	endrec = FALSE;

	while (!endrec)
	{
		for (; mt_ptr->buffptr < mt_ptr->bufftop && cx - cp < maxreclen;)
		{
			switch (*mt_ptr->buffptr)
			{
			case NATIVE_CR:
			case NL:
				mt_ptr->buffptr++;
				continue;
			case NATIVE_VT:
			case NATIVE_FF:
				*cx++ = *mt_ptr->buffptr;
				/* CAUTION : FALL-THROUGH  */
			case NATIVE_LF:
				mt_ptr->buffptr++;
				endrec = TRUE;
				break;
			default:
				*cx++ = *mt_ptr->buffptr++;
				continue;
			}
			break;
		}
		if (mt_ptr->buffptr >= mt_ptr->bufftop && !endrec)
		{
			iomt_readblk (dv);
			if (dv->dollar.zeof)
				endrec = TRUE;
		} else
			endrec = TRUE;
	}
	mt_ptr->rec.addr = (char *) cp;
	mt_ptr->rec.len = INTCAST(cx - cp);
	mt_ptr->last_op = mt_read;
}
