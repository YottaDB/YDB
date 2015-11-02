/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "error.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "gdscc.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "interlock.h"
#include "preemptive_ch.h"
#include "add_inter.h"

GBLREF	gv_namehead		*reset_gv_target;
GBLREF	gv_namehead		*gv_target;
GBLREF	boolean_t		kip_incremented;
GBLREF	short			dollar_tlevel;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF  sgm_info                *first_sgm_info;

/* container for all the common chores that need to be performed on error conditions */

void preemptive_ch(int preemptive_severe)
{
	sgmnt_addrs	*csa;
	sgm_info	*si;
	if (INVALID_GV_TARGET != reset_gv_target)
	{
		if (SUCCESS != preemptive_severe && INFO != preemptive_severe)
			RESET_GV_TARGET;
	}
	if (0 < dollar_tlevel)
	{
		for (si = first_sgm_info;  si != NULL; si = si->next_sgm_info)
		{
			if (si->kip_incremented)
			{
				csa = si->tp_csa;
				DECR_KIP(csa->hdr, csa, si->kip_incremented);
			}
		}
	} else if (kip_incremented)
		DECR_KIP(cs_data, cs_addrs, kip_incremented);
}
