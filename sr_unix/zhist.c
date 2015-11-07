/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "min_max.h"
#include <rtnhdr.h>
#include "zhist.h"
#include "gtmlink.h"

#ifdef USHBIN_SUPPORTED /* This entire file */

/* Routine called from op_rhd_ext() to determine if the routine being called needs to be relinked. To return TRUE,
 * the following conditions must be met:
 *   1. Auto-relinking must be enabled in the directory the routine was located in (which may be different from the
 *      directory of the current routine by the same name).
 *   2. This must be a newer version of the routine than what is currently linked.
 *   3. If the routine is on the stack, recursive relinks must be enabled.
 *
 * Parameters:
 *   - zhist - address of zro_hist block of the routine to check if needs relinking
 *
 * Output:
 *   - TRUE if new object code is (or may be) available - indicates op_rhd_ext() should relink
 *     FALSE if currently linked code is up-to-date
 */
boolean_t need_relink(rhdtyp *rtnhdr, zro_hist *zhist)
{
	zro_validation_entry	*iter;
	uint4			cur_cycle;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != zhist);		/* Should be called unless relinking is possible */
	/* TODO: Do this check second, if below logic would return TRUE */
	if ((LINK_NORECURSIVE == TREF(relink_allowed)) && on_stack(rtnhdr, NULL))
		return FALSE;	/* can't relink, or else we'll get LOADRUNNING */
	/* If SET=$ZRO cycle has changed since validation list was compiled, recreate the list.... just fully relink */
	/* TODO: Only relink when absolutely necessary. If routine has same hash in same location, don't relink */
	if (zhist->zroutines_cycle != TREF(set_zroutines_cycle))
		return TRUE;
	/* Traverse list corresponding to zro entries */
	for (iter = &zhist->base[0]; iter != zhist->end; iter++)
	{
		/* TODO: assert routine name near cycle_reladdr == current routine invocation */
		cur_cycle = RELINKCTL_CYCLE_READ(iter->relinkctl_bkptr, iter->cycle_loc);
		if (cur_cycle != iter->cycle)
			return TRUE;
	}
	return FALSE;
}

/* Routine called from zro_search()
 * zro_zhist_saverecent
 * INPUT:
 * 	array of history entries created during zro_search()
 * OUTPUT:
 * 	global variable TREF(recent_zhist), which is then "passed" to incr_link, which associates this history with routine hdr
 */
void zro_zhist_saverecent(zro_validation_entry *zhent, zro_validation_entry *zhent_base)
{
	int		hist_len;
	zro_hist	*lcl_recent_zhist;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Malloc and save zhist copy */
	assert(NULL != zhent);
	assert(NULL != zhent_base);
	hist_len = zhent - zhent_base;
	lcl_recent_zhist = (zro_hist *)malloc(SIZEOF(zro_hist) + SIZEOF(zro_validation_entry) * hist_len);
	lcl_recent_zhist->zroutines_cycle = TREF(set_zroutines_cycle);
	lcl_recent_zhist->end = &lcl_recent_zhist->base[0] + hist_len;
	memcpy((char *)&lcl_recent_zhist->base[0], (char *)zhent_base, SIZEOF(zro_validation_entry) * hist_len);
	TREF(recent_zhist) = lcl_recent_zhist;
}

/*
 * zro_record_zhist
 * INPUT:
 * 	routine name
 * 	$ZROUTINES entry identifier
 * OUTPUT:
 * 	cycle_addr
 * 	current value at cycle_addr
 * 	NOTE: both saved into current history entry, "zhent"
 */
void zro_record_zhist(zro_validation_entry *zhent, zro_ent *obj_container, mstr *rtnname)
{
	open_relinkctl_sgm	*linkctl;
	relinkrec_loc_t		rec;
	int			len;

	assert(NULL != obj_container);
	linkctl = obj_container->relinkctl_sgmaddr;
	assert(NULL != linkctl);
	zhent->relinkctl_bkptr = linkctl;
	assert(NULL != zhent);
	rec = relinkctl_insert_record(linkctl, rtnname);
	zhent->cycle_loc = rec;
	zhent->cycle = RELINKCTL_CYCLE_READ(linkctl, zhent->cycle_loc);
}
#endif /* USHBIN_SUPPORTED */
