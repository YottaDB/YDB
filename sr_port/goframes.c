/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include <rtnhdr.h>		/* needed for golevel.h */
#include "error.h"
#include "op.h"
#include "stack_frame.h"	/* needed for golevel.h */
#include "tp_frame.h"		/* needed for golevel.h */
#include "golevel.h"
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
GBLREF	boolean_t	skip_error_ret;
GBLREF	tp_frame	*tp_pointer;
GBLREF	stack_frame	*frame_pointer;

LITREF mval             literal_null;

#ifdef GTM_TRIGGER
void	goframes(int4 frames, boolean_t unwtrigrframe, boolean_t fromzgoto)
#else
void	goframes(int4 frames)
#endif
{
        mval            *ret_targ;

	GTMTRIG_ONLY(goframes_unwound_trigger = FALSE);
        for (ret_targ = NULL; frames--; )
        {
		while (tp_pointer && tp_pointer->fp <= frame_pointer)
		{
               	        OP_TROLLBACK(-1);
		}
		if (0 == frames)
		{
			ret_targ = (mval *)get_ret_targ(NULL);
	       		if (NULL != ret_targ)
	       		{
	       		        *ret_targ = literal_null;
	       		        ret_targ->mvtype |= MV_RETARG;
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
