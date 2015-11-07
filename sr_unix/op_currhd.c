/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "op.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "linktrc.h"

GBLREF stack_frame	*frame_pointer;

#ifdef AUTORELINK_SUPPORTED
/* Routine to pick up the current routine header and stash it in lnk_proxy so the next indirect
 * call picks it up. We return 0 as the index into lnk_proxy to find the routine header.
 */
int op_currhd(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TADR(lnk_proxy)->rtnhdr_adr = frame_pointer->rvector;
	DBGINDCOMP((stderr, "op_currhd: Routine reference resolved to 0x"lvaddr"\n", TADR(lnk_proxy)->rtnhdr_adr));
	return 0;
}
#endif
