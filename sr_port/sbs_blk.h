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

#ifndef __SBS_BLK_H__
#define __SBS_BLK_H__

/* The string, float and integer subscript arrays all occupy the same storage in an sbs_blk so they
   need to be the same size. We want these areas to be able to be scaled up for processes with
   larger array requirements which will improve the local variable access times. Hence the basic
   sbs_blk only has one entry in each array in the union block. The actual dimensions of the array
   are computed in gtm_env_init() where we read in the scaling value (or default it).
*/

#define MIN_INT_ELE_CNT		GTM64_ONLY(30) NON_GTM64_ONLY(45)
#define SBS_NUM_INT_ELE		lv_sbs_int_ele_cnt
#define SBS_NUM_FLT_ELE		lv_sbs_flt_ele_cnt
#define SBS_NUM_STR_ELE		lv_sbs_str_ele_cnt

GBLREF int4	lv_sbs_int_ele_cnt;
GBLREF int4	lv_sbs_flt_ele_cnt;
GBLREF int4	lv_sbs_str_ele_cnt;

typedef struct {mstr str; lv_val *lv;} sbs_str_struct;
typedef struct {mflt flt; lv_val *lv;} sbs_flt_struct;

/*
 * CAUTION ----	sbs_blk.sbs_que must have exactly the same position, form,
 *		and name as the corresponding field in symval.
 */

typedef struct sbs_blk_struct
{
	int4	     		cnt;
	uint4			filler;
	struct
	{
		struct sbs_blk_struct	*fl, *bl;
	} sbs_que;
	struct sbs_blk_struct	*nxt;
	union
	{
		/* these arrays should be the last thing in the block as they are of variable dimension */
	      	sbs_str_struct 	sbs_str[1];
	  	sbs_flt_struct	sbs_flt[1];
	  	lv_val		*lv[1];
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
