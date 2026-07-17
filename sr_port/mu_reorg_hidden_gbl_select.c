/****************************************************************
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/***************************************************************************************************************
 * mu_reorg_hidden_gbl_select.c:
 *	Helper for MUPIP REORG (see YDB#1240). The list of global names a REORG operates on is built
 *	by "gv_select", which enumerates names THROUGH the global directory: a name is bound via the gld name
 *	map ("op_gvname_fast"/"op_gvorder") and selected for the region(s) the gld maps it to. A global whose
 *	name exists in a region's directory tree but which the current gld maps to a DIFFERENT region (e.g. the
 *	global was moved to a new region in the gld and killed in the original region, or the region's database
 *	file predates a gld reorganization) is a "hidden" global: "gv_select" never yields a glist entry for
 *	the region that physically holds its blocks. A plain REORG therefore leaves those globals unreorged,
 *	and a REORG -TRUNCATE additionally never moves their blocks (root block included) towards the front of
 *	the file, so a single hidden global whose blocks lie at the end of the file makes "mu_truncate"
 *	truncate nothing (a MUTRUNCALREADY message even though the file has lots of free space).
 *
 *	"mu_reorg_hidden_gbl_select" runs right after "gv_select" when MUPIP REORG (with or without -TRUNCATE)
 *	is invoked without an explicit -SELECT. For each region being processed it walks the region's directory
 *	tree at name level
 *	(the same "gv_target = cs_addrs->dir_tree; gvcst_order()" walk that "op_gvorder" does, minus the gld
 *	name-map filtering) and appends, for every name that "gv_select" did not select for this region, a
 *	glist entry whose gv_target is bound DIRECTLY to the region ("targ_alloc"). Everything downstream of
 *	selection is gld-independent -- DO_OP_GVNAME (see muextr.h) binds a glist entry by looking the name up
 *	in the entry's region's own directory tree -- so the appended entries flow through the reorg phase, the
 *	"mu_swap_root" phase, the -EXCLUDE check and the truncate phase exactly like gld-visible globals do.
 *
 *	Note that killed globals are included naturally: a kill leaves the name in the directory tree (pointing
 *	to an empty root block), and the directory tree is what this walk reads. This matters because the
 *	empty root block of a killed hidden global is the typical truncate blocker (see YDB#1240) and is
 *	invisible to the YDB#1245 tail sweep too (the block's contents no longer name the global it belonged
 *	to; only the directory tree still knows the name).
 **************************************************************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdskill.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"			/* for "tp_set_sgm" prototype (used by CHANGE_REG_IF_NEEDED) */
#include "muextr.h"		/* for glist/GNAME (also pulls in gtmsource.h for CHANGE_REG_IF_NEEDED) */
#include "mu_reorg.h"
#include "hashtab_mname.h"	/* for COMPUTE_HASH_MNAME */
/* Include prototypes */
#include "gvcst_protos.h"	/* for "gvcst_order" prototype */
#include "mupip_reorg.h"
#include "targ_alloc.h"

GBLREF	bool		mu_ctrlc_occurred;
GBLREF	bool		mu_ctrly_occurred;
GBLREF	gd_addr		*gd_header;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey, *gv_altkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	tp_region	*grlist;

/* Append to "gl_head" one glist entry per <name, region> pair such that "name" exists in the directory tree of
 * "region"'s database file but no entry for that pair is already in the list (i.e. the preceding "gv_select"
 * call did not select it, making it a hidden global; see comment at the top of this file). "restrict_reg"
 * mirrors the same-named "gv_select" parameter: if TRUE, only the regions in "grlist" (built by "mu_getlst"
 * from the -REGION qualifier) are looked at, else all regions of the gld.
 */
