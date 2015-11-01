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

#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "filestruct.h"
#include "jnl.h"
#include "dpgbldir.h"
#include "have_crit_any_region.h"

GBLREF volatile boolean_t	crit_in_flux;
GBLREF jnlpool_addrs		jnlpool;

/* If have or are aquiring crit in any region or the replication lock, return TRUE
 * ** NOTE **  This routine is called from signal handlers and is thus called asynchronously.
 * A TRUE value of in_commit causes us to check for a more restrictive case of crit where we are about to commit in some region.
 */
boolean_t have_crit_any_region(boolean_t in_commit)
{
	gd_region	*r_top, *r_local;
	gd_addr		*addr_ptr;
	sgmnt_addrs	*csa;

	if (crit_in_flux)
		return TRUE;
	for (addr_ptr = get_next_gdr(0); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top; r_local++)
		{
			if (r_local->open && !r_local->was_open)
			{
				csa = &FILE_INFO(r_local)->s_addrs;
				if (NULL != csa)
				{
					if (csa->now_crit && (!in_commit || csa->ti->early_tn != csa->ti->curr_tn))
						return TRUE;
				}
			}
		}
	}
	if (NULL != jnlpool.jnlpool_ctl)
	{
		csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
		if (NULL != csa && csa->now_crit)
			return TRUE;
	}
	return FALSE;
}
