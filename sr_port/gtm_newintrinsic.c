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
#include "mv_stent.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "gtm_string.h"
#include "gtm_newintrinsic.h"
#include "op.h"

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF stack_frame	*frame_pointer;
GBLREF tp_frame		*tp_pointer;
GBLREF symval		*curr_symval;
GBLREF uint4		dollar_tlevel;
#ifdef GTM_TRIGGER
GBLREF mval		dollar_ztwormhole;
#endif

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

/* Note this module follows the basic pattern of op_newvar which handles the same
   function except for local vars instead of intrinsic vars. */
void gtm_newintrinsic(mval *intrinsic)
{
	mv_stent 	*mv_st_ent, *mvst_tmp, *mvst_prev;
	stack_frame	*fp, *fp_prev, *fp_fix;
	tp_frame	*tpp;
	unsigned char	*old_sp, *top;
	int		indx;
	int4		shift_size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(intrinsic);
	PUSH_MV_STENT(MVST_MSAV);
	mv_chain->mv_st_cont.mvs_msav.v = *intrinsic;
	mv_chain->mv_st_cont.mvs_msav.addr = intrinsic;
	/* Clear the intrinsic var's current value if not $ZTWORMHOLE or $ETRAP */
	if ((&(TREF(dollar_etrap)) != intrinsic) GTMTRIG_ONLY(&& (&dollar_ztwormhole != intrinsic)))
	{
		intrinsic->mvtype = MV_STR;
		intrinsic->str.len = 0;
	}
	return;
}
