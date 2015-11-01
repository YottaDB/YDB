/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


/*
 * CAUTION ----	The structures lv_val, symval, and lv_sbs_tbl are all
 *		treated as if they were fields of a union.  The fields
 *		lv_val.v.mvtype, symval.ident, and lv_sbs_tbl.ident
 *		MUST be aligned.
 */

typedef struct lv_sbs_tbl_struct
{
       	unsigned char	       		ident;
       	bool   	       	       	       	int_flag;
       	unsigned char	       		level;
	unsigned char	     		filler;
	struct sbs_blk_struct		*str;
       	struct sbs_blk_struct		*num;
	struct lv_val_struct		*lv;
       	struct symval_struct   	       	*sym;
} lv_sbs_tbl;

typedef struct lv_val_struct
{
	mval v;
	struct lv_val_struct *tp_var;
	union
	{
		struct
 		{
       	       	       	lv_sbs_tbl		*children;
			union
			{	struct symval_struct   	*sym;
				lv_sbs_tbl		*sbs;
			}parent;
		} val_ent;
		struct
		{
		 	struct lv_val_struct *next_free;
		} free_ent;
	} ptrs;
} lv_val;

typedef struct lv_blk_struct
{
	lv_val *lv_base,*lv_free,*lv_top;
	struct lv_blk_struct *next;
} lv_blk;

/*
 * CAUTION ----	symval.sbs_que must have exactly the same position, form,
 *		and name as the corresponding field in sbs_blk.
 */

typedef struct symval_struct
{
       	unsigned char		ident;
	unsigned char		tp_save_all;
	struct
	{
		struct sbs_blk_struct	*fl, *bl;
	}			sbs_que;
       	htab_desc		h_symtab;
	lv_blk	       		first_block;
	lv_val	       		*lv_flist;
       	struct symval_struct	*last_tab;
} symval;

lv_val *lv_getslot(symval *sym);
void lv_cnv_int_tbl(lv_sbs_tbl *tbl);
void lv_killarray(lv_sbs_tbl *a);
void lv_newname(ht_entry *hte, symval *sym);
void lv_zap_sbs(lv_sbs_tbl *tbl, lv_val *lv);
lv_blk *lv_newblock(lv_blk *block_addr, lv_blk *next_block, int size);

void op_zshow(mval *func, int type, lv_val *lvn);
void op_fndata(lv_val *x, mval *y);
void op_fno2(lv_val *src,mval *key,mval *dst,mval *direct);
void op_fnnext(lv_val *src,mval *key,mval *dst);
void op_fnorder(lv_val *src, mval *key, mval *dst);
void op_fnzprevious(lv_val *src, mval *key, mval *dst);
void op_kill(lv_val *lv);
void op_lvzwithdraw(lv_val *lv);

void tp_var_clone(lv_val *var);

void lvzwr_var(lv_val *lv, int4 n);
unsigned char   *format_lvname(lv_val *start, unsigned char *buff, int size);
unsigned char *format_key_lv_val(lv_val *lvpin, unsigned char *buff, int size);

lv_val *op_srchindx();
lv_val *op_m_srchindx();
lv_val *op_putindx();
lv_val *op_getindx();

boolean_t lcl_arg1_is_desc_of_arg2(lv_val *cur, lv_val *ref);

