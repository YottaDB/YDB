/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ZWRITE_H_INCLUDED
#define ZWRITE_H_INCLUDED

#define ZWRITE_END	0
#define ZWRITE_ASTERISK 1
#define ZWRITE_UPPER	2
#define ZWRITE_ALL	3
#define ZWRITE_VAL	4
#define ZWRITE_BOTH	5
#define ZWRITE_LOWER	6
#define ZWRITE_PATTERN	7

#define ZWR_HTAB_INIT_SIZE  12		/* Initial # elems in ZWR addr hash table */
/* Number of entries in zwr_zav_blk */
#define ZWR_ZAV_BLK_CNT	    ((GTM64_ONLY(256)NON_GTM64_ONLY(128) - SIZEOF(storElem) - SIZEOF(zwr_zav_blk)) / SIZEOF(zwr_alias_var))

#include "hashtab_addr.h"
#include "lv_val.h"
#include "gcol_list.h"
#include "gtm_malloc.h"

enum zwr_init_types
{
	zwr_patrn_mident, 	/* Input parm is mident addr */
	zwr_patrn_mval		/* Input parm is mval addr (describing string pattern) */
};

/* Structures associated with tracking aliases during ZWRite */
typedef struct zwr_alias_var_struct
{
	boolean_t	value_printed;
	GTM64_ONLY(int4	filler;)
	mstr		zwr_var;	/* Base var name for this entry */
} zwr_alias_var;

typedef struct zwr_zav_blk_struct
{
	zwr_alias_var			*zav_base, *zav_free, *zav_top;
	struct zwr_zav_blk_struct	*next;
	/* note this block (when allocated) will also contain the zwr_alias_var array */
} zwr_zav_blk;

typedef struct zwr_hash_table_struct
{
	boolean_t			cleaned;
	GTM64_ONLY(int4			filler;)
	hash_table_addr			h_zwrtab;
	zwr_zav_blk			*first_zwrzavb;
	zwr_alias_var			*zav_flist;
} zwr_hash_table;

/* Structures used to keep track of the subscript(s) for the current var/value being processed */
typedef struct zwr_sub_lst_struct
{
	struct
	{
		unsigned char		subsc_type;
		mval			*actual;
		mval			*first, *second;
	} subsc_list[1];
} zwr_sub_lst;

typedef struct lvzwrite_datablk_struct
{
	enum zwr_init_types		zwr_intype;
	boolean_t			fixed;
	boolean_t			zav_added;	/* This call resulted in a zwr_alias_var added to hash tab */
	unsigned short			subsc_count;
	unsigned short			curr_subsc;
	uint4				mask;
	uint4				mv_sub_top;	/* Top of >sub array where .actual is auto pointer */
	mval				*pat;
	mstr				*curr_name;
	zwr_sub_lst			*sub;
	struct lvzwrite_datablk_struct	*prev;
} lvzwrite_datablk;

/* PUSH active MERGE or ZSHOW/ZWRITE context */
#define PUSH_MVST_MRGZWRSV_IF_NEEDED											\
{															\
	GBLREF	int			merge_args;									\
	GBLREF	lvzwrite_datablk	*lvzwrite_block;								\
	GBLREF	uint4			zwrtacindx;									\
	GBLREF	merge_glvn_ptr		mglvnp;										\
	GBLREF	gvzwrite_datablk	*gvzwrite_block;								\
	GBLREF	lvzwrite_datablk	*lvzwrite_block;								\
	GBLREF	zshow_out		*zwr_output;									\
	GBLREF zwr_hash_table		*zwrhtab;									\
	DEBUG_ONLY(GBLREF unsigned int	count_prohibit_longjmp;)							\
	DCL_THREADGBL_ACCESS;												\
															\
	SETUP_THREADGBL_ACCESS;												\
	if (TREF(in_zwrite) || (0 != merge_args))									\
	{														\
		PUSH_MV_STENT(MVST_MRGZWRSV);										\
		mv_st_ent = mv_chain;											\
		mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_merge_args = merge_args;					\
		merge_args = 0;												\
		mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_zwrtacindx = zwrtacindx;					\
		zwrtacindx = 0;												\
		mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_in_zwrite = TREF(in_zwrite);					\
		TREF(in_zwrite) = 0;											\
		mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_mglvnp = mglvnp;						\
		mglvnp = NULL;												\
		mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_lvzwrite_block = lvzwrite_block;				\
		lvzwrite_block = NULL;											\
		mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_gvzwrite_block = gvzwrite_block;				\
		gvzwrite_block = NULL;											\
		mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_zwr_output = zwr_output;					\
		zwr_output = NULL;											\
		mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_zwrhtab = zwrhtab;						\
		zwrhtab = NULL;												\
		DEBUG_ONLY(mv_st_ent->mv_st_cont.mvs_mrgzwrsv.save_count_prohibit_longjmp = count_prohibit_longjmp;)	\
		DEBUG_ONLY(count_prohibit_longjmp = 0;)									\
	}														\
}

