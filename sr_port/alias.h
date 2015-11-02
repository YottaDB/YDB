/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef   	ALIAS_H_
# define   	ALIAS_H_

#define DOLLAR_ZWRTAC "$ZWRTAC"

/* Min and max for the number of stringpool garbage collections (SPGC) that will be done without having
   done a lv_val garbage collection (aka LVGC aka als_lvval_gc). While I did give these numbers some
   thought, they aren't far from semi-random. Since the LVGC is kind of expensive (requiring two
   traversals of the lv_vals in a symbol table) I decided 2 SPGCs was good to spread the cost of one
   LVGC over. For the max, I didn't want to let it get too high so the tuning algorithm didn't take a
   long time to get back to a more frequent value when necessary yet 64 seems fairly infrequent.
   These numbers are totally subject to future studies.. [SE 12/2008]
*/
#define MIN_SPGC_PER_LVGC 2
#define MAX_SPGC_PER_LVGC 64

#include "zwrite.h"

/* Macro used intermittently in code to debug alias code in general. Note this macro must be specified
   as a compile option since it is used in macros that do not pull in this alias.h header file.
*/
#ifdef DEBUG_ALIAS
# define DBGALS(x) DBGFPF(x)
# define DBGALS_ONLY(x) x
#else
# define DBGALS(x)
# define DBGALS_ONLY(x)
#endif

/* Macro used intermittently to trace reference count changes */
/* #define DEBUG_REFCNT */
#ifdef DEBUG_REFCNT
# define DBGRFCT(x) DBGFPF(x)
# define DBGRFCT_ONLY(x) x
#else
# define DBGRFCT(x)
# define DBGRFCT_ONLY(x)
#endif

/* Since DBGFPF calls flush_pio, if we are debugging, include the defining header file */
#if defined(DEBUG_ALIAS) || defined(DEBUG_REFCNT)
#  include "io.h"
#endif

/* Macro to increment total refcount (and optionally trace it) */
#define INCR_TREFCNT(lv)											\
{														\
	assert(LV_IS_BASE_VAR(lv));										\
	DBGRFCT((stderr, "\nIncrement trefcnt for lv_val at 0x"lvaddr" from %d to %d by %s line %d\n",		\
			(lv), (lv)->stats.trefcnt, (lv)->stats.trefcnt + 1, __FILE__, __LINE__)); 		\
	++(lv)->stats.trefcnt;											\
}

/* Macro to decrement total refcount (and optionally trace it) */
#define DECR_TREFCNT(lv)											\
{														\
	assert(LV_IS_BASE_VAR(lv));										\
	DBGRFCT((stderr, "\nDecrement trefcnt for lv_val at 0x"lvaddr" from %d to %d by %s line %d\n",		\
			(lv), (lv)->stats.trefcnt, (lv)->stats.trefcnt - 1, __FILE__, __LINE__)); 		\
	--(lv)->stats.trefcnt;											\
	assert(0 <= (lv)->stats.trefcnt);									\
}

/* Macro to increment container refcount (and optionally trace it) */
#define INCR_CREFCNT(lv)											\
{														\
	assert(LV_IS_BASE_VAR(lv));										\
	DBGRFCT((stderr, "\nIncrement crefcnt for lv_val at 0x"lvaddr" from %d to %d by %s line %d\n",		\
			(lv), (lv)->stats.crefcnt, (lv)->stats.crefcnt + 1, __FILE__, __LINE__)); 		\
	++(lv)->stats.crefcnt;											\
}

/* Macro to decrement container refcount (and optionally trace it) */
#define DECR_CREFCNT(lv)											\
{														\
	assert(LV_IS_BASE_VAR(lv));										\
	DBGRFCT((stderr, "\nDecrement crefcnt for lv_val at 0x"lvaddr" from %d to %d by %s line %d\n",		\
			(lv), (lv)->stats.crefcnt, (lv)->stats.crefcnt - 1, __FILE__, __LINE__));		\
	--(lv)->stats.crefcnt;											\
	assert(0 <= (lv)->stats.crefcnt);									\
}

