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
#include "gtm_newintrinsic.h"
#include "dollar_zlevel.h"
#include "op.h"
#include "svnames.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#ifdef VMS
#include <fab.h>		/* needed for dbgbldir_sysops.h */
#endif
#include "dpgbldir.h"
#include "dpgbldir_sysops.h"
#include "error_trap.h"		/* for STACK_ZTRAP_EXPLICIT_NULL macro */

GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF gd_addr		*gd_header;
GBLREF gd_binding	*gd_map;
GBLREF gd_binding	*gd_map_top;
GBLREF mval		dollar_ztrap;
GBLREF mval		dollar_etrap;
GBLREF mval		dollar_estack_delta;
GBLREF mval		dollar_zyerror;
GBLREF mval		dollar_zgbldir;
GBLREF boolean_t	ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
#ifdef GTM_TRIGGER
GBLREF int4		gtm_trigger_depth;
GBLREF mval		dollar_ztwormhole;
#endif

error_def(ERR_NOZTRAPINTRIG);

/* Routine to NEW a special intrinsic variable. Note that gtm_newinstrinsic(),
   which actually does the dirty work, may shift the stack to insert the mv_stent
   which saves the old value. Because of this, any caller of this module MUST
   reload the stack pointers to the M stackframe. This is normally taken care of
   by opp_newintrinsic().
*/
void op_newintrinsic(int intrtype)
{
	mval		*intrinsic;
	boolean_t	stored_explicit_null;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch(intrtype)
	{
		case SV_ZTRAP:
#			ifdef GTM_TRIGGER
			if (0 < gtm_trigger_depth)
				rts_error(VARLSTCNT(1) ERR_NOZTRAPINTRIG);
#			endif
			/* Due to the potential intermix of $ETRAP and $ZTRAP, we put a condition on the
			   explicit NEWing of these two special variables. If "the other" trap handler
			   definition is not null (meaning this handler is not in control) then we will
			   ignore the NEW. This is necessary for example when a frame with $ZT set calls
			   a routine that NEWs and sets $ET. When it unwinds, we don't want it to pop off
			   the old "null" value for $ET which then triggers the nulling out of our current
			   $ZT value. Note that op_svput no longer calls this routine for "implicit" NEWs
			   but calls directly to gtm_newintrinsic instead.
			*/
			if (dollar_etrap.str.len)
			{
				assert(FALSE == ztrap_explicit_null);
				return;
			}
			assert(!ztrap_explicit_null || (0 == dollar_ztrap.str.len));
			DEBUG_ONLY(stored_explicit_null = FALSE;)
			if (ztrap_explicit_null && (0 == dollar_ztrap.str.len))
			{
				DEBUG_ONLY(stored_explicit_null = TRUE;)
				dollar_ztrap.str.len = STACK_ZTRAP_EXPLICIT_NULL;	/* to be later used by unw_mv_ent() */
			}
			intrinsic = &dollar_ztrap;
			break;
		case SV_ETRAP:
			/* See comment above for SV_ZTRAP */
			if (dollar_ztrap.str.len)
			{
				assert(FALSE == ztrap_explicit_null);
				return;
			}
			intrinsic = &dollar_etrap;
			break;
		case SV_ESTACK:
			intrinsic = &dollar_estack_delta;
			break;
		case SV_ZYERROR:
			intrinsic = &dollar_zyerror;
			break;
		case SV_ZGBLDIR:
			intrinsic = &dollar_zgbldir;
			break;
#		ifdef GTM_TRIGGER
		case SV_ZTWORMHOLE:
			intrinsic = &dollar_ztwormhole;
			break;
#		endif
		default:	/* Only above types defined by compiler */
			GTMASSERT;
	}
	gtm_newintrinsic(intrinsic);
	if (SV_ESTACK == intrtype)
	{	/* Some extra processing for new of $ETRAP:
		   The delta from $zlevel we keep for estack is kept in an mval for sake of
		   ease -- gtm_newintrinic knows how to save, new and restore an mval -- but
		   this value is never used AS an mval. We keep the delta in m[0]. Here we
		   are "normalizing" $estack to be same as $stack value.
		*/
		dollar_estack_delta.m[0] = dollar_zlevel() - 1;
	} else if (SV_ZGBLDIR == intrtype)
	{
		if (dollar_zgbldir.str.len != 0)
		{
			gd_header = zgbldir(&dollar_zgbldir);
			/* update the gd_map */
			SET_GD_MAP;
		} else
		{
			dpzgbini();
        		gd_header = NULL;
		}
		if (gv_currkey)
			gv_currkey->base[0] = 0;
		if (gv_target)
			gv_target->clue.end = 0;
	}
	assert((SV_ZTRAP != intrtype) || !stored_explicit_null || (0 == intrinsic->str.len));
        return;
}
