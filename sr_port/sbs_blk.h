/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __SBS_BLK_H__
#define __SBS_BLK_H__

#define SBS_NUM_INT_ELE	30
#define SBS_NUM_FLT_ELE	 9
#define SBS_NUM_STR_ELE	12
#define SBS_BLK_SIZE 128

typedef struct { mstr str ; lv_val *lv ;} sbs_str_struct;
typedef struct { mflt flt ; lv_val *lv ;} sbs_flt_struct;

/*
 * CAUTION ----	sbs_blk.sbs_que must have exactly the same position, form,
 *		and name as the corresponding field in symval.
 */

typedef struct sbs_blk_struct
{
	short	     		cnt;
	/* Note compiler inserts padding here of either 2 or 6 bytes depending on alignment
	   of the pointers below (32 or 64 bit).
	*/
	struct
	{
		struct sbs_blk_struct	*fl, *bl;
	} sbs_que;
	struct sbs_blk_struct	*nxt;
	union
	{
		/* these arrays should all be the same size (in bytes) */
	      	sbs_str_struct 	sbs_str[SBS_NUM_STR_ELE];	/* 12 * 10 */
	  	sbs_flt_struct	sbs_flt[SBS_NUM_FLT_ELE]; 	/*  9 * 13 */
	  	lv_val		*lv[SBS_NUM_INT_ELE];		/* 30 *  4 */
       	} ptr;
} sbs_blk;

typedef struct
{
	sbs_blk		*prev;
	sbs_blk		*blk;
	char		*ptr;
} sbs_search_status;

#define	SBS_BLK_TYPE_ROOT	1
#define	SBS_BLK_TYPE_INT	2
#define	SBS_BLK_TYPE_FLT	3
#define	SBS_BLK_TYPE_STR	4

typedef	struct lv_sbs_srch_hist_type
{
	unsigned char	type;
	union
	{
		lv_val		*root;
		lv_val		**intnum;
		sbs_flt_struct	*flt;
		sbs_str_struct	*str;
	} addr;
} lv_sbs_srch_hist;

mflt *lv_prv_num_inx(sbs_blk *root, mval *key);
mstr *lv_prv_str_inx(sbs_blk *root, mstr *key);
lv_val *lv_get_num_inx(sbs_blk *root, mval *key, sbs_search_status *status);
lv_val *lv_get_str_inx(sbs_blk *root, mstr *key, sbs_search_status *status);
lv_val *lv_ins_num_sbs(sbs_search_status *stat, mval *key, lv_sbs_tbl *tbl);
lv_val *lv_ins_str_sbs(sbs_search_status *stat, mval *key, lv_sbs_tbl *tbl);
lv_val *lv_nxt_num_inx(sbs_blk *root, mval *key, sbs_search_status *status);
lv_val *lv_nxt_str_inx(sbs_blk *root, mstr *key, sbs_search_status *status);

sbs_blk *lv_get_sbs_blk(symval *sym);
void lv_free_sbs_blk(sbs_blk *b);

#endif
