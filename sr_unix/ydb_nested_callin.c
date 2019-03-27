/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtmci.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "libyottadb_int.h"

GBLREF  unsigned char		*msp;
GBLREF  stack_frame     	*frame_pointer;
GBLREF  unsigned char		*fgncal_stack;

/* Nested call-in: setup a new CI environment (additional SFT_CI base-frame) */
void ydb_nested_callin(void)
{
	rhdtyp          	*base_addr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Temporarily mark the beginning of the new stack so that initialization errors in
	 * call-in frame do not unwind entries of the previous stack (see "gtmci_ch"). For the
	 * duration that temp_fgncal_stack has a non-NULL value, it overrides fgncal_stack.
	 */
	TREF(temp_fgncal_stack) = msp;
	/* Generate CIMAXLEVELS error if gtmci_nested_level > CALLIN_MAX_LEVEL */
	if (CALLIN_MAX_LEVEL < TREF(gtmci_nested_level))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_CIMAXLEVELS, 1, TREF(gtmci_nested_level));
	base_addr = make_dmode();
	base_frame(base_addr);			/* More fields filled in by following SET_CI_ENV macro */
	SET_CI_ENV(gtm_levl_ret_code);
	gtmci_isv_save();
	(TREF(gtmci_nested_level))++;
	/* Now that the base-frame for this call-in level has been created, we can create the mv_stent
	 * to save the previous call-in level's fgncal_stack value and clear the override. When this call-in
	 * level pops, fgncal_stack will be restored to the value for the previous level. When a given call
	 * at *this* level finishes, this current value of fgncal_stack is where the stack is unrolled to to
	 * be ready for the next call.
	 */
	SAVE_FGNCAL_STACK;
	TREF(temp_fgncal_stack) = NULL;		/* Drop override */
}
