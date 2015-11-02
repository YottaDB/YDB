/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "rtnhdr.h"
#include "stack_frame.h"
#include "op.h"
#include "hashtab_addr.h"
#include "hashtab_mname.h"
#include "hashtab.h"
#include "stp_parms.h"
#include "lv_val.h"
#include "error.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "sbs_blk.h"
#include "mv_stent.h"
#include "alias.h"
#include "gtm_malloc.h"
#include "stringpool.h"
#include "mmemory.h"

/* Define macros locally used by this routine only. General use macros are defined
   in alias.h
*/

/* Macro to decrement a base var reference. This lightweight version used by
   als_check_xnew_var_aliases() is used to bypass the delete and requeue stuff and
   do only reference count maint since the entire symtab and all its lv_vals
   (lv_blks) are going to be released shortly.
 */
#define DECR_BASE_REF_LIGHT(lvp)								\
	{	/* Perform reference count maintenance for base var */				\
		lv_sbs_tbl *tmpsbs;								\
		assert(MV_SYM == (lvp)->ptrs.val_ent.parent.sym->ident);			\
		assert(0 < (lvp)->stats.trefcnt);						\
		DECR_TREFCNT(lvp);								\
		assert((lvp)->stats.trefcnt >= (lvp)->stats.crefcnt);				\
		if (0 == (lvp)->stats.trefcnt)							\
		{										\
			(lvp)->v.mvtype = 0;							\
			if (tmpsbs = (lvp)->ptrs.val_ent.children)	/* Note assignment */	\
			{									\
				(lvp)->ptrs.val_ent.children = NULL;				\
				als_xnew_killaliasarray(tmpsbs);				\
			}									\
		}										\
	}


/* Macro to decrement an alias container reference. This lightweight version used by
   als_check_xnew_var_aliases() is used to bypass the delete and requeue stuff and
   do only reference count maint since the entire symtab and all its lv_vals
   (lv_blks) are going to be released shortly. Note this macro is known to only
   be used on subscripted nodes hence the additional initial assert for MV_SBS.
*/
#define DECR_AC_REF_LIGHT(lvp)										\
	{												\
		assert(MV_SBS == (lvp)->ptrs.val_ent.parent.sbs->ident);				\
		if (MV_ALIASCONT & (lvp)->v.mvtype)							\
		{	/* Killing an alias container, perform reference count maintenance */ 		\
			lv_val	*lvref = (lv_val *)(lvp)->v.str.addr;					\
			assert(lvref);									\
			assert(MV_SYM == lvref->ptrs.val_ent.parent.sym->ident); 			\
			assert(0 == (lvp)->v.str.len);							\
			assert(0 < lvref->stats.crefcnt);						\
			assert(0 < lvref->stats.trefcnt);						\
			DECR_CREFCNT(lvref);								\
			if (lvref->ptrs.val_ent.parent.sym == (lvp)->ptrs.val_ent.parent.sbs->sym)	\
			{	/* pointed to lvval owned by popd symval, use light flavor */		\
				DECR_BASE_REF_LIGHT(lvref);						\
			} else										\
			{	/* pointed to lvval owned by other symval, use full flavor */		\
				DECR_BASE_REF_NOSYM(lvref, TRUE);					\
			}										\
		}											\
	}

/* Macro to mark an lv_val as reachable and process its descendants if any */
#define MARK_REACHABLE(lvp) 										\
	{												\
		assert((lvp));										\
		/* Since this macro can be called in cases where the lv_val is NOT valid, such as in	\
		   the case an MVST_PVAL  mv_stent entry with an mvs_val entry that has been deleted	\
		   by alias reassignment (see unw_mv_ent), we need to verify we have an actual lv_val	\
		   by the same methods used to build the lv_val list and only then check if this	\
		   lv_val has been processed yet.							\
		*/											\
		if ((MV_SBS != (lvp)->v.mvtype && NULL != (lvp)->ptrs.val_ent.parent.sym)		\
		    && (MV_SYM == (lvp)->ptrs.val_ent.parent.sym->ident)				\
		    && ((lvp)->stats.lvtaskcycle != lvtaskcycle))					\
		{	/* This lv_val has not been processed yet */					\
			DBGRFCT((stderr, "\nMARK_REACHABLE: Marking lv 0x"lvaddr" as reachable\n",	\
				 (lvp)));								\
			(lvp)->stats.lvtaskcycle = lvtaskcycle;		/* Mark it */ 			\
			if ((lvp)->ptrs.val_ent.children PRO_ONLY(&& (lvp)->has_aliascont)) 		\
			{	/* And it has descendents to process */					\
				DBGRFCT((stderr, "MARK_REACHABLE: Scanning same lv for containers\n"));	\
				als_scan_for_containers((lvp), &als_prcs_markreached_cntnr_node,	\
							(void *)NULL, (void *)NULL, (int *)NULL);	\
			}										\
		}											\
	}

/* Macro to clone an lv_val */
#define CLONE_LVVAL(oldlv, newlv, cursymval)					\
	assert(MV_SYM == oldlv->ptrs.val_ent.parent.sym->ident);		\
	newlv = lv_getslot(cursymval);						\
	*newlv = *oldlv;							\
	newlv->ptrs.val_ent.parent.sym = cursymval;				\
	lv_var_clone(newlv);							\
	oldlv->v.mvtype = MV_LVCOPIED;						\
	oldlv->ptrs.copy_loc.newtablv = newlv;

/* Macro to initialize a ZWR_ZAV_BLK structure */
#define ZAV_BLK_INIT(zavb, zavbnext)											\
	(zavb)->next = (zavbnext);											\
	(zavb)->zav_base = (zavb)->zav_free = (zwr_alias_var *)((char *)(zavb) + SIZEOF(zwr_zav_blk));			\
	(zavb)->zav_top = (zwr_alias_var *)((char *)(zavb)->zav_base + (SIZEOF(zwr_alias_var) * ZWR_ZAV_BLK_CNT));

/* Macro to run a given tree looking for container vars. Process what they point to in order to make sure what they point to
   doesn't live in the symbol tree being popped. If so, move to the current tree (copying if necessary). If what is being pointed
   to was not passed through then it will not be put into the symbol table but will instead just be data pointed to by the
   container var.
*/
#define RESOLV_ALIAS_CNTNRS_IN_TREE(lv_base, popdsymval, cursymval)								\
	if ((lv_base)->ptrs.val_ent.children && (lv_base)->stats.lvtaskcycle != lvtaskcycle 					\
	    PRO_ONLY(&& (lv_base)->has_aliascont))										\
	{															\
		(lv_base)->stats.lvtaskcycle = lvtaskcycle;									\
		als_scan_for_containers(lv_base, &als_prcs_xnew_alias_cntnr_node, (void *)popdsymval, (void *)cursymval,	\
					(int *)NULL);										\
	}


GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;
GBLREF unsigned char	*msp, *stackbase, *stacktop, *stackwarn;
GBLREF mv_stent		*mv_chain;
GBLREF tp_frame		*tp_pointer;
GBLREF zwr_hash_table	*zwrhtab;
GBLREF trans_num	local_tn;					/* transaction number for THIS PROCESS */
GBLREF uint4		tstartcycle;
GBLREF uint4		lvtaskcycle;					/* lv_val cycle for misc lv_val related tasks */
GBLREF mstr		**stp_array;
GBLREF int		stp_array_size;
GBLREF lv_val		*zsrch_var, *zsrch_dir1, *zsrch_dir2;
GBLREF tp_frame		*tp_pointer;
GBLREF int4		SPGC_since_LVGC;				/* stringpool GCs since the last dead-data GC */
GBLREF short		dollar_trestart;
GBLREF boolean_t	suspend_lvgcol;
GBLREF lv_xnew_var	*xnewvar_anchor;
GBLREF lv_xnew_ref	*xnewref_anchor;
GBLREF mval		*alias_retarg;

LITREF mname_entry	null_mname_entry;