/* There are three flavors of DECR_BASE_REF depending on the activities we need to persue. The first two flavors
   DECR_BASE_REF and DECR_BASE_REF_RQ take 2 parms (hashtable entry and lv_val addresses). Both of these do hashtable
   entry maint in addition to lv_val maint but do it in different ways. The DECR_BASE_REF macro's purpose is to
   leave the hashtable always pointing to a valid lv_val. If the one we are decrementing goes to zero, we can just
   zap that one and leave it there but if the refcnt is not zero, we can't do that. Since another command may
   follow the command doing the DECR_BASE_REF (e.g. KILL *), we can't leave the hash table entry with a null value
   since gtm_fetch() won't be called to fill it in so we must allocate a new lv_val for it. By contrast, with the
   DECR_BASE_REF_RQ macro, if the lv_val refcnt goes to zero, the lv_val is requeued and in either case, the hte
   address is cleared to make way for a new value. The 3rd flavor is the DCR_BASE_REF_NOSYM macro which is used for
   orphaned lv_vals not in a hashtable. This macro just requeues lv_vals that hit a refcnt of zero.
*/

#define	LVP_KILL_SUBTREE_IF_EXISTS(lvp, dotpsave)				\
{										\
	lvTree	*lvt_child;							\
										\
	assert(LV_IS_BASE_VAR(lvp));						\
	assert(0 == (lvp)->stats.crefcnt);					\
	lvt_child = LV_GET_CHILD(lvp);						\
	if (NULL != lvt_child)							\
	{									\
		assert(((lvTreeNode *)(lvp)) == LVT_PARENT(lvt_child));		\
		LV_CHILD(lvp) = NULL;						\
		lv_killarray(lvt_child, dotpsave);				\
	}									\
}

/* Macro to decrement a base var reference and do appropriate cleanup */
#define DECR_BASE_REF(tabent, lvp, dotpsave)									\
{	/* Perform reference count maintenance for base var */							\
	lv_val	*dbr_lvp;											\
	symval	*sym;												\
														\
	assert(LV_IS_BASE_VAR(lvp));										\
	assert(0 < (lvp)->stats.trefcnt);									\
	DECR_TREFCNT(lvp);											\
	sym = LV_GET_SYMVAL(lvp);										\
	if (0 == (lvp)->stats.trefcnt)										\
	{	/* This lv_val can be effectively killed and remain in hte */ 					\
		LVP_KILL_SUBTREE_IF_EXISTS(lvp, dotpsave);							\
		assert(NULL == (lvp)->tp_var);									\
		LVVAL_INIT(lvp, sym);										\
	} else													\
	{	/* lv_val otherwise still in use -- put a new one in this hte */ 				\
		assert((lvp)->stats.trefcnt >= (lvp)->stats.crefcnt);						\
		dbr_lvp = lv_getslot(sym);									\
		DBGRFCT((stderr, "DECR_BASE_REF: Resetting hte 0x"lvaddr" from 0x"lvaddr" to 0x"lvaddr"\n",	\
			 tabent, (lvp), dbr_lvp));								\
		LVVAL_INIT(dbr_lvp, sym);									\
		tabent->value = dbr_lvp;									\
	}													\
}

/* Macro to decrement a base var reference and do appropriate cleanup except the tabent value is unconditionally
 * cleared and the lvval put on the free queue. Used when the tabent is about to be reused for a different lv.
 */
#define DECR_BASE_REF_RQ(tabent, lvp, dotpsave)								\
{	/* Perform reference count maintenance for base var */						\
	assert(LV_IS_BASE_VAR(lvp));									\
	assert(0 < (lvp)->stats.trefcnt);								\
	DECR_TREFCNT(lvp);										\
	DBGRFCT((stderr, "DECR_BASE_REF_RQ: Resetting hte 0x"lvaddr" from 0x"lvaddr" to NULL\n",	\
		 tabent, tabent->value));								\
	tabent->value = (void *)NULL;									\
	if (0 == (lvp)->stats.trefcnt)									\
	{	/* This lv_val is done .. requeue it after it is killed */ 				\
		LVP_KILL_SUBTREE_IF_EXISTS(lvp, dotpsave);						\
		LV_FREESLOT(lvp);									\
	} else												\
		assert((lvp)->stats.trefcnt >= (lvp)->stats.crefcnt);					\
}

/* Macro to decrement a base var reference and do appropriate cleanup except no hash table
   entry value cleanup is done.
*/
#define DECR_BASE_REF_NOSYM(lvp, dotpsave)					\
{	/* Perform reference count maintenance for base var */			\
	assert(LV_IS_BASE_VAR(lvp));						\
	assert(0 < (lvp)->stats.trefcnt);					\
	DECR_TREFCNT(lvp);							\
	if (0 == (lvp)->stats.trefcnt)						\
	{	/* This lv_val is done .. requeue it after it is killed */ 	\
		LVP_KILL_SUBTREE_IF_EXISTS(lvp, dotpsave);			\
		LV_FREESLOT(lvp);						\
	} else									\
		assert((lvp)->stats.trefcnt >= (lvp)->stats.crefcnt);		\
}

