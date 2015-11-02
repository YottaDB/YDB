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

#ifndef MV_STENT_H
#define MV_STENT_H

#ifndef ERROR_TRAP_H
# include "error_trap.h"
#endif
#include "io.h"
#include "lv_val.h"
#include "error.h"

typedef struct
{
	ht_ent_mname		*hte_addr;	/* Hash table entry for name (updated automagically when ht expands) */
	struct lv_val_struct	*save_value;	/* value to restore to hashtab and lst_addr (if supplied) on pop */
	DEBUG_ONLY(var_tabent	*nam_addr;)	/* Name associated with hash table entry for the saved value (for asserts) */
} mvs_ntab_struct;

/* PVAL includes NTAB plus a pointer to the new value created which may or may not be in the hashtable anymore
 * by the time this is unstacked but it needs to be cleaned up when the frame pops.
 */
typedef struct
{
	lv_val		*mvs_val;	/* lv_val created to hold new value */
	mvs_ntab_struct	mvs_ptab;	/* Restoration-on-pop info */
} mvs_pval_struct;

/* NVAL is similar to PVAL except when dealing with indirect frames, the name of the var in the indirect frame's l_symtab
 * can disappear before we are done with our potential need for it so the extra "name" field in this block allows us to
 * cache the reference to it on the stack so it doesn't disappear.
 *
 * Note routine lvval_gcol() in alias_funcs.c has a dependency that mvs_ptab in PVAL and NVAL are at same offset within
 * the structure.
 *
 * Note the extra "name" field is only carried in DEBUG mode. In non-Debug builds, the NVAL is the same as the PVAL.
 */
typedef struct
{
	lv_val		*mvs_val;	/* lv_val created to hold new value */
	mvs_ntab_struct	mvs_ptab;	/* Restoration-on-pop info */
	DEBUG_ONLY(var_tabent	name;)	/* name.var_name.addr points to whatever the original vartab entry points to */
} mvs_nval_struct;

typedef struct
{
	struct io_desc_struct	*io_ptr;	/* associated device structure */
	boolean_t		buffer_valid;	/* if TRUE, need to update curr_sp_buffer during stp_gcol */
	mstr			curr_sp_buffer;	/* buffer space in stringpool */
	void			*socketptr;	/* void to avoid headers */
} mvs_zintdev_struct;

typedef struct
{	/* Note the top of this structure is a partial version of mvs_trigr_struct so needs to map into the top
	 * of that structure (below) since cleanups share the same code
	 */
	boolean_t		saved_dollar_truth;
	GTM64_ONLY(int4		filler;)		/* Alignment */
	mval			savtarg;		/* Current gv_currkey */
	mstr			savextref;		/* Current extended reference name component (if any) */
	/* End of common area with mvs_trigr_struct */
	stack_frame		*error_frame_save;	/* Save/restore error_frame over $ZINTERRUPT processor */
	dollar_ecode_type	dollar_ecode_save;	/* Save $ECODE array results */
	dollar_stack_type	dollar_stack_save;	/* Save $STACK(...) array results */
} mvs_zintr_struct;

typedef struct
{	/* Saves "global" entries for a trigger invocation base frame. Note the top entries of this
	 * structure are the same as mvs_zintr_struct which should be kept in sync with this one as the
	 * cleanups in unw_mv_ent() share code.
	 */
	boolean_t		saved_dollar_truth;
	GTM64_ONLY(int4		filler;)		/* Alignment */
	mval			savtarg;		/* Current gv_currkey */
	mstr			savextref;		/* Current extended reference name component (if any) */
	/* End of requirement to match mvs_zintr_struct */
#	ifdef GTM_TRIGGER
        /* This ifdef cuts out the meat of a structure never used on VMS but allows the structure to exist so table
	 * references (e.g. in mtables) continue to be valid.
	 */
	boolean_t		*ztvalue_changed_ptr;	/* pointer to ztvalue_changed for previous trigger level */
	mval			*ztvalue_save;		/* Save it once per trigger level */
	mstr			*ztname_save;
	mval			*ztdata_save;
	mval			*ztoldval_save;
	mval			*ztriggerop_save;
	mval			*ztupdate_save;
	condition_handler	*ctxt_save;		/* Where condition handler stack should end up when unwound */
	int4			gtm_trigger_depth_save;	/* Where our depth guage should end up when unwound */
	mval			dollar_etrap_save;	/* If $gtm_trigger_etrap is specified, save dollar_etrap here */
	mval			dollar_ztrap_save;	/* .. likewise if saving $etrap, save $ztrap too */
	boolean_t		ztrap_explicit_null_save; /* Just save it rather than figuring out what to restore */
	int4			mumps_status_save;	/* Each invocation has its own return code */
	boolean_t		run_time_save;		/* MUPIP & GTCM need this flag restored appropriately */
#	ifdef DEBUG
	void			*gtm_trigdsc_last_save;	/* types are generic so all includers don't need gv_trigger.h */
	void			*gtm_trigprm_last_save;
#	endif
#	endif
} mvs_trigr_struct;

