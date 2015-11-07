/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"
#include "error.h"
#include "error_trap.h"
#include "fgncal.h"
#include "gtmci.h"
#include "util.h"

GBLREF  unsigned char		*msp;
GBLREF  int                     mumps_status;
GBLREF  unsigned char		*fgncal_stack;
GBLREF  dollar_ecode_type 	dollar_ecode;
GBLREF  boolean_t		created_core;
GBLREF  boolean_t		dont_want_core;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);

CONDITION_HANDLER(gtmci_ch)
{
	char	src_buf[MAX_ENTRYREF_LEN];
	mstr	src_line;
	START_CH;
	if (DUMPABLE)
	{
		gtm_dump();
		TERMINATE;
	}
	src_line.len = 0;
	src_line.addr = &src_buf[0];
	set_zstatus(&src_line, SIGNAL, NULL, FALSE);
	TREF(in_gtmci) = FALSE;
	if (msp < fgncal_stack) /* restore stack to the last marked position */
		fgncal_unwind();
	mumps_status = SIGNAL;
	UNWIND(NULL, NULL);
}
