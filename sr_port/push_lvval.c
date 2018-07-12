/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2009, 2010 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
=======
 * Copyright (c) 2009-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> df1555e... GT.M V6.3-005
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

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

/* Take supplied mval and place it into a freshly allocated lv_val */
lv_val *push_lvval(mval *arg1)
{
	lv_val		*lvp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

<<<<<<< HEAD
=======
	/* Assert TP active only when filters in use from inside GT.M */
	assert((!dollar_tlevel) || (TREF(comm_filter_init) && (TREF(gtmci_nested_level) == 1)));
>>>>>>> df1555e... GT.M V6.3-005
	PUSH_MV_STENT(MVST_LVAL);
	mv_chain->mv_st_cont.mvs_lvval = lvp = lv_getslot(curr_symval);
	LVVAL_INIT(lvp, curr_symval);
	lvp->v = *arg1;
	return lvp;
}
