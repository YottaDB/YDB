/****************************************************************
 *								*
 * Copyright 2001, 2014 Fidelity Information Services, Inc	*
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

#include "gtm_stdio.h"

#include "rtnhdr.h"		/* needed for golevel.h */
#include "error.h"
#include "op.h"
#include "stack_frame.h"	/* needed for golevel.h */
#include "tp_frame.h"		/* needed for golevel.h */
#include "golevel.h"
#include "stringpool.h"
#ifdef GTM_TRIGGER
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
#include "gv_trigger.h"
#include "gtm_trigger.h"
#include "get_ret_targ.h"

GBLREF	boolean_t	goframes_unwound_trigger;
#endif
GBLREF	mval		*alias_retarg;
GBLREF	stack_frame	*frame_pointer;
GBLREF	boolean_t	skip_error_ret;
GBLREF	tp_frame	*tp_pointer;

LITREF mval             literal_null;

error_def(ERR_NOTEXTRINSIC);

#ifdef GTM_TRIGGER
void	goframes(int4 frames, boolean_t unwtrigrframe, boolean_t fromzgoto)
#else
void	goframes(int4 frames)
#endif
{
        mval            *ret_targ;
	stack_frame	*ret_fp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	GTMTRIG_ONLY(goframes_unwound_trigger = FALSE);
	for (ret_targ = NULL; frames--; )
	{
		while (tp_pointer && tp_pointer->fp <= frame_pointer)
		{
			OP_TROLLBACK(-1);
		}
		if (0 == frames)
		{
			ret_targ = get_ret_targ(&ret_fp);
			/* If alias_retarg is non-NULL, *ret_targ would have been already initialized so no need to set it.
			 * Setting it to literal_null in that case would cause reference counts to not be decremented later
			 * in op_unwind/mdb_condition_handler so it is actually necessary to skip it in that case.
			 */
			if ((NULL != ret_targ) && (NULL == alias_retarg))
			{	/* If gtmci_retval is set, use it instead of literal_null. Existing case of this usage is
				 * a return value from ZHALT. If other cases are added, the assert below may need to be
				 * adjusted or removed.
				 */
				assert(NULL != ret_fp->ret_value);
				assert((NULL == TREF(gtmci_retval)) || (0 < TREF(gtmci_nested_level)));
				if (NULL == TREF(gtmci_retval))
					*ret_targ = literal_null;
				else
				{
					*ret_targ = *(TREF(gtmci_retval));
					DBG_MARK_STRINGPOOL_USABLE;	/* Return mval now copied to a protected mval */
				}
				ret_targ->mvtype |= MV_RETARG;
			} else if ((NULL == ret_targ) && (NULL != TREF(gtmci_retval)))
			{	/* No ret_targ was found but we have a return value set but evidently not expected */
				assert(fromzgoto);
				assert(NULL == ret_fp->ret_value);
				DBG_MARK_STRINGPOOL_USABLE;
				TREF(gtmci_retval) = NULL;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTEXTRINSIC);
			}
		}
		skip_error_ret = TRUE;
#		ifdef GTM_TRIGGER
		if (!(SFT_TRIGR & frame_pointer->type))
		{	/* Normal frame unwind */
			DBGTRIGR((stderr, "goframes: unwinding regular frame at %016lx\n", frame_pointer));
			op_unwind();
			DBGTRIGR((stderr, "goframes: after regular frame unwind: frame_pointer 0x%016lx  ctxt value: 0x%016lx\n",
				  frame_pointer, ctxt));
		} else
		{	/* Trigger base frame unwind (special case) */
			DBGTRIGR((stderr, "goframes: unwinding trigger base frame at %016lx\n", frame_pointer));
			gtm_trigger_fini(TRUE, fromzgoto);
			goframes_unwound_trigger = TRUE;
		}
#		else
		/* If triggers are not enabled, just a normal unwind */
		DBGEHND((stderr, "goframes: unwinding regular frame at %016lx\n", frame_pointer));
		op_unwind();
#		endif
		assert(FALSE == skip_error_ret);	/* op_unwind() should have read and reset this */
		skip_error_ret = FALSE;			/* be safe in PRO versions */
	}
#	ifdef GTM_TRIGGER
	if (unwtrigrframe && (SFT_TRIGR & frame_pointer->type))
	{	/* If we landed on a trigger base frame after unwinding everything, we are in the same boat as if we had run into
		 * one while we were unwinding. We cannot return this frame to (for example) zgoto which is going to morph it into
		 * something else (unwtrigrframe only set when ZGOTO with entryref specified). So if the flag says we should never
		 * land on a trigger frame, go ahead and unwind that one too.
		 */
		DBGTRIGR((stderr, "goframes: unwinding trailing trigger base frame at %016lx\n", frame_pointer));
		gtm_trigger_fini(TRUE, fromzgoto);
		goframes_unwound_trigger = TRUE;
	}
#	endif

	return;
}
