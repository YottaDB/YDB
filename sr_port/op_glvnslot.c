/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
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
#include <rtnhdr.h>
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

GBLREF	stack_frame	*frame_pointer;

/* Finds a slot in the glvn pool for saving a variable name. Used by SET and FOR */
uint4 op_glvnslot(uint4 recycle)
{
	glvn_pool_entry		*slot;
	uint4			indx, findx;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	GLVN_POOL_EXPAND_IF_NEEDED;
	indx = (TREF(glvn_pool_ptr))->top;
	if (GLVN_POOL_UNTOUCHED == GLVN_INDX(frame_pointer))
		/* low water mark - drain back to here when frame is unwound */
		SET_GLVN_INDX(frame_pointer, (indx) ? indx : GLVN_POOL_EMPTY);
	slot = &(TREF(glvn_pool_ptr))->slot[indx];
	slot->sav_opcode = OC_NOOP;			/* flag the slot as not filled out in case something goes wrong */
	if (ANY_SLOT != recycle)
	{	/* attempt to reuse slot (for FOR control) */
		assert((0 < recycle) && (recycle <= MAX_FOR_STACK));
		findx = (TREF(glvn_pool_ptr))->for_slot[recycle];
		if (((GLVN_INDX(frame_pointer) <= findx) || (GLVN_POOL_EMPTY == GLVN_INDX(frame_pointer))) && (findx < indx))
		{	/* reuse and pop anything beyond it */
			slot = &(TREF(glvn_pool_ptr))->slot[findx];
			(TREF(glvn_pool_ptr))->top = findx + 1;
			(TREF(glvn_pool_ptr))->mval_top = slot->mval_top;
			return findx;
		}
		/* point new slot's precursor field at indx, which corresponds an earlier frame */
		(TREF(glvn_pool_ptr))->for_slot[recycle] = indx;
		slot->precursor = (findx < indx) ? findx : GLVN_POOL_EMPTY;
	}
	slot->mval_top = (TREF(glvn_pool_ptr))->mval_top;
	return (TREF(glvn_pool_ptr))->top++;
}
