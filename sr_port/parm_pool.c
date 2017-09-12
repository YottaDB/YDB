/****************************************************************
 *								*
 * Copyright (c) 2012-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <stdarg.h>

#include "gtm_stdio.h"

#include "min_max.h"
#include "lv_val.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "compiler.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "stack_frame.h"
#include "parm_pool.h"
#include "fgncalsp.h"

GBLREF stack_frame	*frame_pointer;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp, *stackbase, *stackwarn, *stacktop;
GBLREF symval		*curr_symval;

LITREF mval		skiparg;

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

/*
 * The structure of the parameter pool looks like this:
 *
 *  64-bit platforms:
 *   _____________________________|_________________|_______________________|_____
 *  |     |     |     |     |     |     |     |     |     |     |     |     |
 *  |  p  |  p  |  p  |  f  | m/c |  p  |  f  | m/c |  p  |  p  |  f  | m/c | ...
 *  |_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____
 *     0     1     2     3     4  |  5     6     7  |  8     9    10    11  |
 *
 *  32-bit platforms:
 *   _________________|___________|_________________|_____
 *  |     |     |     |     |     |     |     |     |
 *  | p/p | p/f | m/c | p/f | m/c | p/p | f/- | m/c | ...
 *  |_____|_____|_____|_____|_____|_____|_____|_____|_____
 *     0     1     2  |  3     4  |  5     6     7  |
 *
 * In this example we store three sets of parameters, first having three, second having one, and
 * third having two parameters. In addition to that, each set also includes a frame field that
 * contains a pointer to the stack frame that the parameter set corresponds to, and mask_and_cnt
 * structure that holds the mask and count values for the parameters stored in the set (refer to
 * parm_pool.h). To always be able to properly populate this pool, we are maintaining a special
 * integer variable, start_idx, which indicates the offset past the mask_and_cnt slot of the
 * current parameter set.
 *
 * Because of alignment issues, on 32-bit platforms we will store two parameter pointers per slot;
 * the slot containing the last parameter will also store the frame field in case the number of
 * parameters is odd; the frame field is stored in an individual parameter otherwise. So, 4 bytes of
 * space are wasted (indicated by '-' in the illustration) on 32-bit platforms when the number of
 * parameters is even.
 *
 * If for some reason a stored parameter set is not read before a new set is stored (due to
 * out-of-band, error, or some other condition), then we can start recording new parameters at
 * start_idx because it already points to the first vacant slot. To guarantee that we will not
 * endlessly keep storing new parameters in the pool, we get rid of the last parameter set on a
 * stack frame unwind if that frame's address corresponds to the frame field of the parameter set.
 * This is achieved via PARM_ACT_UNSTACK_IF_NEEDED macro.
 *
 * For situations when the parameters are stored and subsequently read prior to unwinding the
 * respective stack frame, we increase the value of the set's actualcnt by SAFE_TO_OVWRT value,
 * which is a special *even* value that is guaranteed to be greater than MAX_ACTUALS, the maximum
 * number of parameters allowed to be passed to a label. Adding SAFE_TO_OVWRT, rather than
 * overwriting, accomplishes two purposes in our design: (1) the set is clearly marked to have been
 * used and thus disposable; (2) we can find out how many parameters the set had by subtracting
 * SAFE_TO_OVWRT from actualcnt. So, if we detect that the last parameter set stored in the pool has
 * already been used, we rewind start_idx by the number of slots occupied by that set (including
 * the ones alloted to frame and mask_and_cnt structures). Please note that the "rewind number" is
 * calculated differently on 64- and 32-bit architecture machines. Once we store a new set of
 * parameters, we advance start_idx by however many slots the new set occuppied, to point to the
 * next vacant slot.
 *
 * Although we do not expect a lot of parameters in the pool at the same time, we have a mechanism
 * of expanding the allocated storage. The initial allocation happens when the first set of
 * parameters is passed in the program. Subsequent allocations occur on if-needed basis, as decided
 * in push_parm().
 */

/* Preallocate space for at least init_capacity parameters. */
STATICFNDEF void parm_pool_init(unsigned int init_capacity)
{
	int capacity = 2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!TREF(parm_pool_ptr));
	init_capacity = MAX(init_capacity, PARM_POOL_INIT_CAP);	/* Update the minimum initial capacity if needed. */
	CAPACITY_ROUND_UP2(capacity, init_capacity);		/* Bump up the initial capacity to the first larger power of two. */
	TREF(parm_pool_ptr) = (parm_pool *)malloc(SIZEOF(parm_pool) + (SIZEOF(lv_val *) * (capacity - 1) * LV_VALS_PER_SLOT));
	(TREF(parm_pool_ptr))->capacity = capacity;
	(TREF(parm_pool_ptr))->start_idx = 0;
	(*(TREF(parm_pool_ptr))->parms).mask_and_cnt.actualcnt = SAFE_TO_OVWRT;
}



/* Expand the allocation for the parameter pool. */
STATICFNDEF void parm_pool_expand(int slots_needed, int slots_copied)
{
	parm_pool	*pool_ptr;
	uint4		pool_capacity;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TREF(parm_pool_ptr));
	assert((slots_copied < slots_needed) && (0 < slots_needed));
	pool_capacity = (TREF(parm_pool_ptr))->capacity;
	CAPACITY_ROUND_UP2(pool_capacity, slots_needed);			/* Calculate the new capacity. */
	assert(MAX_TOTAL_SLOTS > pool_capacity);				/* In debug, ensure we are not growing endlessly. */
	pool_ptr = (parm_pool *)malloc(SIZEOF(parm_pool) + SIZEOF(lv_val *) * (pool_capacity - 1) * LV_VALS_PER_SLOT);
	if (0 != slots_copied)							/* Copy only previously saved parameters. */
		memcpy(pool_ptr->parms, (TREF(parm_pool_ptr))->parms, SIZEOF(lv_val *) * slots_copied * LV_VALS_PER_SLOT);
	pool_ptr->start_idx = (TREF(parm_pool_ptr))->start_idx;			/* Restore start_idx. */
	pool_ptr->capacity = pool_capacity;					/* Update the capacity. */
	free(TREF(parm_pool_ptr));
	TREF(parm_pool_ptr) = pool_ptr;						/* Update the global reference. */
	assert(MAX_TOTAL_SLOTS > (*((TREF(parm_pool_ptr))->parms + (TREF(parm_pool_ptr))->start_idx - 1)).mask_and_cnt.actualcnt);
}

/* Generate the two forms of push_parm{_ci}() we need by including the source twice with different #defines so generate the correct
 * code for each.
 */
#define BLD_PUSH_PARM
#include "push_parm_src.h"

#undef BLD_PUSH_PARM
#define BLD_PUSH_PARM_CI
#include "push_parm_src.h"