/* Local routines -- not made static so they show up in pro core stack traces */
void als_xnew_killaliasarray(lv_sbs_tbl *stbl);
void als_prcs_xnew_alias_cntnr_node(lv_val *lv, void *popdsymvalv, void *cursymvalv);
void als_prcs_markreached_cntnr_node(lv_val *lv, void *dummy1, void *dummy2);

CONDITION_HANDLER(als_check_xnew_var_aliases_ch);


/***************************************************************************************/

/* Routine to repair the l_symtab entries in the stack due to hash table expansion such that the
   l_symtab entries no longer point to valid hash table entries.

   Note that the "repair" done by this routine depends on the special processing done in
   expand_hashtab_mname (EXPAND_HASHTAB rtn in hashtab_implementation.h) which does not free the
   old table and places the addresses of the new hash table entries in the the value of the old
   hash table entries. This allows this routine to access the old table with the existing
   l_symtab entries and pull the new values that should be put in the respective l_symtab
   entries before it completes the cleanup and releases the old hash table.

   Operation - Loop through the stack and:

   1) For each unique l_symtab, run through the entries in the l_symtab.
   2) If entry is null, skip.
   3) If entry falls within range of the old symbol table, load the address in it and verify
      that it falls within the range of the new symbol table.
   4) If the entry does not fall within the range of the old symtab:
      a) Stop the search as we must have run into an older symtab
      b) If debug, assert fail if this is not the first_symbol in this l_symtab we have seen
         since an l_symtab can only point to one symtab).
   5) Get new entry address from within the old entry.
   6) Debug only: Assert fail if the new entry address not in range of new symtab.
   7) Note that after procesing the stack to get to the l_symtab entries, we also process the
      mv_stent types that contain hash table entry pointers and have to be processed in the same
      fashion as the l_symtab entries. This processing saves us the hashtable lookup necessary to
      pop NEW'd or parameter values when undoing a stack level and restoring previous values.
*/
void als_lsymtab_repair(hash_table_mname *table, ht_ent_mname *table_base_orig, int table_size_orig)
{
	int			htcnt;
	boolean_t		done;
	mv_stent 		*mv_st_ent;
	ht_ent_mname		*table_top_orig, **last_lsym_hte, **htep, *htenew;
	stack_frame		*fp, *fpprev;
	DEBUG_ONLY(boolean_t	first_sym;)

	assert(table);
	assert(table_base_orig);
	assert(table_base_orig != curr_symval->h_symtab.base);
	table_top_orig = table_base_orig + table_size_orig;
	last_lsym_hte = NULL;
	done = FALSE;
	fp = frame_pointer;
	assert(frame_pointer);
	do
	{	/* Once through for each stackframe using the same symbol table. Note this loop is similar
		   to the stack frame loop in op_clralsvars.c.
		*/
		if (fp->l_symtab != last_lsym_hte)
		{	/* Different l_symtab than last time (don't want to update twice) */
			last_lsym_hte = fp->l_symtab;
			if (htcnt = fp->vartab_len)	/* Note assignment */
			{	/* Only process non-zero length l_symtabs */
				DEBUG_ONLY(first_sym = TRUE);
				for (htep = fp->l_symtab; htcnt; --htcnt, ++htep)
				{
					if (NULL == *htep)
						continue;
					if (*htep < table_base_orig || *htep >= table_top_orig)
					{	/* Entry doesn't point to the current symbol table */
						assert(first_sym);
						done = TRUE;
						break;
					}
					htenew = (ht_ent_mname *)((*htep)->value);	/* Pick up entry we should now use */
					assert(htenew >= table->base && htenew < (table->base + table->size));
					*htep = htenew;
					DEBUG_ONLY(first_sym = FALSE);
				}
			}
		}
		fpprev = fp;
		fp = fp->old_frame_pointer;
		if (done)
			break;
		if (SFF_CI & fpprev->flags)
		{	/* Callins needs to be able to crawl past apparent end of stack to earlier stack segments */
			/* We should be in the base frame now. See if an earlier frame exists */
			/* Note we don't worry about trigger base frames here because triggers *always* have a
			   different symbol table - previous symbol tables and stack levels are not affected
			*/
			fp = *(stack_frame **)(fp + 1);	/* Backups up to "prev pointer" created by base_frame() */
			if (NULL == fp || fp >= (stack_frame *)stackbase || fp < (stack_frame *)stacktop)
				break;	/* Pointer not within the stack -- must be earliest occurence */
		}
	} while(fp);

	/* Next, check the mv_stents for the stackframes we processed. Certain mv_stents also have hash
	   table references in them that need repair.
	*/
	for (mv_st_ent = mv_chain;
	     mv_st_ent < (mv_stent *)(fp ? fp : fpprev);	/* Last stack frame actually processed */
	     mv_st_ent = (mv_stent *)(mv_st_ent->mv_st_next + (char *)mv_st_ent))
	{
		switch (mv_st_ent->mv_st_type)
		{	/* The types processed here contain hash table pointers that need to be modified */
			case MVST_NTAB:
				htep = &mv_st_ent->mv_st_cont.mvs_ntab.hte_addr;
				break;
			case MVST_PVAL:
				htep = &mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.hte_addr;
				break;
			case MVST_NVAL:
				htep = &mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.hte_addr;
				break;
			default:
				continue;
		}
		if (NULL == *htep)
			continue;
		if (*htep < table_base_orig || *htep >= table_top_orig)
			/* Entry doesn't point to the current symbol table so ignore it since it didn't change */
			continue;
		htenew = (ht_ent_mname *)((*htep)->value);	/* Pick up entry we should now use */
		assert(htenew >= table->base && htenew < (table->base + table->size));
		*htep = htenew;
	}
	/* For debug at least make unusable in case any stragglers point to it -- even though we are somewhat duplicating
	   the SmInitAlloc gtmdgblvl flag here, this is so critical for debugging we want to do this even when general
	   checking is not being done. SE 09/2008
	*/
	DEBUG_ONLY(memset(table_base_orig, 0xfe, table_size_orig * SIZEOF(ht_ent_mname)));
	free(table_base_orig);
}


/* Local routine! condition handler whose sole function is to turn off the flag that says we are in
   als_check_xnew_var_alias()..
*/
CONDITION_HANDLER(als_check_xnew_var_aliases_ch)
{
	START_CH;
	suspend_lvgcol = FALSE;
	NEXTCH;
}


