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

/* iomt_sense.c UNIX - stub routine for systems that don't support mag tape
 *
 *    ** See iomt_sense.c in SL_HPUX for a template **
 */
#include "mdef.h"
#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"
/* ****************
#include <gtm_mtio.h>
   **************** */

uint4
iomt_sense (d_mt_struct *mt, iosb *io_status_blk)
{
	error_def (ERR_UNIMPLOP);

	rts_error(VARLSTCNT(1) ERR_UNIMPLOP);

	return 0;
}
