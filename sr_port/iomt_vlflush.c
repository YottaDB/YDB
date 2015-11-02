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

GBLREF io_pair io_curr_device;

void iomt_vlflush(io_desc *dv)
{
	int len;
	unsigned char *cp, fill_char;
	d_mt_struct   *mt_ptr;

	mt_ptr = (d_mt_struct *) dv->dev_sp;
	assert(mt_ptr->fixed == FALSE);
	if (!mt_ptr->stream)
	{
		fill_char = '^';
		len = mt_ptr->rec.len + MT_RECHDRSIZ;
		cp = (unsigned char *) ( mt_ptr->rec.addr - MT_RECHDRSIZ);
		memcpy(mt_ptr->buffer + mt_ptr->bufftoggle, cp, len);
		memset(cp, fill_char, mt_ptr->bufftop - cp);
	}
	iomt_wrtblk(dv);
	mt_ptr->buffer += mt_ptr->bufftoggle;
	mt_ptr->bufftop += mt_ptr->bufftoggle;
	mt_ptr->bufftoggle = -mt_ptr->bufftoggle;
	mt_ptr->rec.addr = (char *) mt_ptr->buffer;
	mt_ptr->buffptr = mt_ptr->buffer;
	if (!mt_ptr->stream)
		mt_ptr->rec.addr += MT_RECHDRSIZ;
	return;
}
