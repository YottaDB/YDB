/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


#ifndef LVVAL_H_INCLUDED
#define LVVAL_H_INCLUDED

#include <stddef.h> /* for OFFSETOF macro used in the IS_OFFSET_AND_SIZE_MATCH macro */

#include "hashtab_mname.h"
#include "lv_tree.h"

/* Define a few generic LV related macros. These macros work irrespective of whether the input is an unsubscripted
 * local variable (aka base variable) which is of type (lv_val *) or a subscripted local variable which is of type
 * (lvTreeNode *). All of these assume the layout of the two structures is very similar (asserted in "LV_TREE_CREATE"
 * macro). These macros will be used extensively for code centralization and to avoid code duplication.
 *
 * The macros that are only passed an LV_PTR need to ensure that it is indeed a "lv_val *" or a "lvTreeNode *".
 * The macros that are also passed an IS_BASE_VAR parameter can safely assume this to be the case since the IS_BASE_VAR
 *	parameter would have been computed using the LV_IS_BAS_VAR macro which already does the ensure.
 *	So we avoid the duplicate assert in these cases.
 */

#define	SYM_IS_SYMVAL(SYM)			(MV_SYM == (SYM)->ident)

#define	IS_LV_TREE(LVT)				(MV_LV_TREE == ((lvTree *)LVT)->ident)
#define	LV_PARENT(LV)				LV_AVLNODE_PARENT(LV)
#define	LV_AVLNODE_PARENT(PTR)			(((lvTreeNode *)PTR)->tree_parent)
#define	LVT_PARENT(LVT)				(((lvTree *)LVT)->sbs_parent)
#define	IS_LVAVLTREENODE(PTR)			(IS_LV_TREE(LV_AVLNODE_PARENT((lv_val *)PTR)))

#define	IS_PARENT_MV_SYM(LV_PTR)		(SYM_IS_SYMVAL(LV_SYMVAL((lv_val *)LV_PTR)))

#define	DBG_ASSERT_LVT(LVT)			DBG_ASSERT(IS_LV_TREE(LVT))	/* is "lvTree *" */

#define	DBG_ASSERT_LV_OR_TREENODE(LV_PTR)	DBG_ASSERT(IS_PARENT_MV_SYM(LV_PTR)	/* is "lv_val *" */	\
							|| IS_LVAVLTREENODE(LV_PTR))	/* is "lvTreeNode *" or "lvTreeNodeFlt *" */

#define	LV_IS_BASE_VAR(LV_PTR)			(DBG_ASSERT_LV_OR_TREENODE(LV_PTR)		\
							IS_PARENT_MV_SYM(LV_PTR))

/* Input to macro is LV_PTR which is guaranteed to be a non-base lv. Return value is the corresponding base_lv */
#define	LV_GET_BASE_VAR(LV_PTR)		(DBG_ASSERT(!LV_IS_BASE_VAR(LV_PTR))			\
						DBG_ASSERT(NULL != LV_AVLNODE_PARENT(LV_PTR))	\
						LV_AVLNODE_PARENT(LV_PTR)->base_lv)

#define	LVT_GET_BASE_VAR(LVT)		(DBG_ASSERT_LVT(LVT)		\
						LVT->base_lv)

#define	LVT_GET_SYMVAL(LVT)		(DBG_ASSERT_LVT(LVT)		\
						LV_GET_SYMVAL(LVT_GET_BASE_VAR(LVT)))

#define	LV_GET_PARENT_TREE(LV_PTR)	(DBG_ASSERT(!LV_IS_BASE_VAR(LV_PTR))			\
						DBG_ASSERT(NULL != LV_AVLNODE_PARENT(LV_PTR))	\
						LV_AVLNODE_PARENT(LV_PTR))

/* The following 3 macros operate on the ptrs.val_ent.children field.
 * If the situation demands returning the children ptr, then use LV_GET_CHILD or LV_CHILD macros in that order of preference.
 * If the situation demands checking if any children exist, then use LV_HAS_CHILD macro.
 */
