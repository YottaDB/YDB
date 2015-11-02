/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iosp.h"
#include "error.h"
#include "gv_rundown.h"
#include "util.h"

GBLREF int4		exi_condition;
GBLREF boolean_t	created_core;
GBLREF boolean_t	dont_want_core;

error_def(ERR_ASSERT);
error_def(ERR_FORCEDHALT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_LKRUNDOWN);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(lastchance1)
{
	int4		actual_exi_condition;

	actual_exi_condition = exi_condition;
	PRN_ERROR;
	dec_err(VARLSTCNT(1) ERR_LKRUNDOWN);
	ESTABLISH(lastchance2);
	gv_rundown();
	REVERT;
	ESTABLISH(lastchance3);
	io_rundown(NORMAL_RUNDOWN);
	REVERT;
	if (DUMPABLE && !SUPPRESS_DUMP)
		DUMP_CORE;
	PROCDIE(actual_exi_condition);
}
