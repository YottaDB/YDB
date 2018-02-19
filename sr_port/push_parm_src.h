/****************************************************************
 *								*
 * Copyright (c) 2001-2013 Fidelity National Information	*
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

/* The actual push_parm routine needs two variations - one that takes arguments directly from the registers and stack as per
 * normal when it is called from the assembler glue routines when one M routine calls another (op_exfun, op_extexfun, and their
 * various mprof flavors) and one that takes its arguments from a passed in vector created by the call-in interface. To keep from
 * replicating the bulk of the ccode in these routines, the routine is macro-ized here so it can be built in both flavors without
 * duplicating code everywhere. The routines are basically the same - only where they get their input actually differs.
 *
 * To generate push_parm(), which takes its parameters in a varargs parameter list, #define BLD_PUSH_PARM. To generate the
 * push_parm_ci() routine, which takes its parameter from a passed in parmblk_struct, #define BLD_PUSH_PARM_CI. If one and only
 * one of these is not set an error is generated.
 */

/* Routine to push lv_val parameters into our parameter pool, taking proper care of unread parameters. An important note is that
 * op_bindparm() should increase actualcnt of the read set of parameters by a value of SAFE_TO_OVWRT, to
 * indicate that it is OK to overwrite those parameters, since they have already been read and bound.
 *
 * Routine push_parm(int totalcnt, bool truth_value, *return_mval, int mask, int actual_cnt, mval *parm...)
 *
 *   - Generally called from the M->M call glue code (op_extexfun, op_exfun, and their mprof variants) but is also called from
 *     jobchild_init() for passing parms to initial instances.
 *   - Parameters:
 *     - totalcnt  	- Total count of all parameters.
 *     - truth value	- The current truth value to save and then restore when routine unwinds.
 *     - return_mval    - Address where the return value for the call is to be put
 *     - mask		- A bit mask (1 bit each argument). If a bit is 1 the associated value is pass by reference.
 *     - actual_cnt	- The number of parameters specified.
 *     - parm		- Comma separated list of actual_cnt parameters (mval addresses) which can be 0 if actual_cnt is 0.
 *
 * Routine push_parm_ci(truth_value, parmblk_struct *ci_parms)
 *
 *   - Generally called from ydb_ci[p]() and from ydb_cij() to process parameters for a call-in to an M routine.
 *   - Parameters:
 *     - truth_value    - The current truth value to save and then restore when routine unwinds.
 *     - ci_parms       - Block that contains the return_mval address, the mask, the arg count and the parms.
 *
 * Terminology:
 * I	- Input parameter only - not changed by callee.
 * O    - Output parameter only - callee sets this but has no initial value.
 * IO   - Both an initial value that can be set/reset by the callee.
 */
