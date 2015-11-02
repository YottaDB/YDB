/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iomt_qio.c UNIX - Stub low-level control routines for the magnetic tape
 *                   device.
 *
 *  ** For new targets, use iomt_qio.c in SR_HPUX as a template **
 */
#include "mdef.h"

#include <errno.h>
#include "gtm_stdio.h"

#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"
#include "error.h"
/**********
#include <gtm_mtio.h>
#include <unistd.h>
#include <values.h>
 *********/

GBLREF io_pair  io_curr_device;

void
iomt_qio (io_desc *iod, uint4 mask, uint4 parm)
{
	error_def (ERR_UNIMPLOP);

	rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
}
