/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <rms.h>
#include "io.h"
#include "iormdef.h"
#include <devdef.h>

GBLREF io_pair		io_curr_device;
error_def(ERR_NOTTOEOFONPUT);
error_def(ERR_DEVICEREADONLY);

void iorm_write(mstr *v)
{
	unsigned char *inpt;
	io_desc *iod;
	int inlen, outlen, n;
	d_rm_struct   *rm_ptr;

	iod = io_curr_device.out;
	rm_ptr = (d_rm_struct *) iod->dev_sp;
	if (rm_ptr->f.fab$b_fac == FAB$M_GET)
		rts_error(VARLSTCNT(1) ERR_DEVICEREADONLY);

	rm_ptr->r.rab$l_ctx = FAB$M_PUT;
	if (!iod->dollar.zeof && rm_ptr->f.fab$l_dev & DEV$M_FOD
		&& !(rm_ptr->r.rab$l_rop & RAB$M_TPT))
	{
		iod->dollar.za = 9;
		rts_error(VARLSTCNT(1) ERR_NOTTOEOFONPUT);
	}
	inlen = v->len;
	outlen = iod->width - (rm_ptr->outbuf_pos - rm_ptr->outbuf);
	if ( ! iod->wrap && inlen > outlen)
		inlen = outlen;
	if (!inlen)
		return;
	for (inpt = v->addr ; ; inpt += n)
	{
		n = (inlen > outlen) ? outlen : inlen;
		memcpy(rm_ptr->outbuf_pos, inpt, n);
		iod->dollar.x += n;
		rm_ptr->outbuf_pos +=n;
		if ((inlen -= n) <= 0)
			break;
		iorm_wteol(1, iod);
		outlen = iod->width - (rm_ptr->outbuf_pos - rm_ptr->outbuf);
	}
	iod->dollar.za = 0;
	return;
}
