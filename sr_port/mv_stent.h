/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MV_STENT_H
#define MV_STENT_H

#include "io.h"
#include "lv_val.h"

typedef struct
{
	ht_ent_mname		*hte_addr;	/* Hash table entry for name (updated automagically when ht expands) */
	struct lv_val_struct	*save_value;	/* value to restore to hashtab and lst_addr (if supplied) on pop */
	DEBUG_ONLY(var_tabent	*nam_addr;)	/* Name associated with hash table entry for the saved value (for asserts) */
} mvs_ntab_struct;

/* PVAL includes NTAB plus a pointer to the new value created which may or may not be in the hashtable anymore
   by the time this is unstacked but it needs to be cleaned up when the frame pops.
*/
typedef struct
{
	struct lv_val_struct	*mvs_val;	/* lv_val created to hold new value */
	mvs_ntab_struct 	mvs_ptab;	/* Restoration-on-pop info */
} mvs_pval_struct;

/* NVAL is similar to PVAL except when dealing with indirect frames, the name of the var in the indirect frame's l_symtab
   can disappear before we are done with our potential need for it so the extra "name" field in this block allows us to
   cache the reference to it on the stack so it doesn't disappear.

   Note routine lvval_gcol() in alias_funcs.c has a dependency that mvs_ptab in PVAL and NVAL are at same offset within
   the structure.

   Note the extra "name" field is only carried in DEBUG mode. In non-Debug builds, the NVAL is the same as the PVAL.
*/
typedef struct
{
	struct lv_val_struct	*mvs_val;	/* lv_val created to hold new value */
	mvs_ntab_struct 	mvs_ptab;	/* Restoration-on-pop info */
	DEBUG_ONLY(var_tabent	name;)		/* name.var_name.addr points to whatever the original vartab entry points to */
} mvs_nval_struct;

/* Structure put on stack to effect transfer of arguments to called M routines */
typedef struct
{
	uint4			mask;
	unsigned int		actualcnt;
	struct lv_val_struct	*actuallist[1];
} parm_blk;

typedef struct
{
	boolean_t		save_truth;
	mval			*ret_value;
	parm_blk		*mvs_parmlist;
} mvs_parm_struct;

typedef struct
{
	boolean_t		saved_dollar_truth;
	mval			savtarg;	/* Current gv_currkey */
	mstr			savextref;	/* Current extended reference name component (if any) */
} mvs_zintr_struct;

typedef struct
{
	struct io_desc_struct	*io_ptr;	/* associated device structure */
	boolean_t		buffer_valid;	/* if TRUE, need to update curr_sp_buffer during stp_gcol */
	mstr			curr_sp_buffer;	/* buffer space in stringpool */
} mvs_zintdev_struct;

typedef struct mv_stent_struct
{
	unsigned int 			mv_st_type : 4;
	unsigned int 			mv_st_next : 28;
	union
	{
		mval			mvs_mval;
		lv_val			*mvs_lvval;
		struct
		{
			mval		v;
			mval		*addr;
		} mvs_msav;
		struct symval_struct	*mvs_stab;
		struct
		{
			unsigned short	iarr_mvals;
			unsigned char 	*iarr_base;
		} mvs_iarr;
		mvs_ntab_struct		mvs_ntab;
		mvs_parm_struct		mvs_parm;
		mvs_zintr_struct	mvs_zintr;
		mvs_zintdev_struct	mvs_zintdev;
		mvs_pval_struct		mvs_pval;
		mvs_nval_struct		mvs_nval;
		struct
		{
			void		**mvs_stck_addr;
			void		*mvs_stck_val;
			int4		mvs_stck_size;
		} mvs_stck;
		int4			mvs_tval;
	  	int4			mvs_tp_holder;
	} mv_st_cont;
} mv_stent;

mval *unw_mv_ent(mv_stent *mv_st_ent);
void push_stck(void* val, int val_size, void** addr, int mvst_stck_type);