/* When an xNEW'd symtab is popped, and there are alias concerns, this routine is called to make things right. Things that
   can be wrong:
   1) If a variable was passed through to the new symtab and then was aliased to a var that belonged to the new symbol table,
      the lv_val in the old symtab was released and a new one assigned in the new symbol table. We have to:
      a) copy that value back to the previous symtab and
      b) we have to fix the reference count since the alias owned by the new symtab is going away.
   2) If a variable is passed through to the new symtab and within that variable an alias container is created that points
      to a var in the newer symtab we need to copy that var/array back to the older symtab so the value remains.
   3) This gets more interesting to deal with when that var is also aliased by a passed through variable (combining issues 1 & 2).

   Operation:
   1) When the new symtab was created by op_xnew, if there were variables that were passed through, they are recorded in the
      symtab chained from xnew_var_list.
   2) Go through that list of variables, lookup each in the popped symbol table and change the hash table entries to deleted
      so those entries are not found in subsequent scans.
   3) Do a more simplistic kill-alias-all in the popped symtab. This involves the following:
      a) Run the hash table looking for aliased variables. If found, do the reference count maintenance but don't worry about
         deleting any data since it will all go away after we return and unw_mv_ent() releases the symbol table and lv_blk chains.
      b) While running the hash table, run any variable trees we find anchored.
   4) Go through the list of forwarded vars again (xnew_var_list) in the popped symval and see if any of the lv_vals are owned
      by the symval being popped. If they are, then the lv_vals involved need to be copied to lv_vals owned by the (now)
      current symtab because the popped symtab's lv_vals will be released by unw_mv_ent() when we return. Note that this also
      involves going through the tree of these vars in case any container vars point to an array that is not being dealt with
      in some fashion by one of the other vars we passed through. We avoid processing arrays multiple times by marking them
      with an incremented lvtaskval.
   5) Go through the list of referenced lv_vals (via containers of xnew_var_list vars) by traversing the xnew_ref_list if it
      exists. These vars were recorded because we may not be able to get to them still just by traversing the reference list vars
      so they were pre-recorded so we could scan the worst case of vars that could have containers pointing to the symtab about
      to be popped.
   6) If a pending alias return value exists, check it to see if it also needs to be processed.

      Note: to prevent re-scanning of already scanned array, this routine uses the lvtaskcycle value. To do this, we use the
      suspend_lvgcol global variable to tell stp_gcol() to not do LVGC processing (which also uses lvtaskcycle).
*/
void als_check_xnew_var_aliases(symval *popdsymval, symval *cursymval)
{
	lv_xnew_var		*xnewvar, *xnewvar_next;
	lv_xnew_ref		*xnewref, *xnewref_next;
	ht_ent_mname    	*tabent;
	hash_table_mname	*popdsymtab, *cursymtab;
	ht_ent_mname		*htep, *htep_top;
	lv_val			*lv, *prevlv, *currlv, *popdlv;
	lv_val			*newlv, *oldlv;
	boolean_t		bypass_lvscan, bypass_lvrepl;
	DBGRFCT_ONLY(mident_fixed vname;)

	ESTABLISH(als_check_xnew_var_aliases_ch);
	suspend_lvgcol = TRUE;
	assert(NULL != popdsymval);
	assert(NULL != cursymval);
	assert((NULL != popdsymval->xnew_var_list) || (NULL != alias_retarg));

	DBGRFCT((stderr, "\nals_check_xnew_var_aliases: Beginning xvar pop processing\n"));
	/* Step 2: (step 1 done in op_xnew()) - Run the list of vars that were passed through the xnew and remove them
	   from the popped hash table so they can not be found by the step 3 scan below - meaning we won't mess with the
	   reference counts of these entries but we will record their locations so we can process them in step 4.
	*/
	popdsymtab = &popdsymval->h_symtab;
	cursymtab = &cursymval->h_symtab;
	for (xnewvar = popdsymval->xnew_var_list; xnewvar; xnewvar = xnewvar->next)
	{
		tabent = lookup_hashtab_mname(popdsymtab, &xnewvar->key);
		assert(tabent);
		xnewvar->lvval = (lv_val *)tabent->value;	/* Cache lookup results for 2nd pass in step 4 */
		DELETE_HTENT(popdsymtab, tabent);
	}
	/* Step 3: Run popped hash table undoing alias references. */
	DBGRFCT((stderr, "als_check_xnew_var_aliases: Step 3 - running popped symtab tree undoing local refcounts\n"));
	for (htep = popdsymtab->base, htep_top = popdsymtab->top; htep < htep_top; htep++)
	{
		if (HTENT_VALID_MNAME(htep, lv_val, lv))
			DECR_BASE_REF_LIGHT(lv);
	}

	/* Step 4: See what, if anything, needs to be copied from popped level to current level. There are 3 possible
	   cases here. Note in all cases, we must decrement the use counters of prevlv since they were incremented in
	   op_xnew to keep things from disappearing prematurely (we don't want the LVs we saved to be scrapped and re-used
	   so we make sure they stay around).

	   Vars used:

	     prevlv == lv from the current symbol table.
	     currlv == lv we are going to eventually put into the current symbol table
	     popdlv == lv from the popped symbol table

	   Cases follow:

	      Condition						Action

	   a) prevlv == popdlv					Scan prevlv for alias containers pointing to popped symtab.
	   b) prevlv != popdlv && popdlv in popd symtab.	Clone popdlv into currlv & do alias container scan.
	   c) prevlv != popdlv && popdlv not in popd symtab.	Same as case (a). Note this includes the case where popdlv
	                                                        already resides in cursymtab.
	 */
	DBGRFCT((stderr, "als_check_xnew_var_aliases: Step 4 - beginning unwind scan of passed through vars\n"));
	INCR_LVTASKCYCLE;
	for (xnewvar = popdsymval->xnew_var_list; xnewvar; xnewvar = xnewvar_next)
	{
		bypass_lvscan = bypass_lvrepl = FALSE;
		tabent = lookup_hashtab_mname(cursymtab, &xnewvar->key);
		assert(tabent);				/* Had better be there since it was passed in thru the exclusive new */
		DBGRFCT_ONLY(
			memcpy(vname.c, tabent->key.var_name.addr, tabent->key.var_name.len);
			vname.c[tabent->key.var_name.len] = '\0';
		);
		prevlv = (lv_val *)tabent->value;
		popdlv = xnewvar->lvval;		/* Value of this var in popped symtab */
		DBGRFCT((stderr, "als_check_xnew_var_aliases: var '%s' prevlv: 0x"lvaddr" popdlv: 0x"lvaddr"\n",
			 &vname.c, prevlv, popdlv));
		if (prevlv == popdlv)
		{	/* Case (a) - Just do the scan below */
			currlv = prevlv;
			bypass_lvrepl = TRUE;
		} else if (popdsymval == popdlv->ptrs.val_ent.parent.sym)
		{	/* Case (b) - Clone the var and tree into blocks owned by current symtab with the caveat that we need not
			   do this if the array has already been cloned (because more than one var that was passed through it
			   pointing to it.
			*/
			if (MV_LVCOPIED == popdlv->v.mvtype)
			{	/* This lv_val has been copied over already so use that pointer instead to put into the
				   current hash table.
				*/
				currlv = popdlv->ptrs.copy_loc.newtablv;
				assert(currlv->ptrs.val_ent.parent.sym == cursymval);
				bypass_lvscan = TRUE;	/* lv_val would have already been scanned */
				DBGRFCT((stderr, "als_check_xnew_var_aliases: lv already copied so setting currlv to 0x"lvaddr"\n",
					 currlv));
			} else
			{
				assert(MV_SYM == popdlv->ptrs.val_ent.parent.sym->ident);
				/* lv_val is owned by the popped symtab .. clone it to the new current tree */
				CLONE_LVVAL(popdlv, currlv, cursymval);
				DBGRFCT((stderr, "als_check_xnew_var_aliases: lv has been cloned from 0x"lvaddr" to 0x"lvaddr"\n",
					 popdlv, currlv));
			}
		} else
		{	/* Case (c) - same as (a) except we do replace the lv in cursymtab */
			assert(MV_SYM == popdlv->ptrs.val_ent.parent.sym->ident);
			currlv = popdlv;
		}
		if (!bypass_lvscan)
		{	/* Need to run this tree (if any) to look for container vars buried within */
			DBGRFCT((stderr, "als_check_xnew_var_aliases: potentially scanning lv 0x"lvaddr"\n", currlv));
			RESOLV_ALIAS_CNTNRS_IN_TREE(currlv, popdsymval, cursymval);
		}
		if (1 < prevlv->stats.trefcnt)
			/* If prevlv is going to be around after we drop op_xnew's refcnt bumps, make sure it gets processed.
			   If it was processed above, then it is marked such and the macro will bypass processing it again.
			*/
			RESOLV_ALIAS_CNTNRS_IN_TREE(prevlv, popdsymval, cursymval);
		DECR_CREFCNT(prevlv);		/* undo bump by op_xnew */
		if (!bypass_lvrepl)
		{	/* Replace the lvval in the current symbol table */
			DBGRFCT((stderr, "als_check_xnew_var_aliases: Resetting variable '%s' hte at 0x"lvaddr" from 0x"lvaddr
				 " to 0x"lvaddr"\n", &vname.c, tabent, prevlv, currlv));
			tabent->value = (void *)currlv;
		}
		assert(1 <= prevlv->stats.trefcnt);	/* verify op_xnew's bump is still there (may be only one) */
		DECR_BASE_REF_NOSYM(prevlv, TRUE);	/* undo bump by op_xnew */
		xnewvar_next = xnewvar->next;
		xnewvar->next = xnewvar_anchor;
		xnewvar_anchor = xnewvar;
	}
	/* Step 5: See if anything on the xnew_ref_list needs to be handled */
	DBGRFCT((stderr, "als_check_xnew_var_aliases: Step 5: Process xnew_ref_list if any\n"));
	for (xnewref = popdsymval->xnew_ref_list; xnewref; xnewref = xnewref_next)
	{
		prevlv = xnewref->lvval;
		DBGRFCT((stderr, "als_check_xnew_var_aliases:  xnewref-prevlv: 0x"lvaddr"\n", prevlv));
		DECR_CREFCNT(prevlv);		/* Will remove the trefcnt in final desposition below */
		/* Only do the scan if the reference count is greater than 1 since we are going to remove the
		   refcnts added by op_xnew as we finish here. So if the var is going away anyway, no need
		   to scan.
		*/
		if (1 < prevlv->stats.trefcnt)
		{	/* Process the array */
			DBGRFCT((stderr, "als_check_xnew_var_aliases: potentially scanning lv 0x"lvaddr"\n", prevlv));
			RESOLV_ALIAS_CNTNRS_IN_TREE(prevlv, popdsymval, cursymval);
		} else
			DBGRFCT((stderr, "als_check_xnew_var_aliases: prevlv was deleted\n"));
		/* Remove refcnt and we are done */
		DECR_BASE_REF_NOSYM(prevlv, TRUE);
		xnewref_next = xnewref->next;
		xnewref->next = xnewref_anchor;
		xnewref_anchor = xnewref;
	}
	/* Step 6: Check if a pending alias return value exists and if so if it needs to be processed.
	 *         This type of value is created by unw_retarg() as the result of a "QUIT *" statement. It is an alias container
	 *	   mval that lives in the compiler temps of the caller with a pointer in an mv_stent of the callee in the mvs_parm
	 *	   block allocated by push_parm. Since this mval-container is just an mval and not an lv_val, we have to largely
	 *         do similar processing to the als_prcs_xnew_alias_cntnr_node() with this block type difference in mind.
	 */
	if (NULL != alias_retarg)
	{
		assert(0 != (MV_ALIASCONT & alias_retarg->mvtype));
		oldlv = (lv_val *)alias_retarg->str.addr;
		if (MV_LVCOPIED == oldlv->v.mvtype)
		{	/* This lv_val has been copied over already so use that pointer instead */
			newlv = oldlv->ptrs.copy_loc.newtablv;
			alias_retarg->str.addr = (char *)newlv;			/* Replace container ptr */
			DBGRFCT((stderr, "\nals_check_xnew_var_aliases: alias retarg var found - referenced array already copied"
				 " - Setting pointer in aliascont mval 0x"lvaddr" to lv 0x"lvaddr"\n", alias_retarg, newlv));
		} else
		{
			assert(MV_SYM == oldlv->ptrs.val_ent.parent.sym->ident);
			if (popdsymval == oldlv->ptrs.val_ent.parent.sym)
			{	/* lv_val is owned by the popped symtab .. clone it to the new current tree */
				CLONE_LVVAL(oldlv, newlv, cursymval);
				alias_retarg->str.addr = (char *)newlv;		/* Replace container ptr */
				DBGRFCT((stderr, "\nals_check_xnew_var_aliases: alias retarg var found - aliascont mval 0x"lvaddr
					 " being reset to point to lv 0x"lvaddr" which is a clone of lv 0x"lvaddr"\n",
					 alias_retarg, newlv, oldlv));
			} else
			{	/* lv_val is owned by current or older symval .. just use it in the subsequent scan in case it
				 * leads us to other lv_vals owned by the popped symtab.
				 */
				DBGRFCT((stderr, "\nals_check_xnew_var_aliases: alias retarg var found - aliascont mval 0x"lvaddr
				 " just being (potentially) scanned for container vars\n", alias_retarg));
				newlv = oldlv;
			}
			RESOLV_ALIAS_CNTNRS_IN_TREE(newlv, popdsymval, cursymval);
		}
	}
	DBGRFCT((stderr, "als_check_xnew_var_aliases: Completed xvar pop processing\n"));
	suspend_lvgcol = FALSE;
	REVERT;
}


