/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_unistd.h"

#ifdef VMS
#include <lckdef.h>
#include <psldef.h>
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "error.h"
#include "filestruct.h"
#include "io.h"
#include "iosp.h"
#include "jnl.h"
#include "util.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	int4		exi_condition;
GBLREF	boolean_t	created_core;
GBLREF	boolean_t	dont_want_core;

static	const	unsigned short	zero_fid[3];

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_GVRUNDOWN);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(lastchance2)
{
	int4		actual_exi_condition;

	actual_exi_condition = (EXIT_NRM != exi_condition ? exi_condition : EXIT_ERR);
	PRN_ERROR;
	dec_err(VARLSTCNT(1) ERR_GVRUNDOWN);
	ESTABLISH(lastchance3);
	io_rundown(NORMAL_RUNDOWN);
	REVERT;
	if (DUMPABLE && !SUPPRESS_DUMP)
		DUMP_CORE;
	PROCDIE(actual_exi_condition);
}
