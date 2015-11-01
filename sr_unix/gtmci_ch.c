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

#include "mdef.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "error.h"
#include "fgncal.h"
#include "gtmci.h"
#include "util.h"
#include "gtmci_signals.h"
#include "invocation_mode.h"

GBLREF  stack_frame     	*frame_pointer;
GBLREF  int                     mumps_status;
GBLREF  parmblk_struct          *param_list;

CONDITION_HANDLER(gtmci_init_ch)
{
	START_CH;
	invocation_mode &= MUMPS_GTMCI_OFF;
	sig_switch_ext(); 	/* restore user signal context */
	PRN_ERROR;
	UNWIND(NULL, NULL);
}

CONDITION_HANDLER(gtmci_ch)
{
	START_CH;
	assert(frame_pointer->flags & SFF_CI);
	invocation_mode &= MUMPS_GTMCI_OFF;
	unw_mv_ent_n(param_list->retaddr ? (param_list->argcnt + 1) : param_list->argcnt);
	sig_switch_ext(); 	/* restore user signal context */
	PRN_ERROR;
	UNWIND(NULL, NULL);
}
