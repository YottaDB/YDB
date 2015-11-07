/****************************************************************
 *								*
 * Copyright (c) 2013-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtmlink.h"
#include "error.h"
#include "gtmio.h"

#ifdef AUTORELINK_SUPPORTED /* This entire file */
GBLREF	int	object_file_des;

/* Routine called from auto_relink_check() and op_rhdadd() to determine if the routine being called needs to be relinked.
 * To return TRUE, the following conditions must be met:
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
	uint4			cur_cycle, rtnnmlen;
	boolean_t		norecurslink;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != zhist);		/* Shouldn't be called unless relinking is possible */
	/* If $ZROUTINES cycle has changed since validation list was created, signal relink but the relink only
	 * actually happens if the file op_zlink() locates has a different object hashcode than the currently
	 * linked routine - else it just rebuilds the history and calls it good.
	 */
	if (zhist->zroutines_cycle != TREF(set_zroutines_cycle))
	{	/* Check if we can return TRUE for this routine */
		if ((LINK_NORECURSIVE == TREF(relink_allowed)) && on_stack(rtnhdr, NULL))
			return FALSE;	/* can't relink, or else we'll get LOADRUNNING */
		return TRUE;
	}
	/* Traverse list corresponding to zro entries */
	norecurslink = (LINK_NORECURSIVE == TREF(relink_allowed));
	for (iter = &zhist->base[0]; iter != zhist->end; iter++)
	{
		DEBUG_ONLY(rtnnmlen = mid_len(&iter->relinkrec->rtnname_fixed));
		assert((rtnnmlen == rtnhdr->routine_name.len)	/* Verify have the right entry - compare names */
		       && (0 == memcmp(&iter->relinkrec->rtnname_fixed, rtnhdr->routine_name.addr, rtnnmlen)));
		cur_cycle = iter->relinkrec->cycle;
		if (cur_cycle != iter->cycle)
		{	/* Check if we can return TRUE for this routine */
			if (norecurslink && on_stack(rtnhdr, NULL))
				return FALSE;	/* can't relink, or else we'll get LOADRUNNING */
			return TRUE;
		}
	}
	return FALSE;
}

/* Routine called from zro_search_hist() to copy a given search history block from stack memory to malloc'd memory
 * so it can be attached to the routine header of the linked routine.
 *
 * Parameters:
 *   zhist_valent      - Last search history entry + 1
 *   zhist_valent_base - First search history entry
 *
 * Return value:
 *   Malloc'd block of search history header (zro_hist *) followed by the array of search history entries for
 *   the given routine.
 */
zro_hist *zro_zhist_saverecent(zro_search_hist_ent *zhist_valent_end, zro_search_hist_ent *zhist_valent_base)
{
	int			hist_len;
	relinkrec_t		*rec;
	zro_hist		*lcl_recent_zhist;
	zro_validation_entry	*zhent;
	zro_search_hist_ent	*zhist_valent;
	mstr			rtnname;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Malloc and return zhist copy */
	assert(NULL != zhist_valent_end);
	assert(NULL != zhist_valent_base);
	hist_len = zhist_valent_end - zhist_valent_base;
	lcl_recent_zhist = (zro_hist *)malloc(SIZEOF(zro_hist) + (SIZEOF(zro_validation_entry) * hist_len));
	lcl_recent_zhist->zroutines_cycle = TREF(set_zroutines_cycle);
	lcl_recent_zhist->end = &lcl_recent_zhist->base[0] + hist_len;
	assert(NULL == TREF(save_zhist));
	TREF(save_zhist) = lcl_recent_zhist;
	ESTABLISH_RET(zro_ins_rec_fail_ch, NULL);
	for (zhist_valent = zhist_valent_base, zhent = &lcl_recent_zhist->base[0];
	     0 < hist_len;
	     zhist_valent++, zhent++, hist_len--)
	{
		rtnname.addr = zhist_valent->rtnname.c;
		rtnname.len = zhist_valent->rtnname_len;
		assert(NULL != zhist_valent->zro_valent.relinkctl_bkptr);
		rec = relinkctl_insert_record(zhist_valent->zro_valent.relinkctl_bkptr, &rtnname);
		assert(NULL != rec);
		zhist_valent->zro_valent.relinkrec = rec;
		zhist_valent->zro_valent.cycle = rec->cycle;
		memcpy((char *)zhent, (char *)&zhist_valent->zro_valent, SIZEOF(zro_validation_entry));
	}
	REVERT;
	TREF(save_zhist) = NULL;
	return lcl_recent_zhist;
}

/* Condition handler called when relinkctl_insert_record() fails. We specifically need to (1) release the zro_hist structure
 * we allocated and (2) close the object file before driving the next condition handler.
 */
CONDITION_HANDLER(zro_ins_rec_fail_ch)
{
	int	rc;

	START_CH(TRUE);
	if (NULL != TREF(save_zhist))
	{
		free(TREF(save_zhist));
		TREF(save_zhist) = NULL;
	}
	CLOSE_OBJECT_FILE(object_file_des, rc);		/* Close object file ignoring error (processing primary error) */
	NEXTCH;
}

/* Routine called from zro_search_hist() to add a $ZROUTINES entry to the (local) search history for a given object file.
 *
 * Parameters:
 *   zhist_valent	- $ZROUTINES search history entry to be filled in.
 *   obj_container	- $ZROUTINES entry for a given object directory.
 *   rtnname		- mstr addr containing name of the routine.
 *
 */
void zro_record_zhist(zro_search_hist_ent *zhist_valent, zro_ent *obj_container, mstr *rtnname)
{
	open_relinkctl_sgm	*linkctl;
	int			len;

	assert(NULL != obj_container);
	linkctl = obj_container->relinkctl_sgmaddr;
	assert(NULL != linkctl);
	zhist_valent->zro_valent.relinkctl_bkptr = linkctl;
	assert(NULL != zhist_valent);
	assert(rtnname->len < SIZEOF(mident_fixed));
	memcpy(zhist_valent->rtnname.c, rtnname->addr, rtnname->len);
	zhist_valent->rtnname_len = rtnname->len;
	if (SIZEOF(zhist_valent->rtnname) > rtnname->len)
		memset((zhist_valent->rtnname.c + rtnname->len), '\0', SIZEOF(zhist_valent->rtnname) - rtnname->len);
}
#endif /* AUTORELINK_SUPPORTED */
