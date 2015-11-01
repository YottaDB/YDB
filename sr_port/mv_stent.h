/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

typedef struct
{
	var_tabent		*nam_addr;
	struct lv_val_struct	**lst_addr;
	struct lv_val_struct	*save_value;
} mvs_ntab_struct;

typedef struct
{
	struct lv_val_struct	*mvs_val;
	mvs_ntab_struct mvs_ptab;
} mvs_pval_struct;

typedef struct
{
	struct lv_val_struct	*mvs_val;
	mvs_ntab_struct 	mvs_ptab;
	var_tabent		name;		/* name.var_name.addr points to whaever the original vartab entry points to */
} mvs_nval_struct;

typedef struct
{
	uint4	mask;
	unsigned short	actualcnt;
	struct lv_val_struct	*actuallist[1];
} parm_blk;

typedef struct
{
	bool		save_truth;
	mval		*ret_value;
	parm_blk	*mvs_parmlist;
} mvs_parm_struct;

typedef struct
{
	int		saved_dollar_truth;
	mval		savtarg;
} mvs_zintr_struct;

typedef struct mv_stent_struct
{
	unsigned int mv_st_type : 4;
	unsigned int mv_st_next : 28;
	union
	{
		mval mvs_mval;
		struct
		{
			mval v;
			mval *addr;
		} mvs_msav;
		struct symval_struct *mvs_stab;
		struct
		{
			unsigned short iarr_mvals;
			unsigned char  *iarr_base;
		} mvs_iarr;
		mvs_ntab_struct mvs_ntab;
		mvs_parm_struct mvs_parm;
		mvs_zintr_struct mvs_zintr;
		mvs_pval_struct mvs_pval;
		mvs_nval_struct mvs_nval;
		struct {
			void **mvs_stck_addr;
			void *mvs_stck_val;
			int4 mvs_stck_size;
		} mvs_stck;
		int4 mvs_tval;
		int4 mvs_tp_holder;
	} mv_st_cont;
} mv_stent;

mval *unw_mv_ent(mv_stent *mv_st_ent);
void push_stck(void* val, int val_size, void** addr);

#define MVST_MSAV 0	/* An mval and an address to store it at pop time, most
			   often used to save/restore new'd intrinsic variables.
			   This is important because the restore addr is fixed,
			   and no extra work is required to resolve it. */
#define MVST_MVAL 1	/* An mval which will be dropped at pop time */
#define MVST_STAB 2	/* A symbol table */
#define MVST_IARR 3	/* An array of (literal or temp) mval's and mstr's on the stack, due to indirection */
#define	MVST_NTAB 4	/* A place to save old name hash table values during parameter passed functions */
#define	MVST_PARM 5	/* A pointer to a parameter passing block */
#define	MVST_PVAL 6	/* A temporary mval for formal parameters */
#define	MVST_STCK 7	/* save value of stackwarn or save an object of generic C struct */
#define MVST_NVAL 8	/* A temporary mval for indirect news */
#define MVST_TVAL 9	/* Saved value of $T, to be restored upon QUITing */
#define MVST_TPHOLD 10	/* Place holder for MUMPS stack pertaining to TSTART */
#define MVST_ZINTR  11  /* Environmental save for $zinterrupt */

#define MV_SIZE(X) (sizeof(*mv_chain) - sizeof(mv_chain->mv_st_cont) + sizeof(mv_chain->mv_st_cont.X))

LITREF unsigned char mvs_size[];

#define PUSH_MV_STENT(T) (((msp -= mvs_size[T]) <= stackwarn) ? \
	((msp <= stacktop) ? (msp += mvs_size[T]/* fix stack */, rts_error(VARLSTCNT(1) ERR_STACKOFLOW)) : \
	 rts_error(VARLSTCNT(1) ERR_STACKCRIT)) : \
	(((mv_stent *) msp)->mv_st_type = T , \
	((mv_stent *) msp)->mv_st_next = (unsigned char *) mv_chain - msp), \
	mv_chain = (mv_stent *) msp)

#define PUSH_MV_STCK(size) (((msp -= (mvs_size[MVST_STCK] + (size))) <= stackwarn) ? \
	((msp <= stacktop) ? (msp += (mvs_size[MVST_STCK] + (size))/* fix stack */, rts_error(VARLSTCNT(1) ERR_STACKOFLOW)) : \
	 rts_error(VARLSTCNT(1) ERR_STACKCRIT)) : \
	(((mv_stent *) msp)->mv_st_type = MVST_STCK , \
	((mv_stent *) msp)->mv_st_next = (unsigned char *) mv_chain - msp), \
	mv_chain = (mv_stent *) msp)

#ifdef DEBUG
#define POP_MV_STENT() (assert(msp == (unsigned char *) mv_chain), \
	msp += mvs_size[mv_chain->mv_st_type], \
	mv_chain = (mv_stent *)((char *) mv_chain + mv_chain->mv_st_next))
#else
#define POP_MV_STENT() (msp += mvs_size[mv_chain->mv_st_type], \
	mv_chain = (mv_stent *)((char *) mv_chain + mv_chain->mv_st_next))
#endif
