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
#include "gtm_newintrinsic.h"
#include "dollar_zlevel.h"
#include "op.h"
#include "svnames.h"

GBLREF mval	dollar_ztrap;
GBLREF mval	dollar_etrap;
GBLREF mval	dollar_estack_delta;
GBLREF mval	dollar_zyerror;

/* Routine to NEW a special intrinsic variable. Note that gtm_newinstrinsic(),
   which actually does the dirty work, may shift the stack to insert the mv_stent
   which saves the old value. Because of this, any caller of this module MUST
   reload the stack pointers to the M stackframe. This is normally taken care of
   by opp_newintrinsic().
*/
void op_newintrinsic(int intrtype)
{
	mval	*intrinsic;

	switch(intrtype)
	{
		case SV_ZTRAP:
			intrinsic = &dollar_ztrap;
			break;
		case SV_ETRAP:
			intrinsic = &dollar_etrap;
			break;
		case SV_ESTACK:
			intrinsic = &dollar_estack_delta;
			break;
		case SV_ZYERROR:
			intrinsic = &dollar_zyerror;
			break;
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
	}
        return;
}
