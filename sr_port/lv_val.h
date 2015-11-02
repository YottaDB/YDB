/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


#ifndef LVVAL_H_INCLUDED
#define LVVAL_H_INCLUDED

/*
 * CAUTION ----	The structures lv_val, symval, and lv_sbs_tbl are all
 *		treated as if they were fields of a union.  The fields
 *		lv_val.v.mvtype, symval.ident, and lv_sbs_tbl.ident
 *		MUST be aligned.
 */

#include "hashtab_mname.h"

/* Queue an lv_val or lv_sbs_tbl block back on the lv_val free list at the give anchor.
   Operations:
   1) Debugging aids in debug builds.
   2) Do the queueing.
   3) Clear the mv_type so it is definitely a deleted value.
*/
#define LV_FLIST_ENQUEUE(flist_ptr, lv_ptr)									\
	{													\
		lv_val **savflist_ptr = (flist_ptr);								\
		DBGRFCT((stderr, "\n<< Free list queueing of lv_val/sbs_blk at 0x"lvaddr" by %s line %d\n",	\
			 (lv_ptr), __FILE__, __LINE__));							\
		DEBUG_ONLY(memset((lv_ptr), 0xfd, SIZEOF(lv_val)));						\
		(lv_ptr)->ptrs.free_ent.next_free = *savflist_ptr;						\
		*savflist_ptr = (lv_ptr);									\
		(lv_ptr)->v.mvtype = 0;	/* clears any use as MV_ALIASCONT */ 					\
		(lv_ptr)->ptrs.val_ent.parent.sym = 0;								\
		DBGALS_ONLY((lv_ptr)->stats.lvmon_mark = FALSE);						\
	}

/* Increment the cycle for tstarts. Field is compared to same name field in lv_val to signify an lv_val has been seen
   during a given transaction so reference counts are kept correct. If counter wraps, clear all the counters in all
   accessible lv_vals.
*/
#define INCR_TSTARTCYCLE												\
	{														\
		symval		*lvlsymtab;										\
		lv_blk		*lvbp; 											\
		lv_val		*lvp, *lvp_top;										\
		if (0 == ++tstartcycle) 										\
		{	/* Set tstart cycle in all active lv_vals to 0 */						\
			for (lvlsymtab = curr_symval; lvlsymtab; lvlsymtab = lvlsymtab->last_tab)			\
				for (lvbp = &curr_symval->first_block; lvbp; lvbp = lvbp->next)				\
					for (lvp = lvbp->lv_base, lvp_top = lvbp->lv_free; lvp < lvp_top; lvp++)	\
						if (MV_SBS != lvp->v.mvtype)						\
							lvp->stats.tstartcycle = 0;					\
			tstartcycle = 1;										\
		}													\
	}



/* Increment the cycle for misc lv tasks. Field is compared to same name field in lv_val to signify an lv_val has been seen
   during a given transaction so reference counts are kept correct. If counter wraps, clear all the counters in all
   accessible lv_vals.
*/
#define INCR_LVTASKCYCLE												\
	{														\
		symval		*lvlsymtab;										\
		lv_blk		*lvbp; 											\
		lv_val		*lvp, *lvp_top;										\
		if (0 == ++lvtaskcycle) 										\
		{	/* Set tstart cycle in all active lv_vals to 0 */						\
			for (lvlsymtab = curr_symval; lvlsymtab; lvlsymtab = lvlsymtab->last_tab)			\
				for (lvbp = &curr_symval->first_block; lvbp; lvbp = lvbp->next)				\
					for (lvp = lvbp->lv_base, lvp_top = lvbp->lv_free; lvp < lvp_top; lvp++)	\
						if (MV_SBS != lvp->v.mvtype)						\
							lvp->stats.lvtaskcycle = 0; 					\
			lvtaskcycle = 1;										\
		}													\
	}

/* Initialize given lv_val */
#define LVVAL_INIT(lv, symvalarg)											\
	{														\
		(lv)->v.mvtype = 0;											\
		(lv)->stats.trefcnt = 1;										\
		(lv)->stats.crefcnt = 0;										\
		(lv)->stats.tstartcycle = 0;										\
		(lv)->stats.lvtaskcycle = 0;										\
		(lv)->has_aliascont = FALSE;										\
		DBGALS_ONLY(if (lvmon_enabled) (lv)->stats.lvmon_mark = TRUE; else (lv)->stats.lvmon_mark = FALSE);	\
		(lv)->tp_var = NULL;											\
		(lv)->ptrs.val_ent.children = NULL;									\
		(lv)->ptrs.val_ent.parent.sym = symvalarg;								\
	}

/* Macro to call lv_var_clone and set the cloned status in the tp_var structure */
#define TP_VAR_CLONE(lv)			\
{						\
	assert((lv)->tp_var);			\
	assert(!((lv)->tp_var->var_cloned));	\
	assert((lv)->tp_var->save_value);	\
	lv_var_clone((lv)->tp_var->save_value);	\
	(lv)->tp_var->var_cloned = TRUE;	\
}

/* Macro to indicate if a given lv_val is an alias or not */
#define IS_ALIASLV(lv) (((1 < (lv)->stats.trefcnt) || (0 < (lv)->stats.crefcnt)) \
			DEBUG_ONLY(&& assert(MV_SYM == (lv)->ptrs.val_ent.parent.sym->ident)))

