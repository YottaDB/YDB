/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef UNIX
# include "io.h"
#endif
#include "error.h"
#include "indir_enum.h"
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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#ifdef GTM_TRIGGER
# include "gv_trigger.h"
# include "gtm_trigger.h"
#endif
#include "ztimeout_routines.h"

#define POP_SPECIFIED 	(ZTRAP_POP & (TREF(ztrap_form)) && (level2go = MV_FORCE_INTD(&ztrap_pop2level))) /* note: assignment */

GBLREF stack_frame	*frame_pointer, *zyerr_frame, *error_frame;
GBLREF unsigned short	proc_act_type;
GBLREF spdesc		rts_stringpool, stringpool, indr_stringpool;
GBLREF mstr		*err_act;
GBLREF boolean_t	is_tracing_on;
GBLREF mval		dollar_zyerror, ztrap_pop2level;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*restart_pc, *restart_ctxt;
GBLREF io_desc		*gtm_err_dev;
GBLREF boolean_t	tref_transform;

error_def(ERR_ASSERT);
error_def(ERR_CTRAP);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

/* After researching the usability of the extra ztrap frame, it is found that
 * the extra frame is not really necessary and hence no more pushed onto the stack.
 * That is, trans_code does not call copy_stack_frame() anymore and neither does
 * op_unwind have to unwind the extra ztrap frame.
 * The functions affected by this change :
 *	op_unwind : Now, it does not need to pop out an extra frame in case
 *		    the curent frame is of ztrap or dev_act type.
 *	trans_code_cleanup : Should also check proc_act_type value for reporting
 *		    ztrap or dev_act error.
 */

void trans_code_finish(void);
void trans_code(void);

CONDITION_HANDLER(zyerr_ch)
{
	START_CH(TRUE);
	if (indr_stringpool.base == stringpool.base)
	{ /* switch to run time stringpool */
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
	}
	if (!DUMPABLE)
	{
		TREF(compile_time) = FALSE;
		UNWIND(NULL, NULL); /* ignore $ZYERROR compile time errors; continue with $ZTRAP (or DEV exception) action */
	} else
	{
		TREF(compile_time) = FALSE;
		NEXTCH; /* serious error, can't ignore; handle error appropriately */
	}
}

void trans_code_finish(void)
{
	mval		dummy;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	frame_pointer->type = proc_act_type;
	proc_act_type = 0;
	if (0 != dollar_zyerror.str.len)
	{
		dummy.mvtype = MV_STR;
		dummy.str = dollar_zyerror.str;
		ESTABLISH(zyerr_ch);
		op_commarg(&dummy, indir_do);
		gv_namenaked_state = NAMENAKED_ININTERRUPT; /* $ZYERROR interrupt; code executed is not visible at compile time */
		REVERT;
		op_newintrinsic(SV_ZYERROR); /* for user's convenience */
		assert(NULL == zyerr_frame);
		zyerr_frame = frame_pointer;
	}
	TREF(compile_time) = FALSE;	/* Switching back to run-time */
	return;
}

