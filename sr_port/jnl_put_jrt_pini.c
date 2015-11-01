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
#include "gtm_time.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write.h"

GBLDEF	jnl_fence_control	jnl_fence_ctl;
GBLDEF	jnl_process_vector	*prc_vec;

GBLREF	short			dollar_tlevel;
GBLREF	uint4			gbl_jrec_time;	/* see comment in gbldefs.c for usage */

LITREF	int			jnl_fixed_size[];

void	jnl_put_jrt_pini(sgmnt_addrs *csa)
{

	assert(csa->now_crit);
	JNL_WHOLE_TIME(prc_vec->jpv_time);
	/* For non-TP, it is possible that a non t_end/tp_tend routine (e.g. the routines that write an aimg record
	 * or an inctn record) calls this function with differing early_tn and curr_tn, in which case that should
	 * (and might not) have set gbl_jrec_time appropriately. We don't want to take a chance.
	 */
	if (dollar_tlevel && csa->ti->early_tn != csa->ti->curr_tn)
	{	/* in the commit phase of a transaction */
		assert(csa->ti->early_tn == csa->ti->curr_tn + 1);
		MID_TIME(prc_vec->jpv_time) = gbl_jrec_time;	/* Reset mid_time to correspond to gbl_jrec_time */
	}
	csa->jnl->regnum = ++jnl_fence_ctl.total_regions;
	jnl_write(csa->jnl, JRT_PINI, (jrec_union *)prc_vec, NULL, NULL);
	csa->jnl->pini_addr = csa->jnl->jnl_buff->freeaddr - JREC_PREFIX_SIZE - JREC_SUFFIX_SIZE - jnl_fixed_size[JRT_PINI];
}