/* Local routine!
   This routine is basically a lightweight lv_killarray() that goes through a given tree looking for container vars and performing
   the necessary reference count cleanup on what it points to but won't go through the bother of deleting the data since that will
   be taken care of in the ensuing cleanup by unw_mv_ent().
*/
void als_xnew_killaliasarray(lv_sbs_tbl *stbl)
{
	lv_sbs_tbl	*tmpsbs;
	sbs_blk		*sblk;
	sbs_flt_struct	*sbsflt;
	sbs_str_struct	*sbsstr;
	int		i;
	lv_val		*lv;
	char		*top;

	if (stbl->num)
	{	/* We have some numeric subcripts */
		if (stbl->int_flag)
		{	/* they are integer subscripts within the direct subscript range */
			sblk = stbl->num;
 	 	   	for (i = 0; i < SBS_NUM_INT_ELE; i++)
       	       	       	{
				if (lv = sblk->ptr.lv[i])
				{
					assert(lv);
					if (tmpsbs = lv->ptrs.val_ent.children)	/* Note assignment */
					{
						lv->ptrs.val_ent.children = NULL;
						als_xnew_killaliasarray(tmpsbs);
					}
					DECR_AC_REF_LIGHT(lv);		/* Decrement alias contain ref and pseudo kill data */
				}
			}
			stbl->int_flag = FALSE;
		} else
		{
			for (sblk = stbl->num; sblk; sblk = sblk->nxt)
		 	{
				for (sbsflt = &sblk->ptr.sbs_flt[0], top = (char *)&sblk->ptr.sbs_flt[sblk->cnt];
				     sbsflt < (sbs_flt_struct *)top; sbsflt++)
		 	 	{
					lv = sbsflt->lv;
					assert(lv);
					if (tmpsbs = lv->ptrs.val_ent.children)	/* Note assignment */
					{
						lv->ptrs.val_ent.children = NULL;
						als_xnew_killaliasarray(tmpsbs);
					}
					DECR_AC_REF_LIGHT(lv);		/* Decrement alias contain ref and pseudo kill data */
		 		}
	 	 	}
		}
		stbl->num = NULL;
	}
	if (stbl->str)
	{	/* We have some string subscripts */
		for (sblk = stbl->str; sblk; sblk = sblk->nxt)
	 	{
			for (sbsstr = &sblk->ptr.sbs_str[0], top = (char *)&sblk->ptr.sbs_str[sblk->cnt];
			     sbsstr < (sbs_str_struct *)top; sbsstr++)
	 	 	{
				lv = sbsstr->lv;
				assert(lv);
				if (tmpsbs = lv->ptrs.val_ent.children)	/* Note assignment */
				{
					lv->ptrs.val_ent.children = NULL;
					als_xnew_killaliasarray(tmpsbs);
				}
				DECR_AC_REF_LIGHT(lv);		/* Decrement alias contain ref and pseudo kill data */
	 		}
	 	}
	 	stbl->str = NULL;
	}
}