/* Macro to decrement an alias container reference and do appropriate cleanup */
#define DECR_AC_REF(lvp, dotpsave)								\
{												\
	if (MV_ALIASCONT & (lvp)->v.mvtype)							\
	{	/* Killing an alias container, perform reference count maintenance */		\
												\
		GBLREF	uint4	dollar_tlevel;							\
												\
		lv_val	*lvref = (lv_val *)(lvp)->v.str.addr;					\
		assert(0 == (lvp)->v.str.len);							\
		assert(!LV_IS_BASE_VAR(lvp));							\
		assert(lvref);									\
		assert(LV_IS_BASE_VAR(lvref));							\
		assert(0 < lvref->stats.crefcnt);						\
		assert(0 < lvref->stats.trefcnt);						\
		if (dotpsave && dollar_tlevel && (NULL != lvref->tp_var)			\
			&& !lvref->tp_var->var_cloned && (1 == lvref->stats.trefcnt))		\
			/* Only clone (here) if target is going to be deleted by decrement */	\
			TP_VAR_CLONE(lvref);							\
		DECR_CREFCNT(lvref);								\
		DECR_BASE_REF_NOSYM(lvref, dotpsave);						\
	}											\
}

/* Macro to mark nested symvals as having had alias activity. Mark nested symvals until we get
 * to a symval owning the lv_val specified. This loop will normally only run once except in the
 * case where the lv_val given is owned by a symval nested by an exclusive NEW.
 */
#define MARK_ALIAS_ACTIVE(lv_own_svlvl)									\
{													\
	symval	*sv;											\
	int4	lcl_own_svlvl;										\
													\
	lcl_own_svlvl = lv_own_svlvl;									\
	for (sv = curr_symval; ((NULL != sv) && (sv->symvlvl >= lcl_own_svlvl)); sv = sv->last_tab)	\
		sv->alias_activity = TRUE;								\
}

/* The following *_CNTNRS_IN_TREE macros scan the supplied tree for container vars. Note that in debug mode,
 * even if "has_aliascont" is NOT set, we will still scan the array for containers but if one is found, then we will assert fail.
 * Note this means the processing is different for PRO and DBG builds in that this routine will not even be called in PRO if
 * the has_aliascont flag is not on in the base mval. But this allows us to check the integrity of the has_aliascont flag
 * in DBG because we will fail if ever a container is found in an array with the flag turned off.
 */

/* Macro to scan a lvTree for container vars and for each one, treat it as if it had been specified in a TP restart variable list
 * by setting it up to be cloned if deleted. This activity should nest so container vars that point to further trees should also
 * be scanned.
 */
#define TPSAV_CNTNRS_IN_TREE(lv_base)												\
{																\
	lvTree	*lvt;														\
																\
	assert(LV_IS_BASE_VAR(lv_base));											\
        if (lv_base->stats.tstartcycle != tstartcycle)										\
	{	/* If haven't processed this lv_val for this transaction (or nested transaction */				\
		lv_base->stats.tstartcycle = tstartcycle;									\
		/* Note it is possible that this lv_val has the current tstart cycle if there has been a restart. We still need	\
		   to rescan the var anyway since one attempt can execute differently than a following attempt and thus create	\
		   different variables for us to find -- if it has aliases in it that is.					\
		*/														\
		if ((NULL != (lvt = LV_GET_CHILD(lv_base))) PRO_ONLY(&& (lv_base)->has_aliascont))				\
		{														\
			DBGRFCT((stderr, "\n## TPSAV_CNTNRS_IN_TREE: Beginning processing lvTree at 0x"lvaddr"\n", lv_base)); 	\
			als_prcs_tpsav_cntnr(lvt);										\
			DBGRFCT((stderr, "\n## TPSAV_CNTNRS_IN_TREE: Finished processing lvTree at 0x"lvaddr"\n", lv_base)); 	\
		}														\
	} else															\
		DBGRFCT((stderr, "\n## TPSAV_CNTNRS_IN_TREE: Bypassing lvTree at 0x"lvaddr" as already processed\n", lv_base));	\
}

