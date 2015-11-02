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

/* iomt_info.c UNIX Stub for platforms that do not support mag tape devices
 *
 *  ** see iomt_info.c in SL_HPUX for a template **
 */
#include "mdef.h"
#include "gtm_stdio.h"
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "iosp.h"
#include "error.h"
/* *****************
#include <gtm_mtio.h>
#include "gtm_stat.h"
#include <gtm_termios.h>
#include <errno.h>
   **************** */

int iomt_info(d_mt_struct *mt)
{
	error_def (ERR_UNIMPLOP);

	rts_error(VARLSTCNT(1) ERR_UNIMPLOP);

	return 0;
}
