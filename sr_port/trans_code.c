/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "indir_enum.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "op.h"
#include "mprof.h"
#include "golevel.h"
#include "mvalconv.h"
#include "svnames.h"
#include "error_trap.h"
#include "jobinterrupt_process.h"
#include "dollar_zlevel.h"		/* for SET_ERROR_FRAME macro to use dollar_zlevel() function */

#define POP_SPECIFIED 	((ztrap_form & ZTRAP_POP) && (level2go = MV_FORCE_INTD(&ztrap_pop2level))) /* note: assignment */

GBLREF stack_frame	*frame_pointer, *zyerr_frame, *error_frame;
GBLREF unsigned short	proc_act_type;
GBLREF spdesc		rts_stringpool, stringpool, indr_stringpool;
GBLREF mstr		*err_act;
GBLREF boolean_t	is_tracing_on;
GBLREF mval		dollar_zyerror, dollar_ztrap, ztrap_pop2level;
GBLREF int		ztrap_form;
GBLREF bool 		compile_time;
GBLREF mval		dollar_etrap;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;
GBLREF mv_stent		*mv_chain;
GBLREF bool		transform;

error_def(ERR_ASSERT);
error_def(ERR_GTMCHECK);
error_def(ERR_GTMASSERT);
error_def(ERR_STACKOFLOW);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKCRIT);

/*
 * After researching the usability of the extra ztrap frame, it is found that
 * the extra frame is not really necessary and hence no more pushed onto the stack.
 * That is, trans_code does not call copy_stack_frame() anymore and neither does
 * op_unwind have to unwind the extra ztrap frame.
 * The functions affected by this change :
	op_unwind : Now, it does not need to pop out an extra frame in case
		    the curent frame is of ztrap or dev_act type.
	trans_code_cleanup : Should also check proc_act_type value for reporting
		    ztrap or dev_act error.
 * -MALLI (4/19/01)
 */

static mval	*dummy_ptr;
void trans_code_finish(void);
void trans_code(void);

CONDITION_HANDLER(zyerr_ch)
{
	START_CH;
	if (indr_stringpool.base == stringpool.base)
	{ /* switch to run time stringpool */
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
	}
	if (!DUMPABLE)
	{
		compile_time = FALSE;
		UNWIND(NULL, NULL); /* ignore $ZYERROR compile time errors; continue with $ZTRAP (or DEV exception) action */
	} else
	{
		NEXTCH; /* serious error, can't ignore; handle error appropriately */
	}
}

void trans_code_finish(void)
{
	mval		dummy;

	frame_pointer->type = proc_act_type;
	proc_act_type = 0;
	if (0 != dollar_zyerror.str.len)
	{
		dummy.mvtype = MV_STR;
		dummy.str = dollar_zyerror.str;
		ESTABLISH(zyerr_ch);
		op_commarg(&dummy, indir_do);
		REVERT;
		op_newintrinsic(SV_ZYERROR); /* for user's convenience */
		assert(NULL == zyerr_frame);
		zyerr_frame = frame_pointer;
	}

	return;
}

CONDITION_HANDLER(trans_code_ch)
{
	mval		dummy;
	int		level2go;

	/* Treat $ZTRAP (and DEVICE exception action) as the target entryref for an implicit GOTO */
	START_CH;
	if (DUMPABLE || /* fatal error; we test for STACKOFLOW as part of DUMPABLE test */
	    (int)ERR_STACKCRIT == SIGNAL || /* successfully compiled ${Z,E}TRAP code but encountered STACK error while attempting
					       to push new frame, OR, STACK error while executing $ZTRAP entryref */
	    !(ztrap_form & ZTRAP_ENTRYREF) || /* user doesn't want ENTRYREF form for $ZTRAP */
	    !(ztrap_form & ZTRAP_CODE) || /* error during $ZTRAP ENTRYREF processing */
	    IS_ETRAP) /* error compiling $ETRAP code */
	{
		NEXTCH;
	}
	assert(!compile_time || (FALSE == transform));
	assert(compile_time || (TRUE == transform));
	/* This is for when stack gets unwound in op_commarg before comp_fini gets called*/
	transform = TRUE;
	compile_time = FALSE;
	assert(SFT_ZTRAP == proc_act_type || SFT_DEV_ACT == proc_act_type); /* are trans_code and this function in sync? */
	assert(NULL != indr_stringpool.base); /* indr_stringpool must have been initialized by now */
	if (indr_stringpool.base == stringpool.base)
	{ /* switch to run time stringpool */
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
	}
	if (POP_SPECIFIED)
	{ /* pop to the level of last 'set $ztrap' */
		golevel(level2go);
		/* previous dummy_ptr would have been popped out */
		dummy.mvtype = MV_STR;
		dummy.str = *err_act;
		dummy_ptr = push_mval(&dummy);
	}
	op_commarg(dummy_ptr, indir_goto);
	trans_code_finish();
	UNWIND(NULL, NULL);
}

void trans_code(void)
{
	mval		dummy;
	int		level2go;

	if (SFT_ZINTR & proc_act_type)
	{	/* Need different translator EP */
		jobinterrupt_process();
		return;
	}
	assert(err_act);
	if (stringpool.base != rts_stringpool.base)
		stringpool = rts_stringpool;
	assert(SFT_ZTRAP == proc_act_type || SFT_DEV_ACT == proc_act_type);
	/* The frame_pointer->mpc of error-causing M routine should always be set
	 * to 'error_return' irrespective of the validity of $etrap code to make sure
	 * the error-occuring frame is always unwound and the new error is rethrown
	 * at one level below */
	if (IS_ETRAP)
	{
		SET_ERROR_FRAME(frame_pointer);	/* reset dollar_ecode.error_frame to point to frame_pointer as well as resetting
						 * error_frame_mpc, error_frame_ctxt fields and error_frame->{mpc,ctxt} */
	}
	if (!(ztrap_form & ZTRAP_CODE) && !IS_ETRAP && POP_SPECIFIED)
		golevel(level2go);
	dummy.mvtype = MV_STR;
	dummy.str = *err_act;
	dummy_ptr = push_mval(&dummy);

	ESTABLISH(trans_code_ch);
	op_commarg(dummy_ptr, ((ztrap_form & ZTRAP_CODE) || IS_ETRAP) ? indir_linetail : indir_goto);
	REVERT;
	trans_code_finish();
	return;
}
