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
#include "gtm_stdio.h"

#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"

void
iomt_rddoslab (io_desc *dv)
{
	static readonly unsigned char label[] = {149, 84, 248, 102, 79, 192, 1, 1, 155, 0, 0, 0, 0, 0};
	uint4   status, status1;
	iosb            io_status_blk;
	d_mt_struct    *mt_ptr;
	error_def (ERR_MTIS);
	error_def (ERR_MTDOSLAB);
	int             inlen;
	unsigned char  *incp;

	inlen = sizeof (label);
	mt_ptr = (d_mt_struct *) dv->dev_sp;
	incp = (unsigned char *) malloc (mt_ptr->block_sz);
	io_status_blk.status = 0;

#ifdef UNIX
	if (mt_ptr->mode != MT_M_READ)
	{
		status = iomt_reopen (dv, MT_M_READ, FALSE);
	}
#endif
	status = iomt_rdlblk (mt_ptr, IO_READLBLK, &io_status_blk,
			      incp, mt_ptr->block_sz);
	if (status != SS_NORMAL || (status1 = io_status_blk.status) != SS_NORMAL)
	{
		if (status1 == SS_ENDOFTAPE)
			dv->dollar.za = 1;
		else
			dv->dollar.za = 9;
		free (incp);
		rts_error (VARLSTCNT (4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
	} else
		dv->dollar.za = 0;

	if (io_status_blk.char_ct != sizeof (label) || memcmp (incp, label, sizeof (label)))
	{
		dv->dollar.za = 9;
		free (incp);
		rts_error (VARLSTCNT (6) ERR_MTDOSLAB, 0, ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
	}
	free (incp);
	return;
}
