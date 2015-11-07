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

#include <jpidef.h>
#include <ssdef.h>

#include "repl_sp.h"
#include "getjobnum.h"

GBLREF	uint4	process_id;
GBLREF	uint4	image_count;

void getjobnum(void)
{
	uint4 	status;
	int4 	item_code;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	item_code = JPI$_PID;
	if (SS$_NORMAL !=(status = lib$getjpi(&item_code, 0, 0, &process_id, 0, 0)))
		rts_error(VARLSTCNT(1) status);
	get_proc_info(process_id, TADR(login_time), &image_count);
}
