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

#ifndef __SBS_BLK_H__
#define __SBS_BLK_H__

/* The string, float and integer subscript arrays all occupy the same storage in an sbs_blk so they
   need to be the same size. Compute the dimensions that will fit into a given sized area. Original
   implementations had this as a 120 byte area but with the advent of unicode (increasing size of
   mstr) and 64 bit (increasing address size in all blocks containing them) a larger block is used.
   Note these sizes do not fit the power2 sizes storage mgmnt uses but since they aren't allocated
   singly but in groups, this is not an issue. To keep the numeric index at the same minimum for
   both 32 and 64 bit, we set the element size at 168 for 32 bit and 240 for 64 bit. Given these
   sizes, array dimensions will end up at the following:

   Block	32Bit	64Bit

   num_int	42	30
   num_flt	8	10
   num_str	8	10

   Given the current 6K allocation that gets chopped into sbs_blks in lv_get_sbs_blk(), these values
   give approx 33 sbs_blks in 32 bit mode and 22 blks in 64 bit mode (05/2007 se).
*/

#define SBS_ELE_BLK_SIZE	(GTM64_ONLY(240)NON_GTM64_ONLY(184))
#define SBS_NUM_INT_ELE		(SBS_ELE_BLK_SIZE / SIZEOF(lv_val *))
#define SBS_NUM_STR_ELE		(SBS_ELE_BLK_SIZE / SIZEOF(sbs_str_struct))
#define SBS_NUM_FLT_ELE		(SBS_ELE_BLK_SIZE / SIZEOF(sbs_flt_struct))

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
		/* these arrays should all be approx the same size (in bytes) */
	      	sbs_str_struct 	sbs_str[SBS_NUM_STR_ELE];
	  	sbs_flt_struct	sbs_flt[SBS_NUM_FLT_ELE];
	  	lv_val		*lv[SBS_NUM_INT_ELE];
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
