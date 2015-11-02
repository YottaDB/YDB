/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "rtnhdr.h"
#include "stack_frame.h"
#include "mv_stent.h"
#include "tp_frame.h"
#include "get_ret_targ.h"

GBLREF stack_frame	*frame_pointer;
GBLREF unsigned char	*msp, *stackbase, *stacktop;
GBLREF mv_stent		*mv_chain;
GBLREF int		process_exiting;

mval *get_ret_targ(stack_frame **retsf)
{
	/* return the target of the return for this frame; return NULL if not called as extrinsic */

	mv_stent	*mvc;
	stack_frame	*sf;

	assert(msp <= stackbase && msp > stacktop);
	assert(mv_chain <= (mv_stent *)stackbase && mv_chain > (mv_stent *)stacktop);
	assert(frame_pointer <= (stack_frame *)stackbase && frame_pointer > (stack_frame *)stacktop);

	for (mvc = mv_chain, sf = frame_pointer; NULL != sf; sf = sf->old_frame_pointer)
	{
		for ( ; mvc < (mv_stent *)sf; mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc))
		{
			switch (mvc->mv_st_type)
			{
				case MVST_PARM:
					if (NULL != retsf)
						*retsf = sf;
					return mvc->mv_st_cont.mvs_parm.ret_value;
				case MVST_MSAV:
				case MVST_MVAL:
				case MVST_IARR:
				case MVST_STAB:
				case MVST_NTAB:
				case MVST_PVAL:
				case MVST_NVAL:
				case MVST_STCK:
				case MVST_STCK_SP:
				case MVST_TVAL:
				case MVST_TPHOLD:
				case MVST_ZINTR:
				case MVST_ZINTDEV:
				case MVST_TRIGR:
				case MVST_RSTRTPC:
				case MVST_STORIG:
				case MVST_MRGZWRSV:
					break;
				default:
					assert(FALSE);
					if (!process_exiting)
						GTMASSERT;
					break;
			}
			if (!mvc->mv_st_next)
				GTMASSERT;	/* Avoid endless loop if next field got zapped */
		}
		if (SFT_COUNT & sf->type)
			break;			/* Counted frame was checked, done */
	}
	return NULL;
}