#define	LV_CHILD(LV_PTR)			(((lv_val *)LV_PTR)->ptrs.val_ent.children)

/* The following macro is an enhanced version of LV_CHILD that can be used wherever you would prefer or dont mind additional
 * assert checking. In some cases, this cannot be used (e.g. in the left hand side of an assignment operation). But otherwise
 * it is preferrable to use this macro as opposed to LV_CHILD.
 */
#define	LV_GET_CHILD(LV_PTR)			(DBG_ASSERT_LV_OR_TREENODE(LV_PTR)			\
							LV_CHILD(LV_PTR))				\

/* if an lv_ptr's chlidren tree pointer is non-NULL, we should be guaranteed (by lv_kill)
 * that there is at least one child node in that subscript tree (currently only an avl tree).
 * assert that as well.
 */
#define	LV_HAS_CHILD(LV_PTR)		(DBG_ASSERT_LV_OR_TREENODE(LV_PTR)					\
						(NULL != LV_CHILD(LV_PTR))					\
						DEBUG_ONLY(&& assert(MV_LV_TREE == (LV_CHILD(LV_PTR))->ident))	\
						DEBUG_ONLY(&& assert((LV_CHILD(LV_PTR))->avl_height)))

/* Like the LV_CHILD and LV_GET_CHILD macro variants, the below macros need to be used with preference to LV_GET_SYMVAL
 * unless it is needed in the left hand side of an assignment in which case use the LV_SYMVAL macro.
 */
#define	LV_SYMVAL(BASE_VAR)			((BASE_VAR)->ptrs.val_ent.parent.sym)

#define	LV_GET_SYMVAL(BASE_VAR)			(DBG_ASSERT(LV_IS_BASE_VAR(BASE_VAR))		\
							LV_SYMVAL(BASE_VAR))

/* The below macro relies on the fact that the parent field is at the same offset in an "lv_val" as it is in a "lvTreeNode".
 * This is asserted in "LV_TREE_CREATE" macro.
 */
#define	LV_GET_PARENT(LV_PTR)			(DBG_ASSERT_LV_OR_TREENODE(LV_PTR)	\
							LV_AVLNODE_PARENT(LV_PTR))
#define	LVT_GET_PARENT(LVT)			(DBG_ASSERT_LVT(LVT)			\
							LVT_PARENT(LVT))

#define	LV_IS_VAL_DEFINED(LV_PTR)		(DBG_ASSERT_LV_OR_TREENODE(LV_PTR)	\
							MV_DEFINED(&((lv_val *)LV_PTR)->v))

#define	LV_SBS_DEPTH(LV_PTR, IS_BASE_VAR, DEPTH)					\
{											\
	assert(IS_OFFSET_AND_SIZE_MATCH(lvTree, sbs_depth, symval, sbs_depth));		\
	assert(!IS_BASE_VAR || (0 == LV_PTR->ptrs.val_ent.parent.sbs_tree->sbs_depth));	\
	assert(IS_BASE_VAR || (0 < LV_PTR->ptrs.val_ent.parent.sbs_tree->sbs_depth));	\
	DEPTH = LV_PTR->ptrs.val_ent.parent.sbs_tree->sbs_depth;			\
}

/* Mark mval held by lv_ptr to be undefined. Also lets stp_gcol and lv_gcol know to NOT protect
 * (from garbage collection) any strings this lv_val was pointing to at the time of the free.
 */
#define	LV_VAL_CLEAR_MVTYPE(LVPTR)	(LVPTR)->v.mvtype = 0;	/* note: also clears any use as MV_ALIASCONT */

/* Queue an lv_val block back on the lv_val free list at the given anchor.
 * Operations:
 * 1) Debugging aids in debug builds.
 * 2) Do the queueing.
 * 3) Clear the mv_type so it is definitely a deleted value.
 *
 * Callers should use LV_FREESLOT instead of directly invoking LV_FLIST_ENQUEUE.
 * There are few exceptions like unw_mv_ent.c.
 */