/* Routine to scan the supplied tree for container vars and invoke the supplied routine and args. Note that in debug mode,
   even if "has_aliascont" is NOT set, we will still scan the array for containers but if one is found, then we will assert fail.
   Note this means the processing is different for PRO and DBG builds in that this routine will not even be called in PRO if
   the has_aliascont flag is is not on in the base mval. But this allows us to check the integrity of the has_aliascont flag
   in DBG because we will fail if ever a container is found in an array with the flag turned off.
*/
void als_scan_for_containers(lv_val *lv_base, void (*als_container_processor)(lv_val *, void *, void *), void *arg1, void *arg2,
			     int *cntnr_cnt)
{
	lv_val		*lv;
	lv_sbs_tbl	*stbl, *child;
	sbs_blk		*num, *str;
	int		i, cntnrs_found;
	boolean_t	nested;

	assert(lv_base);
	if (NULL == cntnr_cnt)
	{	/* This is a primary invocation on a base variable */

		nested = FALSE;
		assert(MV_SYM == lv_base->ptrs.val_ent.parent.sym->ident);
		DEBUG_ONLY(if (!lv_base->has_aliascont)
				   DBGRFCT((stderr, "als_scan_for_containers: Scan would have been avoided in PRO\n")));
		cntnr_cnt = &cntnrs_found;
		cntnrs_found =0;
	} else
	{	/* This is a nested invocation so we are processing a subscript level */
		nested = TRUE;
		assert(MV_SBS == lv_base->ptrs.val_ent.parent.sbs->ident);
	}
	stbl = lv_base->ptrs.val_ent.children;
	assert(stbl);
	assert(MV_SBS == stbl->ident);
	assert(MV_SYM == stbl->sym->ident);
	num = stbl->num;
	str = stbl->str;
	if (num)
	{	/* Process numeric subscripts */
		if (stbl->int_flag)
		{
			assert(!num->nxt);
			for (i = 0;  i < SBS_NUM_INT_ELE;  i++)
			{	/* Look in each bucket .. since index is subscript, not all may have values */
				if ((lv = num->ptr.lv[i]) && (lv->v.mvtype & MV_ALIASCONT))	/* Note lv assignment */
				{
					++*cntnr_cnt;
					(*als_container_processor)(lv, arg1, arg2);
				}
				/* Since we go through each lv, it is possible for lv to be NULL in this array */
				if (lv && (child = lv->ptrs.val_ent.children))				/* Note child assignment */
					als_scan_for_containers(lv, als_container_processor, arg1, arg2, cntnr_cnt);
			}

		} else
		{
			while (num)
			{
				for (i = 0;  i < num->cnt;  i++)
				{
					if ((lv = num->ptr.sbs_flt[i].lv) && (lv->v.mvtype & MV_ALIASCONT)) /* Note lv assign */
					{
						++*cntnr_cnt;
						(*als_container_processor)(lv, arg1, arg2);
					}
					assert(NULL != lv);					/* Should never be null in this
												 * structure */
					if (child = lv->ptrs.val_ent.children)			/* Note child assignment */
						als_scan_for_containers(lv, als_container_processor, arg1, arg2, cntnr_cnt);
				}
				num = num->nxt;
			}
		}
	}
	while (str)
	{
		for (i = 0;  i < str->cnt;  i++)
		{
			if ((lv = str->ptr.sbs_str[i].lv) && (lv->v.mvtype & MV_ALIASCONT))	/* Note lv assignment */
			{
				++*cntnr_cnt;
				(*als_container_processor)(lv, arg1, arg2);
			}
			assert(NULL != lv);							/* Should never be null in this
												 * structure */
			/* If this node has children, run scan on them as well */
			if (child = lv->ptrs.val_ent.children)				/* Note child assignment */
				als_scan_for_containers(lv, als_container_processor, arg1, arg2, cntnr_cnt);

		}
		str = str->nxt;
	}
	/* If no alias containers found in this base var, make sure flag gets turned off */
	if (!nested)
	{
		if (0 == *cntnr_cnt)
			lv_base->has_aliascont = FALSE;
		else
			assert(lv_base->has_aliascont);
	}
}


/* Local routine!
   Routine to process an alias container found in a node of a var being "returned" back through an exclusive new. We may
   have to move the data.
*/
void als_prcs_xnew_alias_cntnr_node(lv_val *lv, void *popdsymvalv, void *cursymvalv)
{
	lv_val	*newlv, *oldlv;
	symval	*popdsymval, *cursymval;

	popdsymval = (symval *)popdsymvalv;
	cursymval = (symval *)cursymvalv;
	assert(MV_SBS == lv->ptrs.val_ent.parent.sym->ident);
	oldlv = (lv_val *)lv->v.str.addr;
	assert(oldlv);
	if (MV_LVCOPIED == oldlv->v.mvtype)
	{	/* This lv_val has been copied over already so use that pointer instead */
		newlv = oldlv->ptrs.copy_loc.newtablv;
		lv->v.str.addr = (char *)newlv;			/* Replace container ptr */
		DBGRFCT((stderr, "\nals_prcs_xnew_alias_cntnr_node: aliascont var found - referenced array already copied"
			 " - Setting pointer in aliascont lv 0x"lvaddr" to lv 0x"lvaddr"\n", lv, newlv));
	} else
	{
		assert(MV_SYM == oldlv->ptrs.val_ent.parent.sym->ident);
		if (popdsymval == oldlv->ptrs.val_ent.parent.sym)
		{	/* lv_val is owned by the popped symtab .. clone it to the new current tree */
			CLONE_LVVAL(oldlv, newlv, cursymval);
			lv->v.str.addr = (char *)newlv;		/* Replace container ptr */
			DBGRFCT((stderr, "\nals_prcs_xnew_alias_cntnr_node: aliascont var found - aliascont lv 0x"lvaddr
				 " being reset to point to lv 0x"lvaddr" which is a clone of lv 0x"lvaddr"\n", lv, newlv, oldlv));
		} else
		{	/* lv_val is owned by current or older symval .. just use it in the subsequent scan in case it
			   leads us to other lv_vals owned by the popped symtab.
			*/
			DBGRFCT((stderr, "\nals_prcs_xnew_alias_cntnr_node: aliascont var found - aliascont lv 0x"lvaddr
				 " just being (potentially) scanned for container vars\n", lv));
			newlv = oldlv;
		}
		RESOLV_ALIAS_CNTNRS_IN_TREE(newlv, popdsymval, cursymval);
	}
}


/* Routine to process an alias container found in an array being "saved" by TSTART (op_tstart). We need to set this array up
   so it gets copied just like op_tstart does for the base variables that are specified in it. In addition, this new array
   itself needs to be scanned so if it points to anything, that too gets saved if modified (all handled by
   TP_SAVE_RESTART_VAR() macro).
*/
void als_prcs_tpsav_cntnr_node(lv_val *lv, void *tfv, void *dummy)
{
	lv_val		*lv_base;
	tp_frame	*tf;

	assert(lv);
	tf = tfv;
	assert(tf);
	lv_base = (lv_val *)lv->v.str.addr;	/* Extract container pointer */
	assert(lv_base);
	assert(MV_SYM == lv_base->ptrs.val_ent.parent.sym->ident);
	assert(1 <= lv_base->stats.trefcnt);
	assert(1 <= lv_base->stats.crefcnt);
	if (NULL == lv_base->tp_var)
	{	/* Save this var if it hasn't already been saved */
		assert(lv_base->stats.tstartcycle != tstartcycle);
		DBGRFCT((stderr, "\ntpSAV_container: Container at 0x"lvaddr" refers to lv 0x"lvaddr" -- Creating tpsav block\n",
			lv, lv_base));
		TP_SAVE_RESTART_VAR(lv_base, tf, &null_mname_entry);
		INCR_CREFCNT(lv_base);	/* 2nd increment for reference via a container node */
		INCR_TREFCNT(lv_base);
		if (lv_base->ptrs.val_ent.children)
			TPSAV_CNTNRS_IN_TREE(lv_base);
	} else
	{	/* If not saving it, we still need to bump the ref count(s) for this reference and
		   process any children if we have't already seen this node (taskcycle check will tell us this).
		*/
		DBGRFCT((stderr, "\ntpSAV_container: Container at 0x"lvaddr" refers to lv 0x"lvaddr" -- Incrementing refcnts\n",
			lv, lv_base));
		INCR_CREFCNT(lv_base);
		INCR_TREFCNT(lv_base);
		assert(0 < lv_base->stats.trefcnt);
		assert(0 < lv_base->stats.crefcnt);
		if (lv_base->stats.tstartcycle != tstartcycle)
		{
			DBGRFCT((stderr, "\ntpSAV_container: .. Container at 0x"lvaddr" refers to lv 0x"lvaddr
				 " -- processing tree\n", lv, lv_base));
			if (lv_base->ptrs.val_ent.children)
				TPSAV_CNTNRS_IN_TREE(lv_base);
		} else
		{
			DBGRFCT((stderr, "\ntpSAV_container: .. Container at 0x"lvaddr" refers to lv 0x"lvaddr
				 " -- Already processed -- bypassing\n", lv, lv_base));
		}
	}
}