#define MVST_MSAV 0	/* An mval and an address to store it at pop time, most
			   often used to save/restore new'd intrinsic variables.
			   This is important because the restore addr is fixed,
			   and no extra work is required to resolve it. */
#define MVST_MVAL 1	/* An mval which will be dropped at pop time */
#define MVST_STAB 2	/* A symbol table */
#define MVST_IARR 3	/* An array of (literal or temp) mval's and mstr's on the stack, due to indirection */
#define	MVST_NTAB 4	/* A place to save old name hash table values during parameter passed functions (used for dotted
			 * parm/alias)  */
#define	MVST_PARM 5	/* A pointer to a parameter passing block */
#define	MVST_PVAL 6	/* A temporary mval for formal parameters or NEW'd variable */
#define	MVST_STCK 7	/* save value of stackwarn or save an object of generic C struct */
#define MVST_NVAL 8	/* A temporary mval for indirect news */
#define MVST_TVAL 9	/* Saved value of $T, to be restored upon QUITing */
#define MVST_TPHOLD 10	/* Place holder for MUMPS stack pertaining to TSTART */
#define MVST_ZINTR  11  /* Environmental save for $zinterrupt */
#define MVST_ZINTDEV 12	/* In I/O when ZINTR, mstr input to now protected */
#define	MVST_STCK_SP 13	/* same as the MVST_STCK type except that it needs special handling in flush_jmp.c (see comment there) */
#define MVST_LVAL 14	/* Same as MVST_MVAL except we are pushing an lv_val instead of an mval */

/* Variation of ROUND_UP2 macro that doesn't have the checking that generates a GTMASSERT. This is necessary because the
   MV_SIZE macro is used in a static table initializer so cannot have executable (non-constant) code in it
*/
#define ROUND_UP2_NOCHECK(VALUE,MODULUS) (((VALUE) + ((MODULUS) - 1)) & ~((MODULUS) - 1))
#define MV_SIZE(X) \
        ROUND_UP2_NOCHECK(((sizeof(*mv_chain) - sizeof(mv_chain->mv_st_cont) + sizeof(mv_chain->mv_st_cont.X))), NATIVE_WSIZE)

LITREF unsigned char mvs_size[];

#define PUSH_MV_STENT(T) (((msp -= mvs_size[T]) <= stackwarn) ? \
	((msp <= stacktop) ? (msp += mvs_size[T]/* fix stack */, rts_error(VARLSTCNT(1) ERR_STACKOFLOW)) : \
	 rts_error(VARLSTCNT(1) ERR_STACKCRIT)) : \
	(((mv_stent *) msp)->mv_st_type = T , \
	((mv_stent *) msp)->mv_st_next = (int)((unsigned char *) mv_chain - msp)), \
	mv_chain = (mv_stent *) msp)

#define PUSH_MV_STCK(size,st_type) (((msp -= (mvs_size[st_type] + (size))) <= stackwarn) ? \
	((msp <= stacktop) ? (msp += (mvs_size[st_type] + (size))/* fix stack */, rts_error(VARLSTCNT(1) ERR_STACKOFLOW)) : \
	 rts_error(VARLSTCNT(1) ERR_STACKCRIT)) : \
	(((mv_stent *) msp)->mv_st_type = st_type, \
	((mv_stent *) msp)->mv_st_next = (int)((unsigned char *) mv_chain - msp)), \
	mv_chain = (mv_stent *) msp)

#ifdef DEBUG
#define POP_MV_STENT() (assert(msp == (unsigned char *) mv_chain), \
	msp += mvs_size[mv_chain->mv_st_type], \
	mv_chain = (mv_stent *)((char *) mv_chain + mv_chain->mv_st_next))
#else
#define POP_MV_STENT() (msp += mvs_size[mv_chain->mv_st_type], \
	mv_chain = (mv_stent *)((char *) mv_chain + mv_chain->mv_st_next))
#endif

#endif