/* Macro similar to TPSAV_CNTNRS_IN_TREE() above but in this case, we know we want to increment the reference counts
 * for all found container var targets. They have already been saved (we will assert they have a tp_var!) and we just want to
 * reestablish the reference counts. This is used when a saved array is being restored and the containers in it need to have
 * their reference counts re-established.
 */
#define TPREST_CNTNRS_IN_TREE(lv_base)											\
{															\
	lvTree	*lvt;													\
															\
	assert(LV_IS_BASE_VAR(lv_base));										\
	if ((NULL != (lvt = LV_GET_CHILD(lv_base))) PRO_ONLY(&& (lv_base)->has_aliascont))				\
	{														\
		DBGRFCT((stderr, "\n++ TPREST_CNTNRS_IN_TREE: Beginning processing lvTree at 0x"lvaddr"\n", lv_base));	\
		als_prcs_tprest_cntnr(lvt);										\
		DBGRFCT((stderr, "\n++ TPREST_CNTNRS_IN_TREE: Finished processing lvTree at 0x"lvaddr"\n", lv_base));	\
	}														\
}

/* Macro similar to TPREST_CNTNRS_IN_TREE() above but in this case we want to decrement the containers since we are
   in unwind processing */
#define TPUNWND_CNTNRS_IN_TREE(lv_base)												\
{																\
	lvTree	*lvt;														\
																\
	assert(LV_IS_BASE_VAR(lv_base));											\
	if ((NULL != (lvt = LV_GET_CHILD(lv_base))) PRO_ONLY(&& (lv_base)->has_aliascont))					\
	{															\
		DBGRFCT((stderr, "\n-- TPUNWND_CNTNRS_IN_TREE: Beginning processing lvTree at 0x"lvaddr"\n", lv_base));		\
		als_prcs_tpunwnd_cntnr(lvt);											\
		DBGRFCT((stderr, "\n-- TPUNWND_CNTNRS_IN_TREE: Finished processing lvTree at 0x"lvaddr"\n", lv_base));		\
	} else															\
		DBGRFCT((stderr, "\n-- TPUNWND_CNTNRS_IN_TREE: Scan for lvTree at 0x"lvaddr" bypassed - no containers", lv_base));\
}

/* Macro to scan a tree for container vars, delete what they point to and unmark the container so it is no longer a container */
#define KILL_CNTNRS_IN_TREE(lv_base)								\
{												\
	lvTree	*lvt;										\
												\
	assert(LV_IS_BASE_VAR(lv_base));							\
	if ((NULL != (lvt = LV_GET_CHILD(lv_base))) PRO_ONLY(&& (lv_base)->has_aliascont))	\
		als_prcs_kill_cntnr(lvt);							\
}

/* Macro to scan an lvval for containers pointing to other structures that need to be scanned in xnew pop processing */
#define XNEWREF_CNTNRS_IN_TREE(lv_base)								\
{												\
	lvTree	*lvt;										\
												\
	assert(LV_IS_BASE_VAR(lv_base));							\
	if ((NULL != (lvt = LV_GET_CHILD(lv_base))) PRO_ONLY(&& (lv_base)->has_aliascont))	\
		als_prcs_xnewref_cntnr(lvt);							\
}

/* Macro to mark the base frame of the current var as having a container */
#define MARK_CONTAINER_ONBOARD(lv_base) 									\
{														\
	assert(LV_IS_BASE_VAR(lv_base));									\
	lv_base->has_aliascont = TRUE;										\
}

void als_lsymtab_repair(hash_table_mname *table, ht_ent_mname *table_base_orig, int table_size_orig);
void als_check_xnew_var_aliases(symval *oldsymtab, symval *cursymtab);
void als_zwrhtab_init(void);
void als_prcs_tpsav_cntnr(lvTree *lvt);
void als_prcs_tprest_cntnr(lvTree *lvt);
void als_prcs_tpunwnd_cntnr(lvTree *lvt);
void als_prcs_kill_cntnr(lvTree *lvt);
void als_prcs_xnewref_cntnr(lvTree *lvt);

ht_ent_mname *als_lookup_base_lvval(lv_val *lvp);
zwr_alias_var *als_getzavslot(void);
int als_lvval_gc(void);
DBGALS_ONLY(void als_lvmon_output(void);)

#endif 	    /* !ALIAS_H_ */