/* For a given container var found in the tree  we need to re-establish the reference counts for the base var
   the container is pointing to. Used during a local var restore on a TP restart.
*/
void als_prcs_tprest_cntnr_node(lv_val *lv, void *dummy1, void *dummy2)
{
	lv_val		*lv_base;

	assert(lv);
	lv_base = (lv_val *)lv->v.str.addr;	/* Extract container pointer */
	assert(lv_base);
	assert(MV_SYM == lv_base->ptrs.val_ent.parent.sym->ident);
	assert(1 <= lv_base->stats.trefcnt);
	assert(1 <= lv_base->stats.crefcnt);
	assert(lv_base->tp_var);
	DBGRFCT((stderr, "\ntpREST_cntnr_node: Processing container at 0x"lvaddr"\n", lv));
	INCR_CREFCNT(lv_base);
	INCR_TREFCNT(lv_base);
	assert(0 < (lv_base)->stats.trefcnt);
	assert(0 < (lv_base)->stats.crefcnt);
}


/* For a given container, decrement the ref count of the creature it points to. Part of unwinding an unmodified
   tp saved variable.
*/
void als_prcs_tpunwnd_cntnr_node(lv_val *lv, void *dummy1, void *dummy2)
{
	lv_val		*lv_base;

	assert(lv);
	lv_base = (lv_val *)lv->v.str.addr;	/* Extract container pointer */
	assert(lv_base);
	assert(MV_SYM == lv_base->ptrs.val_ent.parent.sym->ident);
	assert(1 <= lv_base->stats.trefcnt);
	assert(1 <= lv_base->stats.crefcnt);
	/* Note we cannot assert lv->tp_var here since the tp_var node may have already been freed and cleared
	   by unwind processing of the base var itself. We just have to undo our counts here and keep going.
	*/
	DBGRFCT((stderr, "\ntpUNWND_cntnr_node: Processing container at 0x"lvaddr"\n", lv_base));
	DECR_CREFCNT(lv_base);
	DECR_BASE_REF_NOSYM(lv_base, FALSE);
}


/* This routine deletes the data pointed to by the lv_val and removes the container flag from the value making it just
   a regular NULL/0 value.
*/
void als_prcs_kill_cntnr_node(lv_val *lv, void *dummy1, void *dummy2)
{
	lv_val	*lv_base;

	assert(lv->v.mvtype & MV_ALIASCONT);
	assert(MV_SBS == lv->ptrs.val_ent.parent.sbs->ident);	/* Verify subscripted var */
	lv_base = (lv_val *)lv->v.str.addr;
	assert(lv_base);
	assert(MV_SYM == lv_base->ptrs.val_ent.parent.sym->ident);
	lv_kill(lv_base, TRUE);
	lv->v.mvtype &= ~MV_ALIASCONT;
	DECR_CREFCNT(lv_base);
	DECR_BASE_REF_NOSYM(lv_base, TRUE);
}


/* Local routine!
   This routine checks if the supplied container points to an lv_val that is already marked as having been processd in this pass.
   If not, the lv_val is marked and processed recursively.
*/
void als_prcs_markreached_cntnr_node(lv_val *lv, void *dummy1, void *dummy2)
{
	lv_val	*lv_base;

	assert(lv->v.mvtype & MV_ALIASCONT);
	assert(MV_SBS == lv->ptrs.val_ent.parent.sbs->ident);	/* Verify subscripted var */
	lv_base = (lv_val *)lv->v.str.addr;
	assert(lv_base);
	assert(MV_SYM == lv_base->ptrs.val_ent.parent.sym->ident);
	MARK_REACHABLE(lv_base);
}


/* This regular routine processes the base var pointed to by the container if it has not already been processed in
   this pass (as determined by lvtaskcycle). Processing includes incrementing refcnts and creating an lv_xnew_ref entry
   for the base var so we can check it again when the symtab pops to see if any containers were created in them that
   point to the symtab being popped.
*/
void als_prcs_xnewref_cntnr_node(lv_val *lv, void *dummy1, void *dummy2)
{
	lv_val		*lv_base;
	lv_xnew_ref	*xnewref;

	assert(lv->v.mvtype & MV_ALIASCONT);
	assert(MV_SBS == lv->ptrs.val_ent.parent.sbs->ident);	/* Verify subscripted var */
	lv_base = (lv_val *)lv->v.str.addr;
	assert(lv_base);
	assert(MV_SYM == lv_base->ptrs.val_ent.parent.sym->ident);
	if (lv_base->stats.lvtaskcycle != lvtaskcycle)
	{
		INCR_CREFCNT(lv_base);
		INCR_TREFCNT(lv_base);
		lv_base->stats.lvtaskcycle = lvtaskcycle;
		if (NULL != xnewref_anchor)
		{	/* Reuse entry from list */
			xnewref = xnewref_anchor;
			xnewref_anchor = xnewref->next;
		} else
			xnewref = (lv_xnew_ref *)malloc(SIZEOF(lv_xnew_ref));
		xnewref->lvval = lv_base;
		xnewref->next = curr_symval->xnew_ref_list;
		curr_symval->xnew_ref_list = xnewref;
		if (lv_base->ptrs.val_ent.children)
			XNEWREF_CNTNRS_IN_TREE(lv_base);
	}
}


/* Initialize ZWRite hash table structures used when ZWRiting in an aliased variable environment
 */
void als_zwrhtab_init(void)
{
	zwr_zav_blk	*zavb, *zavb_next;

	if (zwrhtab && zwrhtab->cleaned)
		return;
	if (NULL == zwrhtab)
	{	/* none yet .. allocate and init one */
		zwrhtab = (zwr_hash_table *)malloc(SIZEOF(zwr_hash_table));
		zwrhtab->first_zwrzavb = NULL;
		zwrhtab->zav_flist = NULL;
		init_hashtab_addr(&zwrhtab->h_zwrtab, ZWR_HTAB_INIT_SIZE);
	} else
	{	/* Have one, reinitialize it */
		zwrhtab->zav_flist = NULL;
		assert(zwrhtab->first_zwrzavb);
		zavb = zwrhtab->first_zwrzavb;
		if (zavb)
		{
			for (zavb_next = zavb->next; zavb_next; zavb = zavb_next, zavb_next = zavb->next)
			{	/* Leave one block on queue if it exists .. get rid of others */
				free(zavb);
			}
			assert(zavb);
			zwrhtab->first_zwrzavb = zavb;
		}
		reinitialize_hashtab_addr(&zwrhtab->h_zwrtab);
	}
	if (NULL == zwrhtab->first_zwrzavb)
		zwrhtab->first_zwrzavb = (zwr_zav_blk *)malloc(SIZEOF(zwr_zav_blk) + (SIZEOF(zwr_alias_var) * ZWR_ZAV_BLK_CNT));
	ZAV_BLK_INIT(zwrhtab->first_zwrzavb, NULL);
	zwrhtab->cleaned = TRUE;
}


/* Obtain a zwr_alias_var slot for the zalias hash table */
zwr_alias_var *als_getzavslot(void)
{
	zwr_alias_var	*zav;
	zwr_zav_blk	*zavb;

	assert(zwrhtab);
	assert(zwrhtab->first_zwrzavb);

	zwrhtab->cleaned = FALSE;	/* No longer in a clean/initialized state */
	if (zwrhtab->zav_flist)
	{	/* Free block available */
		zav = zwrhtab->zav_flist;
		zwrhtab->zav_flist = zav->ptrs.free_ent.next_free;
	} else
	{	/* Check if a block can be allocated out of a zavb super block */
		zavb = zwrhtab->first_zwrzavb;
		if (zavb->zav_free >= zavb->zav_top)
		{	/* This block is full too .. need a new one */
			zavb = (zwr_zav_blk *)malloc(SIZEOF(zwr_zav_blk) + (SIZEOF(zwr_alias_var) * ZWR_ZAV_BLK_CNT));
			ZAV_BLK_INIT(zavb, zwrhtab->first_zwrzavb);
			zwrhtab->first_zwrzavb = zavb;
		}
		assert(zavb->zav_free < zavb->zav_top);
		zav = zavb->zav_free++;
	}
	zav->value_printed = FALSE;
	return zav;
}


