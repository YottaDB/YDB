/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"
#include "tp_frame.h"
#include "unw_retarg.h"
#include "unwind_nocounts.h"
#include "error_trap.h"
#include "error.h"
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "min_max.h"
#include "compiler.h"
#include "parm_pool.h"
#include "get_ret_targ.h"
#include "opcode.h"
#include "glvn_pool.h"

GBLREF	void		(*unw_prof_frame_ptr)(void);
GBLREF	stack_frame	*frame_pointer, *zyerr_frame;
GBLREF	unsigned char	*msp,*stackbase,*stacktop;
GBLREF	mv_stent	*mv_chain;
GBLREF	tp_frame	*tp_pointer;
GBLREF	boolean_t	is_tracing_on;
GBLREF	symval		*curr_symval;
GBLREF	mval		*alias_retarg;
GBLREF	boolean_t	dollar_truth;
GBLREF	boolean_t	dollar_zquit_anyway;

LITREF	mval 		literal_null;

error_def(ERR_ALIASEXPECTED);
error_def(ERR_NOTEXTRINSIC);
error_def(ERR_STACKUNDERFLO);
error_def(ERR_TPQUIT);

/* This has to be maintained in parallel with op_unwind(), the unwind without a return argument (intrinsic quit) routine. */
int unw_retarg(mval *src, boolean_t alias_return)
{
	mval		ret_value, *trg;
	boolean_t	got_ret_target;
	stack_frame	*prevfp;
	lv_val		*srclv, *srclvc, *base_lv;
	symval		*symlv, *symlvc;
	int4		srcsymvlvl;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	assert(NULL == alias_retarg);
	alias_retarg = NULL;
	DBGEHND_ONLY(prevfp = frame_pointer);
	if (tp_pointer && tp_pointer->fp <= frame_pointer)
		rts_error(VARLSTCNT(1) ERR_TPQUIT);
	assert(msp <= stackbase && msp > stacktop);
	assert(mv_chain <= (mv_stent *)stackbase && mv_chain > (mv_stent *)stacktop);
	assert(frame_pointer <= (stack_frame *)stackbase && frame_pointer > (stack_frame *)stacktop);
	got_ret_target = FALSE;
	/* Before we do any unwinding or even verify the existence of the return var, check to see if we are returning
	 * an alias (or container). We do this now because (1) alias returns don't need to be defined and (2) the returning
	 * item could go out of scope in the unwinds so we have to bump the returned item's reference counts NOW.
	 */
	if (!alias_return)
	{	/* Return of "regular" value - Verify it exists */
		MV_FORCE_DEFINED(src);
		ret_value = *src;
		ret_value.mvtype &= ~MV_ALIASCONT;	/* Make sure alias container of regular return does not propagate */
	} else
	{	/* QUIT *var or *var(indx..) syntax was used - see which one it was */
		assert(NULL != src);
		srclv = (lv_val *)src;		/* Since can never be an expression, this relationship is guaranteed */
		if (!LV_IS_BASE_VAR(srclv))
		{	/* Have a potential container var - verify */
			if (!(MV_ALIASCONT & srclv->v.mvtype))
				rts_error(VARLSTCNT(1) ERR_ALIASEXPECTED);
			ret_value = *src;
			srclvc = (lv_val *)srclv->v.str.addr;
			assert(LV_IS_BASE_VAR(srclvc));	/* Verify base var */
			assert(srclvc->stats.trefcnt >= srclvc->stats.crefcnt);
			assert(1 <= srclvc->stats.crefcnt);				/* Verify is existing container ref */
			base_lv = LV_GET_BASE_VAR(srclv);
			symlv = LV_GET_SYMVAL(base_lv);
			symlvc = LV_GET_SYMVAL(srclvc);
			MARK_ALIAS_ACTIVE(MIN(symlv->symvlvl, symlvc->symvlvl));
			DBGRFCT((stderr, "unw_retarg: Returning alias container 0x"lvaddr" pointing to 0x"lvaddr" to caller\n",
				 src, srclvc));
		} else
		{	/* Creating a new alias - create a container to pass back */
			memcpy(&ret_value, &literal_null, SIZEOF(mval));
			ret_value.mvtype |= MV_ALIASCONT;
			ret_value.str.addr = (char *)srclv;
			srclvc = srclv;
			MARK_ALIAS_ACTIVE(LV_SYMVAL(srclv)->symvlvl);
			DBGRFCT((stderr, "unw_retarg: Returning alias 0x"lvaddr" to caller\n", srclvc));
		}
		INCR_TREFCNT(srclvc);
		INCR_CREFCNT(srclvc);		/* This increment will be reversed if this container gets put into an alias */
		/* We have a slight chicken-and-egg problem now. The mv_stent unwind loop below may pop a symbol table thus
		 * destroying the lv_val in our container. To prevent this, we need to locate the parm block before the symval is
		 * unwound and set the return value and alias_retarg appropriately so the symtab unwind logic called by
		 * unw_mv_ent() can work any necessary relocation magic on the return var.
		 */
		trg = get_ret_targ(NULL);
		if (NULL != trg)
		{
			*trg = ret_value;
			alias_retarg = trg;
			got_ret_target = TRUE;
		} /* else fall into below which will raise the NOTEXTRINSIC error */
	}
	/* Note: we are unwinding uncounted (indirect) frames here to allow the QUIT command to have indirect arguments
	 * and thus be executed by commarg in an indirect frame. By unrolling the indirect frames here we get back to
	 * the point where we can find where to put the quit value.
	 */
	unwind_nocounts();
	assert(frame_pointer && (frame_pointer->type & SFT_COUNT));
	while (mv_chain < (mv_stent *)frame_pointer)
	{
		msp = (unsigned char *)mv_chain;
		unw_mv_ent(mv_chain);
		POP_MV_STENT();
	}
	if (0 <= frame_pointer->dollar_test)
		dollar_truth = (boolean_t)frame_pointer->dollar_test;
	/* Now that we have unwound the uncounted frames, we should be left with a counted frame that
	 * contains some ret_value, NULL or not. If the value is non-NULL, let us restore the $TEST
	 * value from that frame as well as update *trg for non-alias returns.
	 */
	if ((trg = frame_pointer->ret_value) && !alias_return)	/* CAUTION: Assignment */
	{	/* If this is an alias_return arg, bypass the arg set logic which was done above. */
		assert(!got_ret_target);
		got_ret_target = TRUE;
		*trg = ret_value;
	}
	/* do not throw an error if return value is expected from a non-extrinsic, but dollar_zquit_anyway is true */
	if (!dollar_zquit_anyway && !got_ret_target)
		rts_error(VARLSTCNT(1) ERR_NOTEXTRINSIC);	/* This routine was not invoked as an extrinsic function */
	/* Note that error_ret() should be invoked only after the rts_error() of TPQUIT and NOTEXTRINSIC.
	 * This is so the TPQUIT/NOTEXTRINSIC error gets noted down in $ECODE (which wont happen if error_ret() is called before).
	 */
	INVOKE_ERROR_RET_IF_NEEDED;
	if (is_tracing_on)
		(*unw_prof_frame_ptr)();
	msp = (unsigned char *)frame_pointer + SIZEOF(stack_frame);
	DRAIN_GLVN_POOL_IF_NEEDED;
	PARM_ACT_UNSTACK_IF_NEEDED;
	frame_pointer = frame_pointer->old_frame_pointer;
	DBGEHND((stderr, "unw_retarg: Stack frame 0x"lvaddr" unwound - frame 0x"lvaddr" now current - New msp: 0x"lvaddr"\n",
		 prevfp, frame_pointer, msp));
	if ((NULL != zyerr_frame) && (frame_pointer > zyerr_frame))
		zyerr_frame = NULL;
	if (!frame_pointer)
		rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
	assert(frame_pointer >= (stack_frame *)msp);
	/* ensuring that trg is not NULL */
	if (!dollar_zquit_anyway || trg)
		trg->mvtype |= MV_RETARG;
	assert((frame_pointer < frame_pointer->old_frame_pointer) || (NULL == frame_pointer->old_frame_pointer));
	return 0;
}
