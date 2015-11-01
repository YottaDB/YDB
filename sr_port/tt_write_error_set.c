/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "xfer_enum.h"
#include "outofband.h"
#include "deferred_events.h"
#include "op.h"
#include "iott_wrterr.h"


/* ------------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of TTY write error
 * Should be called only from xfer_set_handlers.
 * ------------------------------------------------------------------------
 */
GBLREF int	iott_write_error;
GBLREF int 	(* volatile xfer_table[])();

void tt_write_error_set(int4 error_status)
{
	iott_write_error = error_status;
	xfer_table[xf_linefetch] = op_fetchintrrpt;
	xfer_table[xf_linestart] = op_startintrrpt;
	xfer_table[xf_zbfetch] = op_fetchintrrpt;
	xfer_table[xf_zbstart] = op_startintrrpt;
	xfer_table[xf_forchk1] = op_startintrrpt;
	xfer_table[xf_forloop] = op_forintrrpt;
}