void mu_reorg_hidden_gbl_select(glist *gl_head, boolean_t restrict_reg)
{
	boolean_t	found;
	gd_region	*reg, *r_top;
	glist		*gl_ptr, *gl_tail, *new_gl;
	int		name_len;
	mname_entry	gvname;
	tp_region	*rptr;

	assert(NULL != gd_header);	/* the "gv_select" call preceding us initialized it */
	for (gl_tail = gl_head; NULL != gl_tail->next; gl_tail = gl_tail->next)
		;
	for (reg = gd_header->regions, r_top = reg + gd_header->n_regions; reg < r_top; reg++)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			break;
		if (restrict_reg)
		{	/* -REGION was specified: only look at the specified regions (same check as "gv_select_reg") */
			for (rptr = grlist; NULL != rptr; rptr = rptr->fPtr)
				if (reg == rptr->reg)
					break;
			if (NULL == rptr)
				continue;
		}
		if (IS_STATSDB_REGNAME(reg))
			continue;	/* statsDB regions hold only ^%YGS; nothing hidden to select */
		if (!IS_REG_BG_OR_MM(reg))
			continue;
		if (!reg->open && IS_AUTODB_REG(reg))
			continue;	/* do not create an AUTODB database file just to find its directory tree empty */
		if (!reg->open)
			gv_init_reg(reg);	/* region may hold ONLY hidden globals, in which case "gv_select"
						 * (whose region opens happen through gld name binds) never opened it
						 */
		CHANGE_REG_IF_NEEDED(reg);
		/* Seed gv_currkey to just below the lowest possible global name ("%") so the first "gvcst_order"
		 * call below returns the very first name in this region's directory tree. Note that this seed
		 * deliberately skips the names of trigger globals like ^#t ('#' sorts before '%'): the -TRUNCATE
		 * code path in "mupip_reorg" processes ^#t in each to-be-truncated region explicitly.
		 */
		gv_currkey->end = 2;
		gv_currkey->prev = 0;
		gv_currkey->base[0] = '%';
		gv_currkey->base[1] = KEY_DELIMITER;
		gv_currkey->base[2] = KEY_DELIMITER;
		GVKEY_DECREMENT_ORDER(gv_currkey);
		for (;;)
		{
			if (mu_ctrly_occurred || mu_ctrlc_occurred)
				break;
			gv_target = cs_addrs->dir_tree;
			found = gvcst_order();
			if (!found)
				break;	/* walked past the last name in this region's directory tree */
			/* gv_altkey now holds the found name (see "op_gvorder", whose directory tree walk this is) */
			assert(1 < gv_altkey->end);
			assert((MAX_MIDENT_LEN + 2) > gv_altkey->end);	/* until names are not in midents */
			name_len = gv_altkey->end - 1;
			for (gl_ptr = gl_head->next; NULL != gl_ptr; gl_ptr = gl_ptr->next)
			{
				if ((reg == gl_ptr->reg) && (name_len == GNAME(gl_ptr).len)
						&& (0 == memcmp(GNAME(gl_ptr).addr, gv_altkey->base, name_len)))
					break;
			}
			if (NULL == gl_ptr)
			{	/* The name is in this region's directory tree but "gv_select" did not select it for
				 * this region: a hidden global. Append a glist entry whose gv_target is bound directly
				 * to this region. The entry's root block is located later by DO_OP_GVNAME's region-local
				 * root search (which reads this region's directory tree, not the gld), so a name whose
				 * global was killed (empty GVT) is processed too, same as "gv_select" achieves for
				 * gld-visible globals via TREF(want_empty_gvts).
				 */
				gvname.var_name.addr = (char *)gv_altkey->base;
				gvname.var_name.len = name_len;
				COMPUTE_HASH_MNAME(&gvname);
				new_gl = (glist *)malloc(SIZEOF(glist));
				new_gl->next = NULL;
				new_gl->reg = reg;
				new_gl->gvt = targ_alloc(reg->max_key_size, &gvname, reg);	/* copies the name */
				new_gl->gvnh_reg = NULL;	/* same shape as the glist entries "mu_trunc_tail_sweep"
								 * builds; nothing in the REORG code paths reads it
								 */
				gl_tail->next = new_gl;
				gl_tail = new_gl;
			}
			/* Set up gv_currkey to $order past the name just processed */
			assert(KEY_DELIMITER == gv_altkey->base[gv_altkey->end - 1]);
			assert(KEY_DELIMITER == gv_altkey->base[gv_altkey->end]);
			memcpy(gv_currkey->base, gv_altkey->base, gv_altkey->end + 1);
			gv_currkey->end = gv_altkey->end;
			GVKEY_INCREMENT_ORDER(gv_currkey);
		}
	}
	/* Do not leave gv_target/gv_currkey pointing into the last walked region's directory tree: the caller is
	 * about to run reorg phases that set up their own gv_target/gv_currkey via DO_OP_GVNAME, and a stale
	 * <gv_target, gv_currkey> pair from here could otherwise fail the DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC
	 * check (in "dbg_check_gvtarget_gvcurrkey_in_sync" in gvt_inline.h) in whatever global reference happens
	 * next. Same cleanup as at the end of "mu_trunc_tail_sweep".
	 */
	gv_target = NULL;
	gv_currkey->end = 0;
	gv_currkey->base[0] = KEY_DELIMITER;
	return;
}
