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
#include "io_params.h"
#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "movtc.h"

LITREF unsigned char LIB_AB_EBC_ASC[];

int iomt_readblk (io_desc *dv)
{
	int4            status;
	unsigned short  ct;
	iosb            io_status_blk;
	d_mt_struct    *mt_ptr;
	error_def (ERR_IOEOF);
	error_def (ERR_MTRDBADBLK);

	mt_ptr = (d_mt_struct *) dv->dev_sp;

#ifdef DP
	FPRINTF(stderr, ">> iomt_read_blk\n");
#endif

#ifdef UNIX
	if (mt_ptr->mode != MT_M_READ)
	{
		status = iomt_reopen (dv, MT_M_READ, FALSE);
		if (status != SS_NORMAL)
		{
			return (status);
		}
	}
#endif
	status = iomt_rdlblk (mt_ptr, mt_ptr->read_mask,
			      &io_status_blk, mt_ptr->buffer,
			      mt_ptr->block_sz);
	if (status != SS_NORMAL)
		rts_error (VARLSTCNT (1) status);
	switch (io_status_blk.status)
	{
	case SS_NORMAL:
		/******************
		*   This test is nooped ... if not, it would raise an error when the input block size did not
		*   match the declared block size.
		*		if (io_status_blk.char_ct != mt_ptr->block_sz)
		*			rts_error(VARLSTCNT(4) ERR_MTRDBADBLK,2,io_status_blk.char_ct, mt_ptr->block_sz);
		******************/
		mt_ptr->bufftop = mt_ptr->buffer + io_status_blk.char_ct;
		dv->dollar.za = 0;
		break;
	case SS_ENDOFFILE:
		if (mt_ptr->labeled == MTLAB_ANSI && !dv->dollar.zeof)
			iomt_rdansiend (dv);
		dv->dollar.zeof = TRUE;
		dv->dollar.za = 0;
		if (dv->error_handler.len > 0)
			rts_error (VARLSTCNT (1) ERR_IOEOF);
		return FALSE;
	case SS_ENDOFTAPE:
		dv->dollar.za = 1;
		rts_error (VARLSTCNT (1) io_status_blk.status);
	default:
		dv->dollar.za = 9;
		rts_error (VARLSTCNT (1) io_status_blk.status);
	}
	if (dv->dollar.zeof)
	{
		dv->dollar.zeof = FALSE;
		dv->dollar.x = 0;
		dv->dollar.y = 0;
	}
	if (mt_ptr->ebcdic)
		movtc (mt_ptr->block_sz, mt_ptr->buffer, LIB_AB_EBC_ASC, mt_ptr->buffer);
	if (!dv->dollar.za)
		mt_ptr->buffptr = mt_ptr->buffer;


#ifdef DP
	FPRINTF(stderr, "<< iomt_readblk\n");
#endif

	return TRUE;
}
