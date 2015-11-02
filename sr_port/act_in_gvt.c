/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "spec_type.h"
#ifdef GTM_TRIGGER
#include <rtnhdr.h>
#include "gv_trigger.h"
#endif

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_altkey;

void act_in_gvt(void)
{
	collseq		*csp;
	error_def(ERR_COLLATIONUNDEF);
	error_def(ERR_COLLTYPVERSION);
	error_def(ERR_GVIS);

#	ifdef GTM_TRIGGER
	if ((HASHT_GBLNAME_LEN == gv_target->gvname.var_name.len) &&
			(0 == MEMCMP_LIT(gv_target->gvname.var_name.addr, HASHT_GBLNAME)))
		return;		/* No collation for triggers */
#	endif
	if (csp = ready_collseq((int)(gv_target->act)))
	{
		if (!do_verify(csp, gv_target->act, gv_target->ver))
		{
			gv_target->root = 0;
			rts_error(VARLSTCNT(8) ERR_COLLTYPVERSION, 2, gv_target->act, gv_target->ver,
				ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
		}
	}
	else
	{
		gv_target->root = 0;
		rts_error(VARLSTCNT(7) ERR_COLLATIONUNDEF, 1, gv_target->act,
			ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
	}
	gv_target->collseq = csp;
	return;
}
