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
#include "stringpool.h"
#include "objlabel.h"
#include "cache.h"
#include "cache_cleanup.h"
#include "trans_code_cleanup.h"
#include "dm_setup.h"
#include "error.h"
#include "error_trap.h"
#include "io.h"
#ifdef GTM_TRIGGER
# include "gtm_trigger_trc.h"
#endif

GBLREF	unsigned short	proc_act_type;
GBLREF	stack_frame	*frame_pointer;
GBLREF	spdesc		stringpool;
GBLREF	spdesc		rts_stringpool;
GBLREF	mval		dollar_ztrap, dollar_etrap;
GBLREF	mstr		*err_act;
GBLREF	io_desc		*active_device;

error_def(ERR_STACKCRIT);
error_def(ERR_ERRWZTRAP);
error_def(ERR_ERRWETRAP);
error_def(ERR_ERRWIOEXC);

void trans_code_cleanup(void)
{
	stack_frame	*fp, *fpprev;
	uint4		err;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
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
	} else if (SFT_DEV_ACT == proc_act_type)
		err = ERR_ERRWIOEXC;
	else
		err = 0;
	proc_act_type = 0;
	if (TREF(compile_time))
	{
		TREF(compile_time) = FALSE;
		if (stringpool.base != rts_stringpool.base)
			stringpool = rts_stringpool;
	}
	for (fp = frame_pointer; fp; fp = fpprev)
	{
		fpprev = fp->old_frame_pointer;
#		ifdef GTM_TRIGGER
		if (SFT_TRIGR & fpprev->type)
			fpprev = *(stack_frame **)(fpprev + 1);
#		endif
		if (fp->type & SFT_DM)
			break;
		if (fp->type & SFT_COUNT)
		{
			assert(NULL != err_act);
			if ((ERR_ERRWZTRAP == err) || (ERR_ERRWETRAP == err))
			{	/* Whether ETRAP or ZTRAP we want to rethrow the error at one level down */
				SET_ERROR_FRAME(fp);	/* reset error_frame to point to the closest counted frame */
				assert(fp->flags & SFF_ETRAP_ERR);
				/* Turn off any device exception related flags now that we are going to handle errors using
				 * $ETRAP or $ZTRAP AT THE PARENT LEVEL only (no more device exceptions).
				 */
				fp->flags &= SFF_DEV_ACT_ERR_OFF;
				dollar_ztrap.str.len = 0;
				err_act = &dollar_etrap.str;
			} else if (ERR_ERRWIOEXC == err)
			{	/* Error while compiling device exception. Set SFF_ETRAP_ERR bit so control is transferred to
				 * error_return() which in turn will rethrow the error AT THE SAME LEVEL in order to try and
				 * use $ZTRAP or $ETRAP whichever is active. Also set the SFF_DEV_ACT_ERR bit to signify this
				 * is a device exception that is rethrown instead of a ztrap/etrap error. Also assert that
				 * the rethrow will not use IO exception again (thereby ensuring error processing will
				 * eventually terminate instead of indefinitely recursing).
				 */
				fp->flags |= (SFF_DEV_ACT_ERR | SFF_ETRAP_ERR);
				assert(NULL == active_device);	/* mdb_condition_handler should have reset it */
			} else if ((ERR_ERRWZBRK == err) || (ERR_ERRWEXC == err))
			{	/* For typical exceptions in ZBREAK and ZSTEP, get back to direct mode */
				dm_setup();
			} else
			{	/* The only other value of err that we know possible is ERR_ERRWZINTR. But we dont expect
				 * to be in trans_code_cleanup in that case so assert false for any other value of err.
				 */
				assert(FALSE);
			}
			break;
		}
		if (fp->type)
		{
			SET_ERR_CODE(fp, err);
		}
		/* If this frame is indicated for cache cleanup, do that cleanup
		 * now before we get rid of the pointers used by that cleanup.
		 */
		IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(fp);
		fp->mpc = CODE_ADDRESS(pseudo_ret);
		fp->ctxt = GTM_CONTEXT(pseudo_ret);
		fp->flags &= SFF_TRIGR_CALLD_OFF;	/* Frame enterable now with mpc reset */
		GTMTRIG_ONLY(DBGTRIGR((stderr, "trans_code_cleanup: turning off SFF_TRIGR_CALLD in frame 0x"lvaddr"\n",
				       frame_pointer)));
	}
	TREF(transform) = TRUE;
	if (err)
		dec_err(VARLSTCNT(1) err);
}
