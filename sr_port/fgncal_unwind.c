/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "tp_frame.h"
#include "error.h"
#include "error_trap.h"
#include "mv_stent.h"
#include "op.h"
#include "fgncal.h"
#ifdef GTM_TRIGGER
#  include "gdsroot.h"
#  include "gtm_facility.h"
#  include "fileinfo.h"
#  include "gdsbt.h"
#  include "gdsfhead.h"
#  include "gv_trigger.h"
#  include "gtm_trigger.h"
#endif
/* On UNIX, the temp_fgncal_stack threadgbl can override fgncal_stack but VMS does not have
 * this support so define the FGNCAL_STACK macro here such that they are the same.
 */
#ifdef UNIX
#  include "gtmci.h"	/* Contains FGNCAL_STACK macro */
#elif defined(VMS)
#  define FGNCAL_STACK fgncal_stack
#else
#  error "Unsupported platform"
#endif

GBLDEF unsigned char	*fgncal_stack;
GBLREF unsigned char	*stackbase, *stacktop, *stackwarn, *msp;
GBLREF mv_stent		*mv_chain;
GBLREF stack_frame	*frame_pointer;

error_def(ERR_STACKUNDERFLO);

void fgncal_unwind(void)
{
	mv_stent	*mvc;
	unsigned char	*local_fgncal_stack;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((msp <= stackbase) && (msp > stacktop));
	assert((mv_chain <= (mv_stent *)stackbase) && (mv_chain > (mv_stent *)stacktop));
	assert((frame_pointer <= (stack_frame*)stackbase) && (frame_pointer > (stack_frame *)stacktop));
	local_fgncal_stack = FGNCAL_STACK;
	while (frame_pointer && (frame_pointer < (stack_frame *)local_fgncal_stack))
	{
#		ifdef GTM_TRIGGER
		if (SFT_TRIGR & frame_pointer->type)
			gtm_trigger_fini(TRUE, FALSE);
		else
#		endif
			op_unwind();
	}
	for (mvc = mv_chain; mvc < (mv_stent *)local_fgncal_stack; mvc = (mv_stent *)(mvc->mv_st_next + (char *) mvc))
		unw_mv_ent(mvc, UNWIND_NEWVARS);
	mv_chain = mvc;
	msp = local_fgncal_stack;
	UNIX_ONLY(TREF(temp_fgncal_stack) = NULL);
	if (msp > stackbase)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_STACKUNDERFLO);
}
