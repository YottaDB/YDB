/****************************************************************
 *								*
 * Copyright (c) 2012-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "lv_val.h"
#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "cache.h"
#include "op.h"
#include "rtnhdr.h"
#include "valid_mname.h"
#include "gtm_string.h"
#include "cachectl.h"
#include "gtm_text_alloc.h"
#include "callg.h"
#include "mdq.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "mv_stent.h"
#include "min_max.h"
#include "glvn_pool.h"

void op_glvnpop(uint4 indx)
{
	glvn_pool_entry		*slot;
	mval			*m, *mtop;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	slot = &(TREF(glvn_pool_ptr))->slot[indx];
	assert((TREF(glvn_pool_ptr))->mval_top >= slot->mval_top);
	m = &(TREF(glvn_pool_ptr))->mval_stack[slot->mval_top];
	mtop = &(TREF(glvn_pool_ptr))->mval_stack[(TREF(glvn_pool_ptr))->mval_top];
	for (; m < mtop; m++)
		glist_unprotect_str(&m->str);
	(TREF(glvn_pool_ptr))->top = indx;
	(TREF(glvn_pool_ptr))->mval_top = slot->mval_top;
}
