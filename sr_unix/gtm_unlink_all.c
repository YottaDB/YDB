/****************************************************************
 *								*
 * Copyright (c) 2011-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "dollar_zlevel.h"
#include "error_trap.h"
#include "golevel.h"
#include "cache.h"
#include "cmd_qlf.h"
#include "hashtab.h"
#include "hashtab_objcode.h"
#include "hashtab_mname.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mprof.h"
#include "gtm_unlink_all.h"
#include "zbreak.h"
#include "gtm_text_alloc.h"
#include "parse_file.h"
#include "zro_shlibs.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "srcline.h"
#include "gtmci.h"	/* for GTM_CIMOD */
#include "dm_setup.h"	/* for GTM_DMOD */
#include "urx.h"
#include "stringpool.h"
#include "zr_unlink_rtn.h"
#ifdef GTM_TRIGGER
#include "gv_trigger.h"
#include "gtm_trigger.h"
#include "gv_trigger_protos.h"
#endif

GBLREF	boolean_t		is_tracing_on;
GBLREF	gv_key			*gv_currkey;
GBLREF	hash_table_objcode	cache_table;
GBLREF	int			dollar_truth;
GBLREF	int			indir_cache_mem_size;
GBLREF	rtn_tabent		*rtn_names, *rtn_names_end, *rtn_names_top, *rtn_fst_table;
GBLREF	stack_frame		*frame_pointer;
GBLREF	gv_namehead		*gv_target_list;

/* Routine to do the following:
 *
 * 1. Stop M-Profiling.
 * 2. Unwind the M stack back to level 1
 * 3. (re)Initialize $ECODE, $REFERENCE and $TEST.
 * 4. Remove all triggers. This includes not only the trigger routines but the trigger definitions and
 *    linkages in the gvt_trigger struct anchored off of the gv_target.
 * 5. Unlink all M routines. This includes getting rid of their linkage entries, breakpoints,
 *    and $TEXT() caches and re-initializing the routine table.
 * 6. Empty the indirect cache.
 * 7. Close the shared libraries associated with M programs and reopen them after the unroll if any
 *    are present in $ZROUTINES.
 *
 * Currently called from op_zgoto() but could be called from elsewhere if needed in the future.
 *
 * Note this routine removes all loaded programs but the intial base-frame on the stack has the address of whatever
 * the first program is (GTM$DMOD in a direct mode situation or the starting program in a -run invocation). It is up
 * to the caller to set the base frame with this address so the stack has no unknown references in it.
 */
void gtm_unlink_all(void)
{
	rtn_tabent	*rtab;
	rhdtyp		*rtnhdr, *rhdr, *next_rhdr;
	ht_ent_mname    *tabent_mname;
	textElem	*telem;
	ht_ent_objcode 	*tabent_obj, *topent;
	cache_entry	*csp;
	mname_entry	key;
	routine_source	*src_tbl;
	gv_namehead	*gvt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Step 1: Stop M-Profiling */
	if (is_tracing_on)
		turn_tracing_off(NULL);
	/* Step 2: Unwind M stack back to level 0 */
	GOLEVEL(0, TRUE);
	assert(0 == dollar_zlevel());
	/* Step 3: re-Initialize $ECODE, $REFERENCE, and $TEST */
	NULLIFY_DOLLAR_ECODE;		/* Clears $ECODE and results returned for $STACK */
	if (NULL != gv_currkey)
	{	/* Clears $REFERENCE */
		gv_currkey->end = 0;
		gv_currkey->base[0] = KEY_DELIMITER;
	}
	dollar_truth = FALSE;		/* aka $TEST */
	/* Step 4: Remove all triggers */
#	ifdef GTM_TRIGGER
	for (gvt = gv_target_list; gvt; gvt = gvt->next_gvnh)
		gvtr_free(gvt);
#	endif
	/* Step 5: Unlink all routines, remove $TEXT cache and remove breakpoints. Note that for the purposes of this section,
	 * there is no difference between normal routines and trigger routines. Both are being removed completely so the code
	 * below is a hodgepodge of code from zlput_rname and gtm_trigger_cleanup(). Note process in reverse order so we can
	 * move rtn_names_end up leaving processed entries (whose keys no longer work) off the end of the table without moving
	 * anything. This is necessary because removing ZBREAK points can call find_rtn_hdr so this table needs to remain in
	 * a usable state while we are doing this.
	 */
	for (rtab = rtn_names_end; rtab > rtn_names; rtab--, rtn_names_end = rtab)
	{	/* [0] is not used (for some reason) */
		rtnhdr = rtab->rt_adr;
		if ((0 == strcmp(rtnhdr->routine_name.addr, GTM_DMOD)) || (0 == strcmp(rtnhdr->routine_name.addr, GTM_CIMOD)))
		{	/* If the routine is GTM$DMOD or GTM$CI, it is allocated in one chunk by make_*mode(). Release it in
			 * one chunk too.
			 */
			GTM_TEXT_FREE(rtnhdr);
		} else
			zr_unlink_rtn(rtnhdr, TRUE);
	}
	/* All programs have been removed. If this is the "first" table allocated which cannot be removed, just reinitialize
	 * the table and we're done. If a new table, release it, recover the first table, initialize and we're done.
	 */
	if (rtn_names != rtn_fst_table)
	{
		free(rtn_names);
		rtn_names = rtn_fst_table;
		assert(NULL != rtn_fst_table);
	}
	memset(rtn_names, 0, SIZEOF(rtn_tabent));
	rtn_names_end = rtn_names_top = rtn_names;
	/* Step 5: Empty the indirect cache */
	for (tabent_obj = cache_table.base, topent = cache_table.top; tabent_obj < topent; tabent_obj++)
	{	/* Run through the hashtable getting rid of all the entries. Not bothering with the cleanups
		 * like are done in cache_table_rebuild since we are going to re-init the table.
		 */
		if (HTENT_VALID_OBJCODE(tabent_obj, cache_entry, csp))
		{
			GTM_TEXT_FREE(csp);
		}
	}
	reinitialize_hashtab_objcode(&cache_table);	/* Completely re-initialize the hash table */
	indir_cache_mem_size = 0;
	/* Step 6: Close all M code shared libraries */
	zro_shlibs_unlink_all();
}