/* See if lv_val is associated with a base named var in the current symval.
   Scan the hash table for valid entries and see if any of them point to supplied lv_val.
   If so, return the hash table entry which contains the var's name that is "lowest" which
   necessitates a full scan of the hash table (which ZWrite processing is going to do several
   times anyway.
*/
ht_ent_mname *als_lookup_base_lvval(lv_val *lvp)
{
	ht_ent_mname	*htep, *htep_top, *htep_loweq;
	lv_val		*lvhtval;

	htep_loweq = NULL;
	htep = curr_symval->h_symtab.base;
	htep_top = curr_symval->h_symtab.base + curr_symval->h_symtab.size;
	assert(htep_top ==  curr_symval->h_symtab.top);
	for (; htep < htep_top; htep++)
	{
		if (HTENT_VALID_MNAME(htep, lv_val, lvhtval) && '$' != *htep->key.var_name.addr && lvp == lvhtval)
		{	/* HT entry is valid and has a key that is not a $ZWRTAC type key and the lval matches
			   so we have a candidate to check for "lowest" alias name for given lv_val addr.
			*/
			if (htep_loweq)
			{	/* See current champ higher than candidate, then get new champ */
				if (0 < memvcmp(htep_loweq->key.var_name.addr, htep_loweq->key.var_name.len,
						htep->key.var_name.addr, htep->key.var_name.len))
					htep_loweq = htep;
			} else
				/* First time thru, free-ride assignment */
				htep_loweq = htep;
		}
	}
	return htep_loweq;
}


