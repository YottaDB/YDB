/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "stack_frame.h"
#include "stringpool.h"
#include "op.h"
#include "mprof.h"

GBLREF stack_frame	*frame_pointer, *zyerr_frame;
GBLREF unsigned char	proc_act_type;
GBLREF spdesc		rts_stringpool, stringpool, indr_stringpool;
GBLREF mstr		*err_act;
GBLREF boolean_t	is_tracing_on;
GBLREF mval		dollar_zyerror, dollar_ztrap, zyerror_ztrap;
GBLREF int		ztrap_form;
GBLREF bool 		compile_time;

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
		dummy_ptr = push_mval(&dummy);
		ESTABLISH(zyerr_ch);
		op_commarg(dummy_ptr, indir_do);
		REVERT;
		op_newzyerror(); /* for user's convenience */
		assert(NULL == zyerr_frame);
		zyerr_frame = frame_pointer;
	}
	return;
}

CONDITION_HANDLER(trans_code_ch)
{
	/* Treat $ZTRAP (and DEVICE exception action) as the target entryref for an implicit GOTO */
	START_CH;
	if (DUMPABLE || /* fatal error; we test for STACKOFLOW as part of DUMPABLE test */
	    (int)ERR_STACKCRIT == SIGNAL || /* successfully compiled $ZTRAP code but encountered STACK error while attempting
					       to push new frame, OR, STACK error while executing $ZTRAP entryref */
	    !(ztrap_form & ZTRAP_ENTRYREF) || /* user doesn't want ENTRYREF form for $ZTRAP */
	    !(ztrap_form & ZTRAP_CODE)) /* error during ENTRYREF processing */
	{
		NEXTCH;
	}
	assert(SFT_ZTRAP == proc_act_type || SFT_DEV_ACT == proc_act_type); /* are trans_code and this function in sync? */
	assert(NULL != indr_stringpool.base); /* indr_stringpool must have been initialized by now */
	if (indr_stringpool.base == stringpool.base)
	{ /* switch to run time stringpool */
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
	}
	op_commarg(dummy_ptr, indir_goto);
	trans_code_finish();
	UNWIND(NULL, NULL);
}

void trans_code(void)
{
	mval		dummy;

	assert(err_act);
	if (stringpool.base != rts_stringpool.base)
		stringpool = rts_stringpool;
	dummy.mvtype = MV_STR;
	dummy.str = *err_act;
	dummy_ptr = push_mval(&dummy);
	assert(SFT_ZTRAP == proc_act_type || SFT_DEV_ACT == proc_act_type);
	ESTABLISH(trans_code_ch);
	op_commarg(dummy_ptr, (ztrap_form & ZTRAP_CODE) ? indir_linetail : indir_goto);
	REVERT;
	trans_code_finish();
	return;
}
