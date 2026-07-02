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
#include "opcode.h"
#include "glvn_pool.h"
#include "parm_pool.h"	/* for CAPACITY_ROUND_UP2 macro */

void glvn_pool_init(void)
{
	glvn_pool		*pool;
	uint4			capacity, mval_capacity, slotoff, i;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	capacity = INIT_GLVN_POOL_CAPACITY;
	mval_capacity = INIT_GLVN_POOL_MVAL_CAPACITY;
	slotoff = (uint4)OFFSETOF(glvn_pool, slot[0]);
	assert(slotoff <= SIZEOF(glvn_pool_entry)); /* The below malloc size is unusual/incorrect, but the kind of thing
						     * that might have dependencies elsewhere. Assert guards against
						     * memory corruption.
						     */
	pool = (glvn_pool *)malloc(ROUND_UP(slotoff, (capacity + 1) * SIZEOF(glvn_pool_entry)));
	pool->mval_stack = (mval *)malloc(mval_capacity * SIZEOF(mval));
	for (i = 0; i < mval_capacity; i++)
		glist_first_init_str(&pool->mval_stack[i].str);
	pool->capacity = capacity;
	pool->top = 0;
	pool->mval_capacity = mval_capacity;
	pool->mval_top = 0;
	memset(pool->for_slot, (int)GLVN_POOL_EMPTY, (MAX_FOR_STACK + 1) * SIZEOF(uint4));
	TREF(glvn_pool_ptr) = pool;
}

void glvn_pool_expand_slots(void)
{
	glvn_pool		*pool, *old_pool;
	uint4			capacity, slotoff;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	old_pool = TREF(glvn_pool_ptr);
	capacity = 2 * old_pool->capacity;
	assert(capacity <= MAX_EXPECTED_CAPACITY); 		/* Don't expect more than this in the test system */
	slotoff = (uint4)OFFSETOF(glvn_pool, slot[0]);
	assert(slotoff <= SIZEOF(glvn_pool_entry)); /* The below malloc size is unusual/incorrect, but the kind of thing
						     * that might have dependencies elsewhere. Assert guards against
						     * memory corruption.
						     */
	pool = (glvn_pool *)malloc(ROUND_UP(slotoff, (capacity + 1) * SIZEOF(glvn_pool_entry)));
	memcpy(pool, old_pool, slotoff + old_pool->top * SIZEOF(glvn_pool_entry));
	pool->capacity = capacity;
	TREF(glvn_pool_ptr) = pool;
	free(old_pool);
}

void glvn_pool_expand_mvals(void)
{
	glvn_pool		*pool;
	glvn_pool_entry		*slot, *top;
	int			i, n;
	mval			*mval_stack, *old_mval_stack;
	uint4			mval_capacity;
	INTPTR_T		shift;
	unsigned int		j;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	pool = TREF(glvn_pool_ptr);
	mval_capacity = 2 * pool->mval_capacity;
	assert(mval_capacity <= MAX_EXPECTED_MVAL_CAPACITY);	/* Don't expect more than this in the test system */
	old_mval_stack = pool->mval_stack;
	for (j = 0; j < pool->mval_top; j++)
		glist_str_before_move(&old_mval_stack[j].str);
	mval_stack = (mval *)malloc(mval_capacity * SIZEOF(mval));
	if (pool->mval_top)
		memcpy((ok_to_clobber_mstr_p)mval_stack, (ok_to_clobber_mstr_p)old_mval_stack, pool->mval_top * SIZEOF(mval));
	shift = (INTPTR_T)mval_stack - (INTPTR_T)old_mval_stack;
	for (j = 0; j < pool->mval_top; j++)
	{
		glist_str_after_move(&mval_stack[j].str);
		assert(glist_str_protected(&mval_stack[j].str));
	}
	for (j = pool->mval_top; j < mval_capacity; j++)
		glist_first_init_str(&mval_stack[j].str);
	for (slot = pool->slot, top = slot + pool->top - 1; slot < top; slot++)
	{	/* Touch up glvn_info pointers, but leave lvn start alone */
		n = slot->glvn_info.n;
		assert(n <= MAX_ACTUALS);
		if (FIRST_SAVED_ARG(slot))
		{
			slot->lvname = (mval *)(shift + (char *)slot->lvname);
			assert(glist_str_protected(&slot->lvname->str));
		}
		for (i = FIRST_SAVED_ARG(slot); i < n; i++)
			slot->glvn_info.arg[i] = (void *)(shift + (char *)slot->glvn_info.arg[i]);
	}
	pool->mval_stack = mval_stack;
	pool->mval_capacity = mval_capacity;
	free(old_mval_stack);
}
