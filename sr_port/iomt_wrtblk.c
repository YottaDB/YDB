/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"

#ifdef UNIX
#include <errno.h>
#endif
#include "movtc.h"

LITREF unsigned char LIB_AB_ASC_EBC[];

void iomt_wrtblk (io_desc *dv)
{
	uint4		status;
	iosb		io_status_blk;
	d_mt_struct	*mt_ptr;
	error_def (ERR_MTIS);

#ifdef DP
	FPRINTF(stderr, ">> iomt_wrtblk\n");
#endif
	mt_ptr = (d_mt_struct *) dv->dev_sp;
	if (mt_ptr->ebcdic)
		movtc (mt_ptr->block_sz, mt_ptr->buffer, LIB_AB_ASC_EBC, mt_ptr->buffer);
	io_status_blk.status = 0;

#ifdef UNIX
	if (mt_ptr->mode != MT_M_WRITE)
	{
		status = iomt_reopen (dv, MT_M_WRITE, FALSE);
		if (status != SS_NORMAL)
			return;
	}
#endif
	status = iomt_wtlblk (mt_ptr->access_id, mt_ptr->write_mask, &io_status_blk,
			      mt_ptr->buffer, mt_ptr->block_sz);
	if ((status != SS_NORMAL) || ((status = io_status_blk.status) != SS_NORMAL))
	{
		if (status == SS_ENDOFTAPE)
			dv->dollar.za = 1;
		else
		{
			dv->dollar.za = 9;
#ifdef UNIX
			rts_error (VARLSTCNT (6) errno, 0, ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
#else /* VAX */
			rts_error(VARLSTCNT(4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
#endif
		}
	} else
		dv->dollar.za = 0;
#ifdef DP
	FPRINTF(stderr, "<< iomt_wrtblk\n");
#endif
	return;
}
