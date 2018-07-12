/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> df1555e... GT.M V6.3-005
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
	mstr	entryref;

	START_CH(TRUE);
	if (DUMPABLE)
	{
		gtm_dump();
		TERMINATE;
	}
<<<<<<< HEAD
	entryref.addr = CALL_IN_M_ENTRYREF;
	entryref.len = STR_LIT_LEN(CALL_IN_M_ENTRYREF);
	set_zstatus(&entryref, SIGNAL, NULL, FALSE);
	FGNCAL_UNWIND_CLEANUP;
=======
	src_line.len = 0;
	src_line.addr = &src_buf[0];
	set_zstatus(&src_line, SIGNAL, NULL, FALSE);
	if (msp < FGNCAL_STACK) /* restore stack to the last marked position */
		fgncal_unwind();
	else TREF(temp_fgncal_stack) = NULL;	/* If fgncal_unwind() didn't run to clear this, we have to */
	if (TREF(comm_filter_init))
		TREF(comm_filter_init) = FALSE;  /* Exiting from filters */
>>>>>>> df1555e... GT.M V6.3-005
	mumps_status = SIGNAL;
	UNWIND(NULL, NULL);
}
