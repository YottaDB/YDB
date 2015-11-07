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
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include <iodef.h>
#include <ssdef.h>
#include <efndef.h>


void iomt_qio(io_desc *iod, uint4 mask, uint4 parm)
{
	uint4 status, status1;
	iosb io_status_blk;
	d_mt_struct   *mt_ptr;
	error_def (ERR_MTIS);

	io_status_blk.status = 0;
	mt_ptr = (d_mt_struct *) iod->dev_sp;
	status = sys$qiow(EFN$C_ENF,mt_ptr->access_id, mask, &io_status_blk, 0,0,parm, 0,0,0,0,0);

	if ((status1 = io_status_blk.status) != SS$_NORMAL)
	{
		if (SS$_ENDOFTAPE == status1)
			iod->dollar.za = 1;
		else
		{
			iod->dollar.za = 9;
			rts_error(VARLSTCNT(4) ERR_MTIS, 2, iod->trans_name->len, iod->trans_name->dollar_io);
		}
	} else
		iod->dollar.za = 0;
	return;
}
