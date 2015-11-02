/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rtnhdr.h>
#include "lv_val.h"
#include "mv_stent.h"
#include "push_lvval.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF mv_stent 	*mv_chain;
GBLREF unsigned char 	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF symval		*curr_symval;
GBLREF uint4		dollar_tlevel;

/* Take supplied mval and place it into a freshly allocated lv_val */
lv_val *push_lvval(mval *arg1)
{
	lv_val	*lvp;

	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	/* Note that since this is only (currently) used by call-ins and no TP
	   transaction can be (currently) be active during a call-in, we do not
	   worry about setting up tp_var structures as is done in lv_newname().
	   The assert below will catch if this condition changes.
	*/
	assert(!dollar_tlevel);
	PUSH_MV_STENT(MVST_LVAL);
	mv_chain->mv_st_cont.mvs_lvval = lvp = lv_getslot(curr_symval);
	LVVAL_INIT(lvp, curr_symval);
	lvp->v = *arg1;
	return lvp;
}