typedef struct
{	/* When MERGE and/or ZWRITE nests (due to trigger or $ZINTRPT firing), the globals used need to be saved to
	 * prevent collisions and restored when this frame pops.
	 */
	int					save_merge_args;
	uint4					save_zwrtacindx;
	boolean_t				save_in_zwrite;
	GTM64_ONLY(int4				filler;)
	struct merge_glvn_struct_type		*save_mglvnp;
	struct gvzwrite_datablk_struct		*save_gvzwrite_block;
	struct lvzwrite_datablk_struct		*save_lvzwrite_block;
	struct zshow_out_struct			*save_zwr_output;
	struct zwr_hash_table_struct		*save_zwrhtab;
} mvs_mrgzwrsv_struct;

typedef struct
{
	uint4			tphold_tlevel;		/* $TLEVEL prior to level we are entering */
#	ifdef GTM_TRIGGER
	mval			ztwormhole_save;	/* Saved $ZTWormhole value to be restored on restart if len != -1 */
#	endif
} mvs_tphold_struct;

typedef struct
{
	unsigned char		*restart_pc_save;
	unsigned char		*restart_ctxt_save;
} mvs_rstrtpc_struct;

/* zintcmd* entries are for timed commands other than I/O which can be
 * interrupted.  Without an identifying structure such as the device
 * structure, we use the restart_pc/ctxt values which must be save
 * immediately before the commands opcode via a call to op_restartpc
 * in ttt.txt (e.g. HANG) or generated via the m_ routine (e.g. LOCK.)
 */
typedef enum zintcmd_ops_enum
{
	ZINTCMD_NOOP = 0,
	ZINTCMD_HANG,
	ZINTCMD_LOCK,		/* also used for ZALLOCATE */
	ZINTCMD_LAST
} zintcmd_ops;

typedef struct
{
	zintcmd_ops	command;
	ABS_TIME	end_or_remain;	/* HANG = end_time, LOCK = remaining */
	unsigned char	*restart_pc_check;	/* of interrupted command */
	unsigned char	*restart_ctxt_check;
	unsigned char	*restart_pc_prior;	/* from zintcmd_active before */
	unsigned char	*restart_ctxt_prior;	/* this entry was put on stack */
} mvs_zintcmd_struct;

/* Homogenous mv_stent structure containing all types. This structure is never allocated as is but is allocated
 * on the M stack using the size for the size defined in mvs_size[] array in mtables.c
 * Note that since mvs_size is unsigned char, the sizeof each struct must be under 256 bytes
 */
typedef struct mv_stent_struct
{
	unsigned int 			mv_st_type : 6;		/* Max type is 63 */
	unsigned int 			mv_st_next : 26;	/* Accomodates distances up to 64MB (stack currently 256KB) */
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
		struct
		{
			void		**mvs_stck_addr;
			void		*mvs_stck_val;
			int4		mvs_stck_size;
		} mvs_stck;
		mvs_ntab_struct		mvs_ntab;
		mvs_zintdev_struct	mvs_zintdev;
		mvs_pval_struct		mvs_pval;
		mvs_nval_struct		mvs_nval;
		mvs_zintr_struct	mvs_zintr;
		mvs_trigr_struct	mvs_trigr;
	  	mvs_tphold_struct	mvs_tp_holder;
		mvs_rstrtpc_struct	mvs_rstrtpc;
		mvs_mrgzwrsv_struct	mvs_mrgzwrsv;
		mvs_zintcmd_struct	mvs_zintcmd;
		int4			mvs_tval;
		int4			mvs_storig;
	} mv_st_cont;
} mv_stent;

void unw_mv_ent(mv_stent *mv_st_ent);
void push_stck(void* val, int val_size, void** addr, int mvst_stck_type);

#define MVST_MSAV	0	/* An mval and an address to store it at pop time, most
				 * often used to save/restore new'd intrinsic variables.
				 * This is important because the restore addr is fixed,
				 * and no extra work is required to resolve it.
				 */
#define MVST_MVAL	1	/* An mval which will be dropped at pop time */
#define MVST_STAB	2	/* A symbol table */
#define MVST_IARR	3	/* An array of (literal or temp) mval's and mstr's on the stack, due to indirection */
#define	MVST_NTAB	4	/* A place to save old name hash table values during parameter passed functions (used for dotted
				 * parm/alias)  */
#define MVST_ZINTCMD	5	/* Non IO timed commands when ZINTR */
#define	MVST_PVAL	6	/* A temporary mval for formal parameters or NEW'd variable */
#define	MVST_STCK	7	/* save value of stackwarn or save an object of generic C struct */
#define MVST_NVAL	8	/* A temporary mval for indirect news */
#define MVST_TVAL	9	/* Saved value of $T, to be restored upon QUITing */
#define MVST_TPHOLD	10	/* Place holder for MUMPS stack pertaining to TSTART */
#define MVST_ZINTR	11	/* Environmental save for $zinterrupt */
#define MVST_ZINTDEV	12	/* In I/O when ZINTR, mstr input to now protected */
#define	MVST_STCK_SP	13	/* same as the MVST_STCK type except that it needs special handling in flush_jmp.c
				 * (see comment in mtables.c where mvs_save is defined).
				 */