#define LV_FLIST_ENQUEUE(flist_ptr, lv_ptr)							\
{												\
	lv_val **savflist_ptr = (flist_ptr);							\
	DBGRFCT((stderr, "\n<< Free list queueing of lv_val at 0x"lvaddr" by %s line %d\n",	\
		 (lv_ptr), __FILE__, __LINE__));						\
	assert(LV_IS_BASE_VAR(lv_ptr));								\
	/* assert that any subtree underneath this lv_ptr has already been freed up */		\
	assert(NULL == LV_CHILD(lv_ptr));							\
	LV_VAL_CLEAR_MVTYPE(lv_ptr);								\
	DEBUG_ONLY(memset((lv_ptr), 0xfd, SIZEOF(lv_val)));					\
	(lv_ptr)->ptrs.free_ent.next_free = *savflist_ptr;					\
	*savflist_ptr = (lv_ptr);								\
	LV_SYMVAL(lv_ptr) = NULL;								\
	DBGALS_ONLY((lv_ptr)->lvmon_mark = FALSE);						\
}

/* Increment the cycle for tstarts. Field is compared to same name field in lv_val to signify an lv_val has been seen
   during a given transaction so reference counts are kept correct. If counter wraps, clear all the counters in all
   accessible lv_vals.
*/
#define INCR_TSTARTCYCLE												\
{															\
	symval		*lvlsymtab;											\
	lv_blk		*lvbp;												\
	lv_val		*lvp, *lvp_top;											\
															\
	if (0 == ++tstartcycle)												\
	{	/* Set tstart cycle in all active lv_vals to 0 */							\
		for (lvlsymtab = curr_symval; lvlsymtab; lvlsymtab = lvlsymtab->last_tab)				\
			for (lvbp = curr_symval->lv_first_block; lvbp; lvbp = lvbp->next)				\
				for (lvp = (lv_val *)LV_BLK_GET_BASE(lvbp), lvp_top = LV_BLK_GET_FREE(lvbp, lvp);	\
						lvp < lvp_top; lvp++)							\
					lvp->stats.tstartcycle = 0;							\
		tstartcycle = 1;											\
	}														\
}

/* Increment the cycle for misc lv tasks. Field is compared to same name field in lv_val to signify an lv_val has been seen
   during a given transaction so reference counts are kept correct. If counter wraps, clear all the counters in all
   accessible lv_vals.
*/
#define INCR_LVTASKCYCLE												\
{															\
	symval		*lvlsymtab;											\
	lv_blk		*lvbp;												\
	lv_val		*lvp, *lvp_top;											\
															\
	if (0 == ++lvtaskcycle)												\
	{	/* Set tstart cycle in all active lv_vals to 0 */							\
		for (lvlsymtab = curr_symval; lvlsymtab; lvlsymtab = lvlsymtab->last_tab)				\
			for (lvbp = curr_symval->lv_first_block; lvbp; lvbp = lvbp->next)				\
				for (lvp = (lv_val *)LV_BLK_GET_BASE(lvbp), lvp_top = LV_BLK_GET_FREE(lvbp, lvp);	\
						lvp < lvp_top; lvp++)							\
					lvp->stats.lvtaskcycle = 0;							\
		lvtaskcycle = 1;											\
	}														\
}

/* Initialize given lv_val (should be of type "lv_val *" and not "lvTreeNode *") */
#define LVVAL_INIT(lv, symvalarg)											\
{															\
	DBGALS_ONLY(GBLREF boolean_t lvmon_enabled;)									\
	assert(MV_SYM == symvalarg->ident); /* ensure above macro is never used to initialize a "lvTreeNode *" */	\
	(lv)->v.mvtype = 0;												\
	(lv)->stats.trefcnt = 1;											\
	(lv)->stats.crefcnt = 0;											\
	(lv)->stats.tstartcycle = 0;											\
	(lv)->stats.lvtaskcycle = 0;											\
	(lv)->has_aliascont = FALSE;											\
	DBGALS_ONLY(if (lvmon_enabled) (lv)->lvmon_mark = TRUE; else (lv)->lvmon_mark = FALSE);				\
	(lv)->tp_var = NULL;												\
	LV_CHILD(lv) = NULL;												\
	LV_SYMVAL(lv) = symvalarg;											\
}

