/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "collseq.h"
#ifdef GTM_TRIGGER
#include "gv_trigger.h"
#endif

error_def(ERR_COLLATIONUNDEF);
error_def(ERR_COLLTYPVERSION);
error_def(ERR_GVIS);

void act_in_gvt(gv_namehead *gvt)
{
	collseq		*csp;

#	ifdef GTM_TRIGGER
	if (IS_MNAME_HASHT_GBLNAME(gvt->gvname.var_name))
		return;		/* No collation for triggers */
#	endif
	if ((csp = ready_collseq((int)(gvt->act))))	/* WARNING: ASSIGNMENT */
	{
		if (!do_verify(csp, gvt->act, gvt->ver))
		{
			gvt->root = 0;
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_COLLTYPVERSION, 2, gvt->act, gvt->ver,
				ERR_GVIS, 2, gvt->gvname.var_name.len, gvt->gvname.var_name.addr);
		}
	} else
	{
		gvt->root = 0;
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_COLLATIONUNDEF, 1, gvt->act,
			ERR_GVIS, 2, gvt->gvname.var_name.len, gvt->gvname.var_name.addr);
	}
	gvt->collseq = csp;
	return;
}
