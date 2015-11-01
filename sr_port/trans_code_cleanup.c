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
#include "rtnhdr.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "masscomp.h"
#include "cache.h"
#include "trans_code_cleanup.h"
#include "dm_setup.h"
#include "error.h"

GBLREF unsigned char	proc_act_type;
GBLREF stack_frame	*frame_pointer;
GBLREF spdesc		stringpool;
GBLREF spdesc		rts_stringpool;
GBLREF bool		compile_time;
GBLREF bool		transform;

void trans_code_cleanup(int signal)
{
	stack_frame	*fp;
	uint4		err;
	unsigned char	save_proc_act_type = proc_act_type;
	error_def(ERR_STACKCRIT);
	error_def(ERR_ERRWZTRAP);
	error_def(ERR_ERRWZBRK);
	error_def(ERR_ERRWIOEXC);
	error_def(ERR_ERRWEXC);

	/* With no extra ztrap frame being pushed onto stack, we may miss error(s)
	 * during trans_code if we don't check proc_act_type in addition to
	 * frame_pointer->type below.
	 */
	if (SFT_ZTRAP == proc_act_type)
	{
		err = ERR_ERRWZTRAP;
		frame_pointer->flags |= SFF_ZTRAP_ERR;
	}
	else if (SFT_DEV_ACT == proc_act_type)
	{
		err = ERR_ERRWIOEXC;
		frame_pointer->flags |= SFF_DEV_ACT_ERR;
	}
	else
		err = 0;

	if (compile_time)
	{
		compile_time = FALSE;
		if (stringpool.base != rts_stringpool.base)
			stringpool = rts_stringpool;
		proc_act_type = 0;
	}
	for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
	{
		if (fp->type & SFT_DM)
			break;
		if (fp->type & SFT_COUNT)
		{
			if (!save_proc_act_type || (int)ERR_STACKCRIT != signal)
				dm_setup();
			break;
		}
		if (fp->type)
		{
			switch (fp->type)
			{
			case SFT_ZBRK_ACT:
				err = (int) ERR_ERRWZBRK;
				break;
			case SFT_DEV_ACT:
				err = (int) ERR_ERRWIOEXC;
				break;
			case SFT_ZTRAP:
				err = (int) ERR_ERRWZTRAP;
				break;
			case SFT_ZSTEP_ACT:
				err = (int) ERR_ERRWEXC;
				break;
			default:
				GTMASSERT;
			}
		}
		/* If this frame is indicated for cache cleanup, do that cleanup
		   now before we get rid of the pointers used by that cleanup.
		*/
		IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(fp);
		fp->mpc = CODE_ADDRESS(pseudo_ret);
		fp->ctxt = CONTEXT(pseudo_ret);
	}
	transform = TRUE;
	if (err)
		dec_err(VARLSTCNT(1) err);
}
