/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rtnhdr.h>
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
GBLREF	boolean_t	ztrap_explicit_null;

error_def(ERR_STACKCRIT);

void trans_code_cleanup(void)
{
	stack_frame	*fp, *fpprev;
	uint4		errmsg;
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
			errmsg = ERR_ERRWZTRAP;
		else
			errmsg = ERR_ERRWETRAP;
	} else if (SFT_DEV_ACT == proc_act_type)
		errmsg = ERR_ERRWIOEXC;
	else
		errmsg = 0;
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
			if ((ERR_ERRWZTRAP == errmsg) || (ERR_ERRWETRAP == errmsg))
			{	/* Whether ETRAP or ZTRAP we want to rethrow the error at one level down */
				SET_ERROR_FRAME(fp);	/* reset error_frame to point to the closest counted frame */
				assert(fp->flags & SFF_ETRAP_ERR);
				/* Turn off any device exception related flags now that we are going to handle errors using
				 * $ETRAP or $ZTRAP AT THE PARENT LEVEL only (no more device exceptions).
				 */
				dollar_ztrap.str.len = 0;
				ztrap_explicit_null = FALSE;
				fp->flags &= SFF_DEV_ACT_ERR_OFF;
				fp->flags &= SFF_ZTRAP_ERR_OFF;
				err_act = &dollar_etrap.str;
				break;
			} else if (ERR_ERRWIOEXC == errmsg)
			{	/* Error while compiling device exception. Set SFF_ETRAP_ERR bit so control is transferred to
				 * error_return() which in turn will rethrow the error AT THE SAME LEVEL in order to try and
				 * use $ZTRAP or $ETRAP whichever is active. Also set the SFF_DEV_ACT_ERR bit to signify this
				 * is a device exception that is rethrown instead of a ztrap/etrap error. Also assert that
				 * the rethrow will not use IO exception again (thereby ensuring error processing will
				 * eventually terminate instead of indefinitely recursing).
				 */
				fp->flags |= (SFF_DEV_ACT_ERR | SFF_ETRAP_ERR);
				assert(NULL == active_device);	/* mdb_condition_handler should have reset it */
				break;
			} else if ((ERR_ERRWZBRK == errmsg) || (ERR_ERRWEXC == errmsg))
			{	/* For typical exceptions in ZBREAK and ZSTEP, get back to direct mode */
				dm_setup();
				fp = frame_pointer;	/* Let code at end see direct mode is current frame */
				break;
			} else
			{	/* The only known way to be here is if the command is a command given in direct mode as
				 * mdb_condition_handler won't drive an error handler in that case which would be caught in
				 * one of the above conditions. Not breaking out of the loop here means the frame will just
				 * unwind and we'll break on the direct mode frame which will be redriven. If the parent frame
				 * is not a direct mode frame, we'll assert in debug or break in pro and just continue.
				 * to direct mode.
				 */
				assert(fp->flags && (SFF_INDCE));
				if (!fp->old_frame_pointer || !(fp->old_frame_pointer->type & SFT_DM))
				{
					assert(FALSE);
					break;
				}
			}
		}
		if (fp->type)
		{
			SET_ERR_CODE(fp, errmsg);
		}
		/* If this frame is indicated for cache cleanup, do that cleanup
		 * now before we get rid of the pointers used by that cleanup.
		 */
		IF_INDR_FRAME_CLEANUP_CACHE_ENTRY_AND_UNMARK(fp);
		fp->mpc = CODE_ADDRESS(pseudo_ret);
		fp->ctxt = GTM_CONTEXT(pseudo_ret);
		fp->flags &= SFF_IMPLTSTART_CALLD_OFF;	/* Frame enterable now with mpc reset */
		GTMTRIG_ONLY(DBGTRIGR((stderr, "trans_code_cleanup: turning off SFF_IMPLTSTART_CALLD in frame 0x"lvaddr"\n",
				       frame_pointer)));
	}
	TREF(transform) = TRUE;
	/* Only put error on console if frame we unwind to is a direct mode frame */
	if (0 != errmsg)
	{
		ecode_set(errmsg);			/* Add error with trap indicator to $ECODE */
		UNIX_ONLY(if (fp->type & SFT_DM))	/* Bypass check for VMS and just push error out */
		{
			UNIX_ONLY(PRN_ERROR);		/* Can only appear in condition handler in VMS */
			dec_err(VARLSTCNT(1) errmsg);
		}
	}
}