/* Macro to call lv_var_clone and set the cloned status in the tp_var structure.
 * Note that we want the cloned tree to have its base_lv point back to "lv" and not the cloned lv.
 * This is because in case we want to restore the lv, we can then safely move the saved tree
 * back to "lv" without then having to readjust the base_lv linked in the "lvTree *" structures beneath
 */
#define TP_VAR_CLONE(lv)				\
{							\
	lv_val 	*lcl_savelv;				\
	tp_var	*lcl_tp_var;				\
							\
	assert(LV_IS_BASE_VAR(lv));			\
	lcl_tp_var = (lv)->tp_var;			\
	assert(lcl_tp_var);				\
	assert(!lcl_tp_var->var_cloned);		\
	lcl_savelv = lcl_tp_var->save_value;		\
	assert(NULL != lcl_savelv);			\
	assert(NULL == LV_CHILD(lcl_savelv));		\
	LV_CHILD(lcl_savelv) = LV_CHILD(lv);		\
	lv_var_clone(lcl_savelv, lv);			\
	lcl_tp_var->var_cloned = TRUE;			\
}

/* Macro to indicate if a given lv_val is an alias or not */
#define IS_ALIASLV(lv) (DBG_ASSERT(LV_IS_BASE_VAR(lv)) ((1 < (lv)->stats.trefcnt) || (0 < (lv)->stats.crefcnt)) \
			DEBUG_ONLY(&& assert(IS_PARENT_MV_SYM(lv))))


#define	LV_NEWBLOCK_INIT_ALLOC			16
#define	LV_BLK_GET_BASE(LV_BLK)			(((sm_uc_ptr_t)LV_BLK) + SIZEOF(lv_blk))
#define	LV_BLK_GET_FREE(LV_BLK, LVBLK_BASE)	(&LVBLK_BASE[LV_BLK->numUsed])

typedef struct lv_val_struct
{
	mval 				v;			/* Value associated with this lv_val */
	/* Note: The offsets of "ptrs.val_ent.children" and "ptrs.val_ent.parent.sym" are relied upon by
	* other modules (e.g. lv_tree.h) and asserted in LV_TREE_CREATE macro. */
	union
	{
		struct
 		{
       	       	       	lvTree		*children;
			union
			{	/* Note these two fields are still available when mvtype == MV_LVCOPIED
				 * and there is code in "als_check_xnew_var_aliases" that depends on this
				 */
				struct symval_struct   	*sym;
				lvTree			*sbs_tree;
			} parent;
		} val_ent;
		struct
		{
		 	struct lv_val_struct *next_free;
		} free_ent;
		struct
		{	/* When xnew'd lv's are copied to previous symtab, their new root is
			 * set here so multiple references can be resolved properly (mvtype == MV_LVCOPIED)
			 */
			struct lv_val_struct *newtablv;
		} copy_loc;
	} ptrs;
	struct
	{	/* Note these flags are irrelevant for other than base (unsubscripted) local vars */
		int4			trefcnt;		/* Total refcnt (includes container vars) */
		int4			crefcnt;		/* Container reference count */
		uint4			tstartcycle;		/* Cycle of level 0 tstart command */
		uint4			lvtaskcycle;		/* Cycle of various lv related tasks */
	} stats;
	boolean_t			has_aliascont;		/* This base var has or had an alias container in it */
	boolean_t			lvmon_mark;		/* This lv_val is being monitored; Used only #ifdef DEBUG_ALIAS */
	struct tp_var_struct		*tp_var;
} lv_val;

typedef struct lv_blk_struct
{
	struct lv_blk_struct	*next;
	uint4			numAlloc;
	uint4			numUsed;
} lv_blk;

