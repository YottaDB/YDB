/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
	boolean_t	stored_explicit_null, etrap_was_active;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (intrtype)
	{
		case SV_ZTRAP:
#			ifdef GTM_TRIGGER
			if (0 < gtm_trigger_depth)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOZTRAPINTRIG);
#			endif
			assert(!ztrap_explicit_null || (0 == (TREF(dollar_ztrap)).str.len));
			DEBUG_ONLY(stored_explicit_null = FALSE;)
			if (ztrap_explicit_null && (0 == (TREF(dollar_ztrap)).str.len))
			{
				DEBUG_ONLY(stored_explicit_null = TRUE;)
				(TREF(dollar_ztrap)).str.len = STACK_ZTRAP_EXPLICIT_NULL;	/* used later by unw_mv_ent() */
			}
			/* Intentionally omitted the "break" here */
		case SV_ETRAP:
			/* Save the active error trap to the stack if either of them is new'ed */
			if (etrap_was_active = ETRAP_IN_EFFECT)
				intrinsic = &(TREF(dollar_etrap));
			else
				intrinsic = &(TREF(dollar_ztrap));
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
			assertpro(FALSE && intrtype);
	}
	gtm_newintrinsic(intrinsic);
	if (SV_ZTRAP == intrtype)
	{
		ztrap_explicit_null = TRUE;
		if(etrap_was_active)
			NULLIFY_TRAP(TREF(dollar_etrap));
	} else if (SV_ETRAP == intrtype) {
		ztrap_explicit_null = FALSE;
		if(!etrap_was_active)
			NULLIFY_TRAP(TREF(dollar_ztrap));
	} else if (SV_ESTACK == intrtype)
	{	/* Some extra processing for new of $ETRAP:
		   The delta from $zlevel we keep for estack is kept in an mval for sake of
		   ease -- gtm_newintrinic knows how to save, new and restore an mval -- but
		   this value is never used AS an mval. We keep the delta in m[0]. Here we
		   are "normalizing" $estack to be same as $stack value.
		*/
		dollar_estack_delta.m[0] = dollar_zlevel() - 1;
	} else if (SV_ZGBLDIR == intrtype)
	{
		assert(0 == dollar_zgbldir.str.len);	/* "gtm_newintrinsic" call above should have cleared it */
		dpzgbini();				/* SET $ZGBLDIR="" */
		if (gv_currkey)
		{
			gv_currkey->base[0] = 0;
			gv_currkey->prev = gv_currkey->end = 0;
		}
		if (gv_target)
			gv_target->clue.end = 0;
	}
	assert((SV_ZTRAP != intrtype) || !stored_explicit_null || (0 == intrinsic->str.len));
        return;
}