typedef struct lv_sbs_tbl_struct
{	/* Note this structure is allocated from lv_getslot() which also allocates lv_val structures so its size
	   must be less than or equal to an lv_val. */
       	unsigned short			ident;
	unsigned char			filler;
       	unsigned char	       		level;
       	boolean_t      	       	       	int_flag;
	struct sbs_blk_struct		*str;
       	struct sbs_blk_struct		*num;
	struct lv_val_struct		*lv;
       	struct symval_struct   	       	*sym;
} lv_sbs_tbl;

typedef struct lv_val_struct
{
	mval 				v;			/* Value associated with this lv_val */
	struct
	{	/* Note these flags are irrelevant for other than base (unsubscripted) local vars */
		int4			trefcnt;		/* Total refcnt (includes container vars) */
		int4			crefcnt;		/* Container reference count */
		uint4			tstartcycle;		/* Cycle of level 0 tstart command */
		uint4			lvtaskcycle;		/* Cycle of various lv related tasks */
	} stats;
	boolean_t			has_aliascont;		/* This base var has or had an alias container in it */
	boolean_t			lvmon_mark;		/* This lv_val is being monitored */
	struct tp_var_struct		*tp_var;
	union
	{
		struct
 		{
       	       	       	lv_sbs_tbl		*children;
			union
			{	/* Note these two fields are still available when mvtype == MV_LVCOPIED and there is
				   code in als_check_xnew_var_aliases() that depends on this */
				struct symval_struct   	*sym;
				lv_sbs_tbl		*sbs;
			} parent;
		} val_ent;
		struct
		{
		 	struct lv_val_struct *next_free;
		} free_ent;
		struct
		{	/* When xnew'd lv's are copied to previous symtab, their new root is
			   set here so multiple references can be resolved properly (mvtype == MV_LVCOPIED */
			struct lv_val_struct *newtablv;
		} copy_loc;
	} ptrs;
} lv_val;

typedef struct lv_blk_struct
{
	lv_val *lv_base, *lv_free, *lv_top;
	struct lv_blk_struct *next;
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

/*
 * CAUTION ----	symval.sbs_que must have exactly the same position, form,
 *		and name as the corresponding field in sbs_blk.
 */
typedef struct symval_struct
{
       	unsigned short			ident;
	unsigned char			tp_save_all;
	unsigned char			filler1;
	boolean_t			alias_activity;
	struct
	{
		struct sbs_blk_struct	*fl, *bl;
	} sbs_que;
	lv_xnew_var			*xnew_var_list;
	lv_xnew_ref			*xnew_ref_list;
       	hash_table_mname		h_symtab;
	lv_blk	       			first_block;
	lv_val	       			*lv_flist;
       	struct symval_struct		*last_tab;
	int4				symvlvl;		/* Level of symval struct (nesting) */
	boolean_t			trigr_symval;		/* Symval is owned by a trigger */
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

lv_val *lv_getslot(symval *sym);
void lv_cnv_int_tbl(lv_sbs_tbl *tbl);
void lv_killarray(lv_sbs_tbl *a, boolean_t dotpsave);
void lv_newname(ht_ent_mname *hte, symval *sym);
void lv_zap_sbs(lv_sbs_tbl *tbl, lv_val *lv);
lv_blk *lv_newblock(lv_blk *block_addr, lv_blk *next_block, int size);
void lv_var_clone(lv_val *var);
void lv_kill(lv_val *lv, boolean_t dotpsave);

void op_zshow(mval *func, int type, lv_val *lvn);
void op_fndata(lv_val *x, mval *y);
void op_fnzdata(lv_val *x, mval *y);
void op_fno2(lv_val *src,mval *key,mval *dst,mval *direct);
void op_fnnext(lv_val *src,mval *key,mval *dst);
void op_fnorder(lv_val *src, mval *key, mval *dst);
void op_fnzprevious(lv_val *src, mval *key, mval *dst);
void op_kill(lv_val *lv);
void op_lvzwithdraw(lv_val *lv);
void op_setals2als(lv_val *src, int dstindx);
void op_setalsin2alsct(lv_val *src, lv_val *dst);
void op_setalsctin2als(lv_val *src, int dstindx);
void op_setalsct2alsct(lv_val *src, lv_val *dst);
void op_setfnretin2als(mval *srcmv, int destindx); /* no an lv_val ref but kept here with its friends so it not lonely */
void op_setfnretin2alsct(mval *srcmv, lv_val *dstlv);
void op_killalias(int srcindx);
void op_clralsvars(lv_val *dst);
void op_fnzahandle(lv_val *src, mval *dst);
void op_fnincr(lv_val *local_var, mval *increment, mval *result);

void lvzwr_var(lv_val *lv, int4 n);
unsigned char   *format_lvname(lv_val *start, unsigned char *buff, int size);
unsigned char *format_key_lv_val(lv_val *lvpin, unsigned char *buff, int size);

lv_val *op_srchindx(UNIX_ONLY_COMMA(int argcnt_arg) lv_val *lv, ...);
lv_val *op_m_srchindx(UNIX_ONLY_COMMA(int4 count) lv_val *lvarg, ...);
lv_val *op_putindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...);
lv_val *op_getindx(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...);

boolean_t lcl_arg1_is_desc_of_arg2(lv_val *cur, lv_val *ref);

#endif