/* When op_xnew creates a symtab, these blocks will describe the vars that were passed through from the
   previous symtab. They need special alias processing. Note we keep our own copy of the key (rather than
   pointing to the hash table entry) since op_xnew processing can cause a hash table expansion and we have
   no good way to update pointers so save the hash values as part of the key to eliminate another lookup.
*/
typedef struct lv_xnew_var_struct
{
	struct lv_xnew_var_struct	*next;
	mname_entry			key;
	lv_val				*lvval;		/* There are two uses for this field. In op_new, it is used to
							   hold the previous lvval addr for the 2nd pass. In unwind
							   processing (als_check_xnew_var_aliases) it holds the lvval
							   in the symtab being popped since it cannot be obtained once the
							   symtab entry is deleted in the first pass (step 2).
							*/
} lv_xnew_var;
/* While lv_xnew_var_struct are the structures that were explicitly passed through, this is a list of the structures
   that are pointed to by any container vars in any of the passed through vars and any of the vars those point to, etc.
   The objective is to come up with a definitive list of structures to search to see if containers got created in them
   that point to the structure being torn down.
*/
typedef struct lv_xnewref_struct
{
	struct lv_xnewref_struct	*next;
	lv_val				*lvval;		/* This structure can be addressed through the passed thru vars
							   but is not itself one of them */
} lv_xnew_ref;

typedef struct symval_struct
{
       	unsigned short		ident;
	unsigned short		sbs_depth;  /* is always 0. Defined to match offset & size of lvTree->sbs_depth.
					     * This way callers can avoid an if check depending on whether
					     * the input pointer is a "symval *" or a "lvTree *" type.
					     * This is also asserted in "LV_TREE_CREATE" macro. */
	boolean_t		tp_save_all;
	lv_xnew_var		*xnew_var_list;
	lv_xnew_ref		*xnew_ref_list;
	hash_table_mname	h_symtab;
	lv_blk			*lv_first_block;
	lv_blk			*lvtree_first_block;
	lv_blk			*lvtreenode_first_block;
	lv_val			*lv_flist;
	lvTree			*lvtree_flist;
	lvTreeNode		*lvtreenode_flist;
	struct symval_struct	*last_tab;
	int4			symvlvl;		/* Level of symval struct (nesting) */
	boolean_t		trigr_symval;		/* Symval is owned by a trigger */
	boolean_t		alias_activity;
} symval;

/* Structure to describe the block allocated to describe a var specified on a TSTART to be restored
   on a TP restart. Block moved here from tpframe.h due to the structure references it [now] makes.
   Block can take two forms: (1) the standard tp_data form which marks vars to be modified or (2) the
   tp_change form where a new symbol table was stacked.
*/
typedef struct tp_var_struct
{
	struct tp_var_struct		*next;
	struct lv_val_struct		*current_value;
	struct lv_val_struct		*save_value;
	mname_entry			key;
	boolean_t			var_cloned;
	GTM64_ONLY(int4			filler;)
} tp_var;

typedef struct lvname_info_struct
{
	intszofptr_t	total_lv_subs; /* Total subscripts + 1 for name itself */
	lv_val		*start_lvp;
	mval 		*lv_subs[MAX_LVSUBSCRIPTS];
	lv_val		*end_lvp;
} lvname_info;
typedef lvname_info	*lvname_info_ptr;

#define	DOTPSAVE_FALSE		FALSE	/* macro to indicate parameter by name "dotpsave" is passed a value of "FALSE" */
#define	DOTPSAVE_TRUE		TRUE	/* macro to indicate parameter by name "dotpsave" is passed a value of "TRUE"  */

#define	DO_SUBTREE_FALSE	FALSE	/* macro to indicate parameter by name "do_subtree" is passed a value of "FALSE" */
#define	DO_SUBTREE_TRUE		TRUE	/* macro to indicate parameter by name "do_subtree" is passed a value of "TRUE"  */

#define	LV_FREESLOT(LV)						\
{								\
	symval	*sym;						\
								\
	assert(LV_IS_BASE_VAR(LV));				\
	sym = LV_GET_SYMVAL(LV);				\
	LV_FLIST_ENQUEUE(&sym->lv_flist, LV); 			\
}

