/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"

void
iomt_flush (io_desc *dv)
{
	unsigned char  *cp;
	d_mt_struct    *mt_ptr;

	mt_ptr = (d_mt_struct *) dv->dev_sp;
	if (mt_ptr->last_op == mt_write)
	{
		if (mt_ptr->rec.len)
		{
			iomt_wteol (1, dv);
			assert (mt_ptr->rec.len == 0);
		}
		if (mt_ptr->stream)
			cp = mt_ptr->buffptr;
		else
			cp = (unsigned char *) mt_ptr->rec.addr;

		if (!mt_ptr->fixed && !mt_ptr->stream)
			cp -= MT_RECHDRSIZ;
		if (cp > mt_ptr->buffer)
		{
			if (cp >= mt_ptr->bufftop)
				assert (cp == mt_ptr->bufftop);
			else
				memset (cp, (mt_ptr->stream ? 0 : '^'), mt_ptr->bufftop - cp);
			iomt_wrtblk (dv);
			iomt_wrtinit (dv);
		}
	}
	return;
}
