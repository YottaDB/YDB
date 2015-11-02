/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_fcntl.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"
#include "error.h"
#include "gtmio.h"

uint4 iomt_reopen (io_desc *dv, unsigned short mode, int rewind)
{
	uint4   	status;
	d_mt_struct	*mt_ptr;
	int		res, fpos, rpos;
	int4		mt_fileno, mt_blkno;
	int		rc;

	error_def (ERR_MTIOERR);

	mt_ptr = (d_mt_struct *) dv->dev_sp;

#ifdef DP
	FPRINTF(stderr, ">> iomt_reopen\n");

	FPRINTF(stderr, "iomt_wrtblk - \n");
	FPRINTF(stderr, "         - filepos %d\n", mt_ptr->filepos);
	FPRINTF(stderr, "         - recpos %d\n", mt_ptr->recpos);
#endif

	if (!rewind)
	{
		fpos = mt_ptr->filepos;
		rpos = mt_ptr->recpos;
	}
	CLOSEFILE_RESET(mt_ptr->access_id, rc);	/* resets "mt_ptr->access_id" to FD_INVALID */
	if (0 > rc)
	{
		PERROR("iomt_reopen");
		rts_error (VARLSTCNT (5) ERR_MTIOERR, 2, dv->name->len, dv->name->dollar_io, errno);
	}


#ifdef DP
	FPRINTF(stderr, "Rewind: %d Drive going from ", rewind);

	if (mt_ptr->mode == MT_M_WRITE)
		FPRINTF(stderr, "WRITE ");
	else
		FPRINTF(stderr, "READ ");
	if (mode == MT_M_WRITE)
		FPRINTF(stderr, "to WRITE\n");
	else
		FPRINTF(stderr, "to READ\n");
#endif

	if (mode == MT_M_WRITE)
	{
		mt_ptr->access_id = OPEN(dv->trans_name->dollar_io, O_WRONLY);
		mt_ptr->mode = MT_M_WRITE;
	} else
	{
		mt_ptr->access_id = OPEN(dv->trans_name->dollar_io, O_RDONLY | O_NDELAY);
		mt_ptr->mode = MT_M_READ;
	}
	if (mt_ptr->access_id < 0)
	{
		status = MT_TAPERROR;
		rts_error (VARLSTCNT (5) ERR_MTIOERR, 2, dv->name->len, dv->name->dollar_io, errno);
	} else
		status = SS_NORMAL;

#ifdef DP
	FPRINTF(stderr, "iomt_wrtblk - \n");
	FPRINTF(stderr, "         - filepos %d\n", mt_ptr->filepos);
	FPRINTF(stderr, "         - recpos %d\n", mt_ptr->recpos);

	FPRINTF(stderr, "<< iomt_reopen(%d)\n",mt_ptr->access_id);
#endif

	return status;
}