/* Note: Corresponding POP of MERGE or ZSHOW/ZWRITE context is done in unw_mv_ent.c (case MVST_MRGZWRSV) */

/* Nullify active MERGE or ZSHOW/ZWRITE context */
#define	NULLIFY_MERGE_ZWRITE_CONTEXT									\
MBSTART {												\
	GBLREF	int			merge_args;							\
	GBLREF	lvzwrite_datablk	*lvzwrite_block;						\
	DCL_THREADGBL_ACCESS;										\
													\
	SETUP_THREADGBL_ACCESS;										\
	merge_args = 0;											\
	TREF(in_zwrite) = 0;										\
	if (lvzwrite_block)										\
	{												\
		glist_unprotect_lvzwrite_block(lvzwrite_block, 0, lvzwrite_block->mv_sub_top);		\
		lvzwrite_block->mv_sub_top = lvzwrite_block->curr_subsc = lvzwrite_block->subsc_count = 0;\
	}												\
} MBEND

static inline boolean_t glist_lvzwrite_block_unprotected(lvzwrite_datablk *lvz_p, unsigned int start, unsigned int end)
{
	assert(end <= MAX_LVSUBSCRIPTS);
	for (; start < end; start++)
	{
		if (lvz_p->sub && lvz_p->sub->subsc_list[start].actual)
		{
			if (glist_str_protected(&lvz_p->sub->subsc_list[start].actual->str))
				return FALSE;
		}
	}
	return TRUE;
}

static inline void glist_unprotect_lvzwrite_block(lvzwrite_datablk *lvz_p, unsigned int start, unsigned int end)
{
	DEBUG_ONLY(GBLREF unsigned int count_prohibit_longjmp;)
	assert(end <= MAX_LVSUBSCRIPTS);
	for (; start < end; start++)
	{
		if (lvz_p->sub && lvz_p->sub->subsc_list[start].actual)
		{
			if (lvz_p->sub->subsc_list[start].actual != lvz_p->sub->subsc_list[start].first)
			{
				assert(glist_str_protected(&lvz_p->sub->subsc_list[start].actual->str));
				glist_unprotect_str(&lvz_p->sub->subsc_list[start].actual->str);
			}
			lvz_p->sub->subsc_list[start].actual = NULL;
		}
	}
	DEBUG_ONLY(count_prohibit_longjmp = 0;)
}


typedef struct gvzwrite_datablk_struct
{
	boolean_t			type;
	unsigned short			subsc_count;
	unsigned short			curr_subsc;
	boolean_t			fixed;
	uint4				mask;
	mval				*pat;
	gd_addr				*ref_gbldir;	/* ZWRITE doesn't support explicit external references, but best be ready */
	gv_key				*ref_key;
	gd_region			*ref_reg;
	gv_namehead			*ref_targ;	/* in dbg, used to check that reset_gv_target tracks correctly */
	zwr_sub_lst			*sub;
	boolean_t			gv_last_subsc_null;
	boolean_t			gv_some_subsc_null;
} gvzwrite_datablk;

void gvzwr_arg(int t, mval *a1, mval *a2);
void gvzwr_init(unsigned short t, mval *val, int4 pat);
void gvzwr_out(void);
void gvzwr_var(uint4 data, int4 n);
void lvzwr_arg(int t, mval *a1, mval *a2);
void lvzwr_init(enum zwr_init_types t, mval *val);
void lvzwr_out(lv_val *lvp);
unsigned char *lvzwr_key(unsigned char *buff, int size);

#endif