/* Routine to do a garbage collection on the lv_vals in the current symbol table in order to detect if
   there is any "lost data" which are 2 or more base lv_vals that point to each other thus keeping their
   reference counts non-zero and prevent them from being deleted but otherwise have no entry in the hash table
   themselves nor are linked to by any combination of container var linked arrays that do have an entry in
   the hash table -- in other words, they are totally orphaned with no way to retrieve them so are effectively
   dead but are not able to be killed in an automated fashion. This routine will find and kill those blocks
   returning a count of the lv_vals thus found and killed.

   Operation:

   1)  Run lv_blks which contain all lv_val structures in use for this symbol table.
   2)  Record each base lv_val in our version of the array used by stp_gcol. Base lv_vals can be identified
       by a type not equal to MV_SBS and having a non-zero parent.sym field pointing to a block with type
       MV_SYM. There are 3 exceptions to this: In UNIX, the zsrch_var, zsrch_dir1, and zsrch_dir2 fields contain
       lv_vals that should not be released. Check for and avoid them.
   3)  Increment lvtaskcycle with which we will mark lv_vals as having been marked accessible as we discover them.
   4)  Go through the hashtable. Set the lvtaskcycle field to mark the lv_val "reachable".
   5)  If the lv_val has descendants, run the decendant chain to look for container vars.
   6)  The base lv_vals that container vars point to, if not already marked "reachable"  will be so marked and
       the search recursively invoked on the new var.
   7)  Do the same by running the mv_stent chain marking the temporarily displaced vars for parameters and NEW's as
       "reachable".
   8)  Do the same by running the TP stack marking local variable copies made for this symbol table as reachable.
   9)  Mark any pending return argument as "reachable".
   10) Once the "reachable" search is complete, run through the created list of lv_vals and locate any that were
       not reachable. If they remain undeleted (deleted will have parent.sym field zeroed), delete them.

   Note this routine uses the same buffer structure that stp_gcol() uses except it loads its array with lv_val*
   instead of mstr*. An address is an address..

*/
int als_lvval_gc(void)
{
	int		killcnt;
	lv_blk		*lv_blk_ptr;
	ht_ent_mname	*htep, *htep_top;
	lv_val		*lvp, *lvlimit;
	lv_val		**lvarraycur, **lvarray, **lvarraytop, **lvptr;
	mv_stent 	*mv_st_ent;
	tp_frame	*tf;
	tp_var		*restore_ent;
	lv_sbs_tbl	*tmpsbs;
	DEBUG_ONLY(uint4 savelvtaskcycle);

	assert(!suspend_lvgcol);
	DBGRFCT((stderr, "als_lvval_gc: Beginning lv_val garbage collection\n"));
	if (NULL == stp_array)
		/* Same initialization as is in stp_gcol_src.h */
		stp_array = (mstr **)malloc((stp_array_size = STP_MAXITEMS) * SIZEOF(mstr *));

	lvarraycur = lvarray = (lv_val **)stp_array;
	lvarraytop = lvarraycur + stp_array_size;

	/* Steps 1,2 - find all the base lv_vals and put in list */
	for (lv_blk_ptr = &curr_symval->first_block;
	     lv_blk_ptr;
	     lv_blk_ptr = lv_blk_ptr->next)
	{
		for (lvp = lv_blk_ptr->lv_base, lvlimit = lv_blk_ptr->lv_free;
		     lvp < lvlimit;  lvp++)
		{
			if (MV_SBS != lvp->v.mvtype && NULL != lvp->ptrs.val_ent.parent.sym &&
			    MV_SYM == lvp->ptrs.val_ent.parent.sym->ident UNIX_ONLY(&& zsrch_var != lvp)
			    UNIX_ONLY(&& zsrch_dir1 != lvp && zsrch_dir2 != lvp))
			{	/* Put it in the list */
				assert(0 < lvp->stats.trefcnt);
				if (lvarraycur >= lvarraytop)
				{	/* Need more room -- expand */
					stp_expand_array();
					lvarraycur = lvarray = (lv_val **)stp_array;
					lvarraytop = lvarraycur + stp_array_size;
				}
				*lvarraycur++ = lvp;
			}
		}
	}
	/* Step 3, increment lvtaskcycle to mark "reachable" lv_vals */
	INCR_LVTASKCYCLE;
	DEBUG_ONLY(savelvtaskcycle = lvtaskcycle);
	/* Steps 4,5,6 - Find and mark reachable lv_vals */
	DBGRFCT((stderr, "als_lvval_gc: Starting symtab scan\n"));
	htep = curr_symval->h_symtab.base;
	htep_top = curr_symval->h_symtab.base + curr_symval->h_symtab.size;
	assert(htep_top == curr_symval->h_symtab.top);
	for (; htep < htep_top; htep++)
	{
		if (HTENT_VALID_MNAME(htep, lv_val, lvp))
		{	/* HT entry is valid. Note for purposes of this loop, we do NOT bypass $ZWRTAC type keys since
			   they are valid reachable variables even if hidden */
			MARK_REACHABLE(lvp);
		}
	}
	/* Step 7 - Run the mv_stent chain marking those vars as reachable. */
	DBGRFCT((stderr, "als_lvval_gc: Starting mv_stent scan\n"));
	for (mv_st_ent = mv_chain; mv_st_ent; mv_st_ent = (mv_stent *)(mv_st_ent->mv_st_next + (char *)mv_st_ent))
	{
		switch (mv_st_ent->mv_st_type)
		{	/* The types processed here contain lv_vals we want to mark */
			case MVST_NTAB:
				lvp = mv_st_ent->mv_st_cont.mvs_ntab.save_value;
				DBGRFCT((stderr, "als_lvval_gc: NTAB at 0x"lvaddr" has save value 0x"lvaddr"\n",
					 mv_st_ent, lvp));
				assert(NULL != lvp);
				break;
			case MVST_PVAL:
				/* Note the save_value in the PVAL types below can be zero since they are created in
				   op_bindparm with a NULL save_value value and later filled in by op_bindparm. It is
				   possible to trigger this code in between the push_parm call of the caller and the
				   op_bindparm call of the callee when tracing or with an outofband event. In that case,
				   we don't want that NULL save_value pointer ending our loop prematurely.
				*/
				lvp = mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab.save_value;
				DBGRFCT((stderr, "als_lvval_gc: PVAL at 0x"lvaddr" has save value 0x"lvaddr"\n",
					 mv_st_ent, lvp));
				assert(NULL != mv_st_ent->mv_st_cont.mvs_pval.mvs_val);
				/* Mark created lv_val to hold current value as reachable as it may not (yet) be in the
				   hashtable if op_bindparm has not yet run.
				*/
				MARK_REACHABLE(mv_st_ent->mv_st_cont.mvs_pval.mvs_val);
				if (NULL == lvp)
					continue;	/* Don't end loop prematurely */
				break;
			case MVST_NVAL:
				lvp = mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab.save_value;
				DBGRFCT((stderr, "als_lvval_gc: NVAL at 0x"lvaddr" has save value 0x"lvaddr"\n",
					 mv_st_ent, lvp));
				assert(NULL != mv_st_ent->mv_st_cont.mvs_nval.mvs_val);
				/* Mark created lv_val to hold current value as reachable as it may not (yet) be in the
				   hashtable if op_bindparm has not yet run.
				*/
				MARK_REACHABLE(mv_st_ent->mv_st_cont.mvs_nval.mvs_val);
				assert(NULL != lvp);
				break;
			case MVST_STAB:
				/* The symbol table is changing to be other than the table we are using so we can
				   stop the loop now. Exiting the switch with lvp NULL indicates that.
				*/
				DBGRFCT((stderr, "als_lvval_gc: STAB mv_stent at 0x"lvaddr" stops mv_stent scan\n",
					mv_st_ent));
				lvp = NULL;
				break;
			default:
				DBGRFCT((stderr, "als_lvval_gc: Ignoring mv_stent type %d\n", mv_st_ent->mv_st_type));
				continue;
		}
		if (NULL == lvp)
			break;
		MARK_REACHABLE(lvp);
	}
	/* Step 8 - Run the TP stack to see if there is anything we can mark reachable */
	DBGRFCT((stderr, "als_lvval_gc: Starting TP stack scan\n"));
	for (tf = tp_pointer; NULL != tf && tf->sym == curr_symval; tf = tf->old_tp_frame)
	{
		for (restore_ent = tf->vars;  NULL != restore_ent;  restore_ent = restore_ent->next)
		{	/* Since TP keeps its own use count on these sorts of variables, we will mark both the
			   current and saved values in these blocks. This is because the "current value" could
			   be detached from the hash table at this point but is still viable while we hold a use
			   count on it.
			*/
			MARK_REACHABLE(restore_ent->current_value);
			MARK_REACHABLE(restore_ent->save_value);
		}
	}
	/* Step 9 - Mark any pending alias return argument as reachable */
	if (NULL != alias_retarg)
	{	/* There is a pending alias return arg (container). Find the lv it is pointing to and mark it and its progeny
		 * as reachable.
		 */
		assert(0 != (MV_ALIASCONT & alias_retarg->mvtype));
		lvp = (lv_val *)alias_retarg->str.addr;
		assert(MV_SYM == lvp->ptrs.val_ent.parent.sym->ident);
		MARK_REACHABLE(lvp);
	}
	/* Step 10 - Run the list of base lv_vals we created earlier and see which ones were not marked with the current
	   cycle. Note they may have already been deleted after the first one gets deleted so we can check for that
	   by looking for a zeroed parent field. Note the object of this code is not to force-delete the vars as we
	   encounter them but by performing a deletion of their arrays, we will kill the interlinking container vars that
	   are keeping these vars alive.
	*/
	killcnt = 0;
	DBGRFCT((stderr, "\nals_lvval_gc: final orphaned lvval scan\n"));
	for (lvptr = lvarray; lvptr < lvarraycur; ++lvptr)
	{
		lvp = *lvptr;
		if (lvp->stats.lvtaskcycle != lvtaskcycle)
		{	/* Have an orphaned lv_val */
			DBGRFCT((stderr, "\nals_lvval_gc: lvval 0x"lvaddr" has been identified as orphaned\n", lvp));
			++killcnt;
			if (lvp->ptrs.val_ent.parent.sym)
			{	/* Var is still intact, kill it. Note that in this situation, since there are no hash table
				   entries pointing to us, our container refs and total refs should be equal. We can't
				   use the "regular" DECR macros because those get us into trouble. For example if this
				   var has a container pointing to another var who has a container pointing to us and it
				   is only those pointers keeping both vars alive, decrementing our counter causes it to
				   become zero which messes up the deletion of the other var's container since the refcnts
				   are already zero. What we will do instead is INCREASE the trefcnt to keep this var
				   from being deleted, then drive the kill of any array it has to spur these vars to go
				   away.

				 */
				DBGRFCT((stderr, "\nals_lvval_gc: Working to release unreachable lvval 0x"lvaddr"\n", lvp));
				assert(lvp->stats.trefcnt == lvp->stats.crefcnt);
				INCR_TREFCNT(lvp);
				if (tmpsbs = lvp->ptrs.val_ent.children)	/* Note assignment */
				{
					assert(lvp == tmpsbs->lv);
					lvp->ptrs.val_ent.children = NULL;
					lv_killarray(tmpsbs, FALSE);
				}
				DECR_BASE_REF_NOSYM(lvp, FALSE);	/* Var might go away now, or later if need more
									   deletes first */
			} else
				DBGRFCT((stderr, "\nals_lvval_gc: Seems to have become released -- lvval 0x"lvaddr"\n", lvp));
		}
	}
	DBGRFCT((stderr, "\nals_lvval_gc: final orphaned lvval scan completed\n"));
#ifdef DEBUG
	/* The removal of a reference for each lv_val should have done it but let's go back and verify they all
	   went away. If not, then it is not a simple case of user silliness and we have a problem.
	*/
	for (lvptr = lvarray; lvptr < lvarraycur; ++lvptr)
	{
		lvp = *lvptr;
		if (lvp->stats.lvtaskcycle != lvtaskcycle && lvp->ptrs.val_ent.parent.sym)
		{	/* Var is still intact, kill it */
			assert(FALSE);
		}
	}
#endif
	assert(lvtaskcycle == savelvtaskcycle);
	DBGRFCT((stderr, "als_lvval_gc: GC complete -- recovered %d lv_vals\n", killcnt));
	SPGC_since_LVGC = 0;
	return killcnt;
}


#ifdef DEBUG_ALIAS
/* Routine to check lv_val monitoring. If any lv_vals that were created during the monitoring period still exist, emit a
   message to that effect with the lv_val address. In this manner we hope to find lv_val leaks (if any) in various tests.
   Note that this is a debugging (not debug build) only routine since its costs are non-trivial and is only enabled
   when necessary. Other tests check for memory leaks so if they find one, this monitoring can be used to discover the
   source so this is not needed for test coverage.
*/
void als_lvmon_output(void)
{
	symval		*lvlsymtab;
	lv_blk		*lvbp;
	lv_val		*lvp, *lvp_top;

	flush_pio();
	for (lvlsymtab = curr_symval; lvlsymtab; lvlsymtab = lvlsymtab->last_tab)
		for (lvbp = &curr_symval->first_block; lvbp; lvbp = lvbp->next)
			for (lvp = lvbp->lv_base, lvp_top = lvbp->lv_free; lvp < lvp_top; lvp++)
				if (MV_SBS != lvp->v.mvtype && lvp->stats.lvmon_mark)
				{	/* lv_val slot not used as an sbs and is marked. Report it */
					FPRINTF(stderr, "als_lvmon_output: lv_val at 0x"lvaddr" is still marked\n", lvp);
				}
	fflush(stderr);
}
#endif
