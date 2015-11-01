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

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"

void
iomt_wtdoslab (io_desc *dv)
{
	static readonly unsigned char label[] = {149, 84, 248, 102, 79, 192, 1, 1, 155, 0, 0, 0, 0, 0};
	uint4   status, status1;
	iosb            io_status_blk;
	unsigned char  *outcp;
	d_mt_struct    *mt_ptr;
	error_def (ERR_MTIS);
	error_def (ERR_MTRDONLY);
	int             inlen;

	mt_ptr = (d_mt_struct *) dv->dev_sp;
	if (mt_ptr->read_only)
		rts_error (VARLSTCNT (1) ERR_MTRDONLY);
	inlen = sizeof (label);
	outcp = (unsigned char *) malloc (inlen);
	memcpy (outcp, &label[0], inlen);
	io_status_blk.status = 0;

#ifdef UNIX
	if (mt_ptr->mode != MT_M_WRITE)
	{
		status = iomt_reopen (dv, MT_M_WRITE, FALSE);
	}
#endif
	status = iomt_wtlblk (mt_ptr->access_id, IO_WRITELBLK, &io_status_blk,
			      outcp, inlen);

	if ((status1 = io_status_blk.status) != SS_NORMAL)
	{
		if (status1 == SS_ENDOFTAPE)
		{
			dv->dollar.za = 1;
		} else
		{
			dv->dollar.za = 9;
		}
		free (outcp);
		rts_error (VARLSTCNT (4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
	} else
		dv->dollar.za = 0;

	free (outcp);
	return;
}