#ifdef BLD_PUSH_PARM
#  ifdef BLD_PUSH_PARM_CI
#     error "Both BLD_PUSH_PARM and BLD_PUSH_PARM_CI are defined - only one can be defined at a time"
#  endif
void push_parm(unsigned int totalcnt, int truth_value, ...)
#endif
#ifdef BLD_PUSH_PARM_CI
#  ifdef BLD_PUSH_PARM
#     error "Both BLD_PUSH_PARM and BLD_PUSH_PARM_CI are defined - only one can be defined at a time"
#  endif
void push_parm_ci(int truth_value, parmblk_struct *ci_parms)
#endif
#if !defined(BLD_PUSH_PARM) && !defined(BLD_PUSH_PARM_CI)
#  error "Neither BLD_PUSH_PARM or BLD_PUSH_PARM_CI are defined - one must be defined for this routine to build properly"
#endif
{
	va_list			var;
	mval			*ret_value, *actpmv;
	int			mask, i, slots_needed;
	unsigned int		actualcnt, prev_count;
	lv_val			*actp;
	lv_val			**act_list_ptr;
	stack_frame		*save_frame;
	parm_slot		*curr_slot;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef BLD_PUSH_PARM
	VAR_START(var, truth_value);
	assert(PUSH_PARM_OVERHEAD <= totalcnt);
	ret_value = va_arg(var, mval *);
	mask = va_arg(var, int);
	actualcnt = va_arg(var, unsigned int);
	assert(PUSH_PARM_OVERHEAD + actualcnt == totalcnt);
#	endif
#	ifdef BLD_PUSH_PARM_CI
	ret_value = ci_parms->retaddr;
	actualcnt = ci_parms->argcnt;
	mask = ci_parms->mask;
#	endif
	assert(MAX_ACTUALS >= actualcnt);
	if (!TREF(parm_pool_ptr))
	{
		parm_pool_init(SLOTS_NEEDED_FOR_SET(actualcnt));		/* Allocate pool memory for actualcnt params
										 * plus the frame and mask_and_cnt slots. */
		act_list_ptr = &((*(TREF(parm_pool_ptr))->parms).actuallist);	/* Save a reference to the first vacant slot. */
	} else
	{	/* If some parameters have been saved, it is only safe to overwrite them if they have been marked read;
		 * in such case back off start_idx to the beginning of that set to reutilize the space.
		 */
		prev_count = (*(((TREF(parm_pool_ptr))->parms + (TREF(parm_pool_ptr))->start_idx) - 1)).mask_and_cnt.actualcnt;
		if ((0 != (TREF(parm_pool_ptr))->start_idx) && (MAX_ACTUALS < prev_count))
			(TREF(parm_pool_ptr))->start_idx -= (SLOTS_NEEDED_FOR_SET((prev_count - SAFE_TO_OVWRT)));
		assert(MAX_TOTAL_SLOTS > (TREF(parm_pool_ptr))->start_idx);	/* In debug, ensure we are not growing endlessly. */
		slots_needed = (TREF(parm_pool_ptr))->start_idx + SLOTS_NEEDED_FOR_SET(actualcnt);
		if (slots_needed > (TREF(parm_pool_ptr))->capacity)		/* If we have less than needed, expand the pool. */
			parm_pool_expand(slots_needed, (TREF(parm_pool_ptr))->start_idx);
		/* Save a reference to the first vacant slot. */
		act_list_ptr = &((*((TREF(parm_pool_ptr))->parms + (TREF(parm_pool_ptr))->start_idx)).actuallist);
	}
	save_frame = NULL;
	if (frame_pointer->old_frame_pointer)
	{	/* Temporarily rewind frame_pointer if the parent is not a base frame. */
		save_frame = frame_pointer;
		frame_pointer = frame_pointer->old_frame_pointer;
	}
	for (i = 0; i < actualcnt; i++, act_list_ptr++)				/* Save parameters in the following empty slots. */
	{
#		ifdef BLD_PUSH_PARM
		actp = va_arg(var, lv_val *);
#		endif
#		ifdef BLD_PUSH_PARM_CI
		actp = ci_parms->args[i];
#		endif
		if (!(mask & (1 << i)))
		{	/* Not a dotted pass-by-reference parm or call-in O/IO parm */
			actpmv = &actp->v;
			MV_FORCE_DEFINED_UNLESS_SKIPARG(actpmv);
			PUSH_MV_STENT(MVST_PVAL);
			mv_chain->mv_st_cont.mvs_pval.mvs_val = lv_getslot(curr_symval);
			LVVAL_INIT(mv_chain->mv_st_cont.mvs_pval.mvs_val, curr_symval);
			mv_chain->mv_st_cont.mvs_pval.mvs_val->v = *actpmv;		/* Copy mval input. */
			mv_chain->mv_st_cont.mvs_pval.mvs_ptab.save_value = NULL;	/* Filled in by op_bindparm. */
			mv_chain->mv_st_cont.mvs_pval.mvs_ptab.hte_addr = NULL;
			DEBUG_ONLY(mv_chain->mv_st_cont.mvs_pval.mvs_ptab.nam_addr = NULL);
			*act_list_ptr = (lv_val *)&mv_chain->mv_st_cont.mvs_pval;
		} else
			/* Dotted pass-by-reference parm. No save of previous value, just pass lvval. */
			*act_list_ptr = actp;
	}
#	ifdef BLD_PUSH_PARM
	va_end(var);
#	endif
	if (save_frame)								/* Restore frame_pointer if previously saved. */
		frame_pointer = save_frame;
	frame_pointer->ret_value = ret_value;					/* Save the return value in the stack frame. */
	if (ret_value)								/* Save $test value in the stack frame. */
		frame_pointer->dollar_test = truth_value;
	(TREF(parm_pool_ptr))->start_idx += SLOTS_NEEDED_FOR_SET(actualcnt);	/* Update start_idx for the future parameter set. */
	curr_slot = PARM_CURR_SLOT;
	(*(curr_slot - 1)).mask_and_cnt.mask = mask;				/* Save parameter mask. */
	(*(curr_slot - 1)).mask_and_cnt.actualcnt = actualcnt;			/* Save parameter count. */
	PARM_ACT_FRAME(curr_slot, actualcnt) = frame_pointer;			/* Save frame pointer. */
	assert(frame_pointer && (frame_pointer->type & SFT_COUNT));		/* Ensure we are dealing with a counted frame. */
}
