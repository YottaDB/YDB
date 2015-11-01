/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "objlabel.h"
#include "cache.h"
#include "trans_code_cleanup.h"
#include "dm_setup.h"
#include "error.h"
#include "error_trap.h"

GBLREF unsigned short	proc_act_type;
GBLREF stack_frame	*frame_pointer;
GBLREF spdesc		stringpool;
GBLREF spdesc		rts_stringpool;
GBLREF bool		compile_time;
GBLREF bool		transform;
GBLREF mval		dollar_ztrap, dollar_etrap;
GBLREF mstr             *err_act;

void trans_code_cleanup(void)
{
	stack_frame	*fp;
	uint4		err;

	error_def(ERR_STACKCRIT);
	error_def(ERR_ERRWZTRAP);
	error_def(ERR_ERRWETRAP);
	error_def(ERR_ERRWIOEXC);

	assert(!(SFT_ZINTR & proc_act_type));
	/* With no extra ztrap frame being pushed onto stack, we may miss error(s)
	 * during trans_code if we don't check proc_act_type in addition to
	 * frame_pointer->type below.
	 */
	if (SFT_ZTRAP == proc_act_type)
	{
		if (0 < dollar_ztrap.str.len)
			err = (int)ERR_ERRWZTRAP;
		else
		{
			assert(0 < dollar_etrap.str.len);
			err = (int)ERR_ERRWETRAP;
		}
		frame_pointer->flags |= SFF_ZTRAP_ERR;
	} else if (SFT_DEV_ACT == proc_act_type)
	{
		err = ERR_ERRWIOEXC;
		frame_pointer->flags |= SFF_DEV_ACT_ERR;
	} else
		err = 0;

	proc_act_type = 0;
	if (compile_time)
	{
		compile_time = FALSE;
		if (stringpool.base != rts_stringpool.base)
			stringpool = rts_stringpool;
	}
	for (fp = frame_pointer; fp; fp = fp->old_frame_pointer)
	{
		if (fp->type & SFT_DM)
			break;
		if (fp->type & SFT_COUNT)
		{
			assert(NULL != err_act);
			if (!IS_ETRAP)
				dm_setup();
			break;
		}
		if (fp->type)
		{
			SET_ERR_CODE(fp, err);
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