CONDITION_HANDLER(trans_code_ch)
{
	int		level2go;

	START_CH(TRUE);
	/* Treat $ZTRAP (and DEVICE exception action) as the target entryref for an implicit GOTO */
	if (DUMPABLE 				/* fatal error; we test for STACKOFLOW as part of DUMPABLE test */
	    || (int)ERR_STACKCRIT == SIGNAL 	/* Successfully compiled ${Z,E}TRAP code but encountered STACK error while
						 * attempting to push new frame, OR, STACK error while executing $ZTRAP entryref
						 */
	    || !(ZTRAP_ENTRYREF & TREF(ztrap_form))	/* User doesn't want ENTRYREF form for $ZTRAP */
	    || !(ZTRAP_CODE & TREF(ztrap_form))	/* Error during $ZTRAP ENTRYREF processing */
	    || IS_ETRAP)			/* Error compiling $ETRAP code */
	{
		NEXTCH;
	}
	assert(!TREF(compile_time) || (FALSE == tref_transform));
	assert(TREF(compile_time) || (TRUE == tref_transform));
	/* This is for when stack gets unwound in op_commarg before comp_fini gets called*/
	tref_transform = TRUE;
	TREF(compile_time) = FALSE;
	assert(SFT_ZTRAP == proc_act_type || SFT_DEV_ACT == proc_act_type); /* are trans_code and this function in sync? */
	assert(NULL != indr_stringpool.base); /* indr_stringpool must have been initialized by now */
	if (indr_stringpool.base == stringpool.base)
	{ /* switch to run time stringpool */
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
	}
	if (POP_SPECIFIED)
	{ /* pop to the level of last 'set $ztrap' */
		GOLEVEL(level2go, TRUE);
		/* Note: TREF(trans_code_pop) already holds the stuff that we need to recompile */
	}
	op_commarg(TADR(trans_code_pop), indir_goto);
	gv_namenaked_state = NAMENAKED_ININTERRUPT; /* $ZTRAP or EXCEPTION interrupt;
							the code executed is not visible at compile time. */
	if (NULL != gtm_err_dev)
	{
		if ((gtmsocket == gtm_err_dev->type) && gtm_err_dev->newly_created)
		{
			assert(gtm_err_dev->state != dev_open);
			iosocket_destroy(gtm_err_dev);
			gtm_err_dev = NULL;
		}
#		ifdef UNIX
		if ((NULL != gtm_err_dev) && (gtmsocket != gtm_err_dev->type))
			remove_rms(gtm_err_dev);
#		endif
		gtm_err_dev = NULL;
	}
	trans_code_finish();
	UNWIND(NULL, NULL);
}

void trans_code(void)
{
	int		level2go;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (SFT_ZINTR & proc_act_type)
	{	/* Need different translator EP */
		jobintrpt_ztime_process(FALSE);	/* FALSE indicates jobinterrupt - NOT ztimeout */
		return;
	}
	if ((SFT_ZTIMEOUT & proc_act_type) && ((TREF(dollar_ztimeout)).ztimeout_vector.str.len))
	{	/* Else current ETRAP or ZTRAP is the vector */
		jobintrpt_ztime_process(TRUE);	/* TRUE indicates ztimeout */
		return;
	}
	assert(err_act);
	if (stringpool.base != rts_stringpool.base)
	{
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
	}
	assert(SFT_ZTRAP == proc_act_type || SFT_DEV_ACT == proc_act_type);
	/* The frame_pointer->mpc of error-causing M routine should always be set
	 * to 'error_return' irrespective of the validity of $etrap code to make sure
	 * the error-occuring frame is always unwound and the new error is rethrown
	 * at one level below
	 */
	if (IS_ETRAP)
		SET_ERROR_FRAME(frame_pointer);	/* reset error_frame to point to frame_pointer */
	if (!(ZTRAP_CODE & TREF(ztrap_form)) && !IS_ETRAP && POP_SPECIFIED)
	{
		GOLEVEL(level2go, TRUE);
	}
	(TREF(trans_code_pop)).mvtype = MV_STR;
	(TREF(trans_code_pop)).str = *err_act;
	ESTABLISH(trans_code_ch);
	op_commarg(TADR(trans_code_pop), (ZTRAP_CODE & (TREF(ztrap_form)) || IS_ETRAP) ? indir_linetail : indir_goto);
	gv_namenaked_state = NAMENAKED_ININTERRUPT; /* $ETRAP interrupt; the code executed is not visible at compile time. */
	REVERT;
	if (NULL != gtm_err_dev)
	{
		if (gtmsocket != gtm_err_dev->type)
			remove_rms(gtm_err_dev);
		else if (gtm_err_dev->newly_created)
		{
			assert(gtm_err_dev->state != dev_open);
			iosocket_destroy(gtm_err_dev);
		}
		gtm_err_dev = NULL;
	}
	trans_code_finish();
	return;
}
