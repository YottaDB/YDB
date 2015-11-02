/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_stdio.h"
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"
#include "copy.h"

int iomt_wrtinit (io_desc *dv)
{
	unsigned char  *cp;
	uint4  *quadbyteptr;
	int             x, y;
	d_mt_struct    *mt_ptr;
	static readonly unsigned char litzeros[4] = {'0', '0', '0', '0'};

	mt_ptr = (d_mt_struct *) dv->dev_sp;
#ifdef UNIX
	if (mt_ptr->mode != MT_M_WRITE)
	{
		uint4  status;
		status = iomt_reopen (dv, MT_M_WRITE, FALSE);
		if (status != SS_NORMAL)
		{
			return (status);
		}
	}
#endif

	mt_ptr->bufftop = mt_ptr->buffer + mt_ptr->block_sz;
	mt_ptr->buffptr = mt_ptr->buffer;
	if (mt_ptr->fixed || mt_ptr->stream)
	{
		mt_ptr->rec.addr = (char *) mt_ptr->buffer;
		mt_ptr->rec.len = 0;
	} else
	{
		quadbyteptr = (uint4 *) mt_ptr->buffer;
		GET_LONGP(quadbyteptr, &litzeros[0]);
		quadbyteptr++;
		mt_ptr->rec.addr = (char *) quadbyteptr;
		mt_ptr->rec.len = 0;
	}
	if (mt_ptr->labeled && mt_ptr->last_op == mt_rewind)
	{
		if (mt_ptr->labeled == MTLAB_DOS11)
			iomt_wtdoslab (dv);
		else if (mt_ptr->labeled == MTLAB_ANSI)
		{
			iomt_wtansilab (dv, MTL_VOL1 | MTL_HDR1 | MTL_HDR2);
			iomt_tm (dv);
		} else
			GTMASSERT;
	}
	dv->dollar.za = 0;
	mt_ptr->last_op = mt_write;
	return SS_NORMAL;
}
