/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
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
#include "tp_frame.h"
#include "golevel.h"
#include "dollar_zlevel.h"
#include "error.h"

GBLREF	stack_frame	*frame_pointer;

error_def(ERR_ZGOTOTOOBIG);
error_def(ERR_ZGOTOLTZERO);

#ifdef GTM_TRIGGER
void	golevel(int4 level, boolean_t unwtrigrframe)
#else
void	golevel(int4 level)
#endif
{
        stack_frame     *fp, *fpprev;
        int4            unwframes, unwlevels, prevlvl;

        if (0 > level)
                rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZGOTOLTZERO);
	unwlevels = dollar_zlevel() - level;
        if (0 > unwlevels)
		/* Couldn't get to the level we were trying to unwind to */
                rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZGOTOTOOBIG);
	unwframes = 0;
        for (fp = frame_pointer; NULL != fp; fp = fpprev)
        {
		assert(0 <= unwlevels);
		fpprev = fp->old_frame_pointer;
		if (NULL == fpprev GTMTRIG_ONLY( && !(SFT_TRIGR & fp->type)))
			break;		/* break on base frame not trigger related */
#		ifdef GTM_TRIGGER
		/* Break if level reached -- note trigger base frame is type=counted but is not counted as
		 * part of level our count is not decremented on a trigger base frame.
		 */
		if (SFT_COUNT & fp->type)
		{
			if (0 == unwlevels)
				break;	/* break on reaching target level with a counted frame */
			if (!(SFT_TRIGR & fp->type))
				unwlevels--;
		}
		unwframes++;
		if (SFT_TRIGR & fp->type)
		{	/* Unwinding a trigger base frame leaves a null frame_pointer so allow us to jump over the
			 * base frame to the rich stack beneath..
			 */
			assert(NULL == fpprev);
			fpprev = *(stack_frame **)(fp + 1);
			assert(NULL != fpprev);
		}
#		else
		if ((SFT_COUNT & fp->type) && (0 == unwlevels--))
			break;		/* break on reaching target level with a counted frame */
		unwframes++;
#		endif
        }
	DBGEHND_ONLY(prevlvl = dollar_zlevel());
	GOFRAMES(unwframes, unwtrigrframe, FALSE);
	DBGEHND((stderr, "golevel: Unwound from level %d to level %d  which is %d frames ending in stackframe 0x"lvaddr" with"
		 " type 0x%04lx\n", prevlvl, level, unwframes, frame_pointer, (frame_pointer ? frame_pointer->type : 0xffff)));
        return;
}
