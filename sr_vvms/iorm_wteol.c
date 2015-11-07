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
#include <rms.h>
#include <devdef.h>
#include "gtm_string.h"
#include "io.h"
#include "iormdef.h"
error_def(ERR_NOTTOEOFONPUT);
error_def(ERR_DEVICEREADONLY);

void iorm_wteol(int4 n_eol, io_desc *iod)
{
	struct RAB 	*r;
	int4 		stat;
	int 		reclen;
	int4 		i;
	d_rm_struct  	*rm_ptr;

	rm_ptr = iod->dev_sp;
	if (rm_ptr->f.fab$b_fac == FAB$M_GET)
		rts_error(VARLSTCNT(1) ERR_DEVICEREADONLY);
	r = &rm_ptr->r;
	r->rab$l_rbf = rm_ptr->outbuf;
	r->rab$l_ctx = FAB$M_PUT;
	if (!iod->dollar.zeof && rm_ptr->f.fab$l_dev & DEV$M_FOD
		&& !(rm_ptr->r.rab$l_rop & RAB$M_TPT))
	{
		iod->dollar.za = 9;
		rts_error(VARLSTCNT(1) ERR_NOTTOEOFONPUT);
	}

	for (i = 0; i < n_eol; i++)
	{
		reclen = rm_ptr->outbuf_pos - rm_ptr->outbuf;
		if (rm_ptr->b_rfm != FAB$C_FIX)
			rm_ptr->l_rsz = reclen;
		else
		{
			reclen = rm_ptr->l_mrs - reclen;
			rm_ptr->l_rsz = rm_ptr->l_mrs;
			if (reclen > 0)
				memset(rm_ptr->outbuf_pos, SP, reclen);
		}
		stat = iorm_put(iod);
		switch (stat)
		{
		case RMS$_NORMAL:
			break;
		default:
			rm_ptr->outbuf_pos = rm_ptr->outbuf;
			iod->dollar.za = 9;
			rts_error(VARLSTCNT(2) stat, r->rab$l_stv);
		}
		rm_ptr->outbuf_pos = rm_ptr->outbuf;
	}

	iod->dollar.za = 0;
	iod->dollar.x = 0;
	iod->dollar.y += n_eol;
	if (iod->length)
		iod->dollar.y %= iod->length;
	return;
}