#define MVST_LVAL	14	/* Same as MVST_MVAL except we are pushing an lv_val instead of an mval */
#define MVST_TRIGR	15	/* Used to save the base environment for Trigger execution */
#define MVST_RSTRTPC	16	/* Used to save/restore the restartpc/context across error or jobinterrupt frames */
#define MVST_STORIG	17	/* This is the origin mv_stent placed on the stack during initialization */
#define MVST_MRGZWRSV	18	/* Block used to save merge/zwrite control blocks when one or more of them nest */
#define	MVST_LAST	18	/* update this, mvs_size and mvs_save in mtables.c, and switches in unw_mv_ent.c,
				 * stp_gcol_src.h, and get_ret_targ.c when adding a new MVST type */

/* Variation of ROUND_UP2 macro that doesn't have the checking that generates a GTMASSERT. This is necessary because the
 * MV_SIZE macro is used in a static table initializer so cannot have executable (non-constant) code in it
 */
#define ROUND_UP2_NOCHECK(VALUE,MODULUS) (((VALUE) + ((MODULUS) - 1)) & ~((MODULUS) - 1))
#define MV_SIZE(X) \
        ROUND_UP2_NOCHECK(((SIZEOF(*mv_chain) - SIZEOF(mv_chain->mv_st_cont) + SIZEOF(mv_chain->mv_st_cont.X))), NATIVE_WSIZE)

#define PUSH_MV_STENT(T) (((msp -= mvs_size[T]) <= stackwarn) ?							\
	((msp <= stacktop) ? (msp += mvs_size[T]/* fix stack */, rts_error(VARLSTCNT(1) ERR_STACKOFLOW)) :	\
	 rts_error(VARLSTCNT(1) ERR_STACKCRIT)) :								\
	(((mv_stent *)msp)->mv_st_type = T ,									\
	((mv_stent *)msp)->mv_st_next = (int)((unsigned char *) mv_chain - msp)),				\
	mv_chain = (mv_stent *)msp)

#define PUSH_MV_STCK(size, st_type) (((msp -= ROUND_UP(mvs_size[st_type] + (size), SIZEOF(char *))) <= stackwarn) ?	\
	((msp <= stacktop) ? (msp += ROUND_UP(mvs_size[st_type] + (size), SIZEOF(char *)) /* fix stack */,		\
        rts_error(VARLSTCNT(1) ERR_STACKOFLOW)) :									\
	rts_error(VARLSTCNT(1) ERR_STACKCRIT)) :									\
	(((mv_stent *)msp)->mv_st_type = st_type,									\
	((mv_stent *)msp)->mv_st_next = (int)((unsigned char *) mv_chain - msp)),					\
	mv_chain = (mv_stent *)msp)

#ifdef DEBUG
#define POP_MV_STENT() (assert(msp == (unsigned char *) mv_chain),		\
	msp += mvs_size[mv_chain->mv_st_type],					\
	mv_chain = (mv_stent *)((char *) mv_chain + mv_chain->mv_st_next))
#else
#define POP_MV_STENT() (msp += mvs_size[mv_chain->mv_st_type],			\
	mv_chain = (mv_stent *)((char *) mv_chain + mv_chain->mv_st_next))
#endif

#define	IS_PTR_INSIDE_M_STACK(PTR)	(((unsigned char *)PTR < (sm_uc_ptr_t)stackbase) && ((unsigned char *)PTR > stacktop))

#define PUSH_MVST_MRGZWRSV								\
{											\
	PUSH_MV_STENT(MVST_MRGZWRSV);							\
	mv_st_ent = mv_chain;								\
	mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_merge_args = merge_args;		\
	merge_args = 0;									\
	mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_zwrtacindx = zwrtacindx;		\
	zwrtacindx = 0;									\
	mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_in_zwrite = TREF(in_zwrite);		\
	TREF(in_zwrite) = 0;								\
	mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_mglvnp = mglvnp;			\
	mglvnp = NULL;									\
	mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_lvzwrite_block = lvzwrite_block;	\
	lvzwrite_block = NULL;								\
	mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_gvzwrite_block = gvzwrite_block;	\
	gvzwrite_block = NULL;								\
	mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_zwr_output = zwr_output;		\
	zwr_output = NULL;								\
	mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_zwrhtab = zwrhtab;			\
	zwrhtab = NULL;									\
}

/* Declare those global variables and error messages that are used by the PUSH_MV_STENT and POP_MV_STENT macros */
LITREF	unsigned char	mvs_size[];
GBLREF	unsigned char	*stackbase, *stacktop, *stackwarn, *msp;
GBLREF	mv_stent	*mv_chain;

error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

#endif
