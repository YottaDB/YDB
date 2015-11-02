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

#include "gtm_unistd.h"
#include "error.h"
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"
#include "io_params.h"
#include <gtm_mtio.h>

GBLREF io_pair  io_curr_device;

/* iomt_ch.c error handler for an ioctl() failure in iomt_qio() */
CONDITION_HANDLER(iomt_ch)
{
#if defined(__hpux) || defined(__sun)
	struct mtop	mtarg;
#endif
#if defined(_AIX)
	struct stop	mtarg;
#endif
	io_desc		*dv;
	d_mt_struct	*mt_ptr;
	char		closep;
	mval		close_params;
	error_def (ERR_MTIOERR);

	START_CH
	if (arg == ERR_MTIOERR)
	{
		dv = io_curr_device.in;
		mt_ptr = (d_mt_struct *)dv->dev_sp;
		/* Rewind the tape if possible */
#if defined(__hpux) || defined(__sun)
		mtarg.mt_op = MTREW;
		mtarg.mt_count = 1;
		ioctl(mt_ptr->access_id, MTIOCTOP, &mtarg);
#endif
#if defined(_AIX)
		mtarg.st_op = STREW;
		mtarg.st_count = 1;
		ioctl (mt_ptr->access_id, STIOCTOP, &mtarg);
#endif
		/* Close the tape */
		closep = iop_eol;
		close_params.mvtype = MV_STR;
		close_params.str.len = 1;
		close_params.str.addr = &closep;
       		iomt_close(dv, &close_params);
		/* Reset our position pointers */
		mt_ptr->filepos = 0;
		mt_ptr->recpos = 0;
	}
	NEXTCH;
}