#define	LVTREE_FREESLOT(LVT)						\
{									\
	symval	*sym;							\
									\
	sym = LVT_GET_SYMVAL(LVT);					\
	assert(NULL != LVT_GET_PARENT(LVT));				\
	LVT_PARENT(LVT) = NULL;	/* indicates this is free */		\
	/* avl_root is overloaded to store linked list in free state */	\
	LVT->avl_root = (lvTreeNode *)sym->lvtree_flist;			\
	sym->lvtree_flist = LVT;					\
}

#define	LVTREENODE_FREESLOT(LV)								\
{											\
	symval	*sym;									\
	lv_val	*base_lv;								\
											\
	assert(!LV_IS_BASE_VAR(LV));							\
	base_lv = LV_GET_BASE_VAR(LV);							\
	sym = LV_GET_SYMVAL(base_lv);							\
	LV_AVLNODE_PARENT(LV) = NULL;	/* indicates to stp_gcol this is free */	\
	/* sbs_child is overloaded to store linked list in free state */		\
	(LV)->sbs_child = (lvTree *)sym->lvtreenode_flist;				\
	sym->lvtreenode_flist = LV;							\
}

unsigned char   *format_lvname(lv_val *start, unsigned char *buff, int size);
lv_val		*lv_getslot(symval *sym);
lvTree		*lvtree_getslot(symval *sym);
lvTreeNode	*lvtreenode_getslot(symval *sym);

void	lv_kill(lv_val *lv, boolean_t dotpsave, boolean_t do_subtree);
void	lv_killarray(lvTree *lvt, boolean_t dotpsave);
void	lv_newblock(symval *sym, int numElems);
void	lv_newname(ht_ent_mname *hte, symval *sym);
void	lvtree_newblock(symval *sym, int numElems);
void	lvtreenode_newblock(symval *sym, int numElems);
void	lv_var_clone(lv_val *clone_var, lv_val *base_lv);
void	lvzwr_var(lv_val *lv, int4 n);

void	op_clralsvars(lv_val *dst);
void	op_fndata(lv_val *x, mval *y);
void	op_fnzdata(lv_val *x, mval *y);
void	op_fnincr(lv_val *local_var, mval *increment, mval *result);
void	op_fnnext(lv_val *src,mval *key,mval *dst);
void	op_fno2(lv_val *src,mval *key,mval *dst,mval *direct);
void	op_fnorder(lv_val *src, mval *key, mval *dst);
void	op_fnzahandle(lv_val *src, mval *dst);
void	op_fnzprevious(lv_val *src, mval *key, mval *dst);
void	op_kill(lv_val *lv);
void	op_killalias(int srcindx);
void	op_lvzwithdraw(lv_val *lv);
void	op_setals2als(lv_val *src, int dstindx);
void	op_setalsin2alsct(lv_val *src, lv_val *dst);
void	op_setalsctin2als(lv_val *src, int dstindx);
void	op_setalsct2alsct(lv_val *src, lv_val *dst);
void	op_setfnretin2als(mval *srcmv, int destindx); /* no an lv_val ref but kept here with its friends so it not lonely */
void	op_setfnretin2alsct(mval *srcmv, lv_val *dstlv);
void	op_zshow(mval *func, int type, lv_val *lvn);

lv_val   *op_getindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...);
lv_val   *op_putindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...);
lv_val   *op_srchindx(UNIX_ONLY_COMMA(int argcnt_arg) lv_val *lv, ...);
lv_val   *op_m_srchindx(UNIX_ONLY_COMMA(int4 count) lv_val *lvarg, ...);

/* Function Prototypes for local variables functions of merge */
boolean_t 	lcl_arg1_is_desc_of_arg2(lv_val *cur, lv_val *ref);
unsigned char	*format_key_mvals(unsigned char *buff, int size, lvname_info *lvnp);
unsigned char   *format_key_lv_val(lv_val *lvpin, unsigned char *buff, int size);
#endif
