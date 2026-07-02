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

#include "mdef.h"

#include <stddef.h>
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmio.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "fnpc.h"
#ifdef UTF8_SUPPORTED
# include "utfcgr.h"
#endif
#include "gdscc.h"
#include "cache.h"
#include "comline.h"
#include "compiler.h"
#include "io.h"
#include "jnl.h"
#include "lv_val.h"
#include "subscript.h"
#include "mdq.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "stack_frame.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "mlkdef.h"
#include "zbreak.h"
#include "zshow.h"
#include "zwrite.h"
#include "error.h"
#include "stpg_sort.h"
#include "hashtab_objcode.h"
#include "hashtab_str.h"
#include "min_max.h"
#include "alias.h"
#include "gtmimagename.h"
#include "srcline.h"
#include "opcode.h"
#include "glvn_pool.h"
#include "iormdef.h"
#include "localvarmonitor.h"
#include "have_crit.h"
#include "reachables.h"

#ifdef DEBUG
#define DOUBLECHECK_GCOL
#define DOUBLECHECK_GCOL_ASSERT_INTL(X) assert(X)
#endif
#ifndef STP_MOVE
GBLDEF int	indr_stp_low_reclaim_passes = 0;
GBLDEF int	rts_stp_low_reclaim_passes = 0;
GBLDEF int	indr_stp_incr_factor = 1;
GBLDEF int	rts_stp_incr_factor = 1;
#else
GBLREF int	indr_stp_low_reclaim_passes;
GBLREF int	rts_stp_low_reclaim_passes;
GBLREF int	indr_stp_incr_factor;
GBLREF int	rts_stp_incr_factor;
#endif

GBLREF mvar 			*mvartab;
GBLREF mlabel 			*mlabtab;
GBLREF int 			mvmax;
GBLREF int			mlmax;
GBLREF int 			mvar_index;
GBLREF hash_table_objcode 	cache_table;
GBLREF unsigned char		*msp, *stackbase, *stacktop, *stackwarn;
GBLREF uint4			stp_array_size;
GBLREF io_log_name		*io_root_log_name;
GBLREF lvzwrite_datablk		*lvzwrite_block;
GBLREF mliteral			literal_chain;
GBLREF mstr			*comline_base,
       				**stp_array;
GBLREF mval			dollar_system, dollar_zerror, dollar_zgbldir, dollar_zstatus;
GBLREF mval			dollar_zyerror, dollar_zinterrupt, dollar_zsource, dollar_ztexit;
GBLREF mv_stent			*mv_chain;
GBLREF sgm_info			*first_sgm_info;
GBLREF spdesc			indr_stringpool, rts_stringpool, stringpool;
GBLREF stack_frame		*frame_pointer;
GBLREF symval			*curr_symval;
GBLREF tp_frame			*tp_pointer;
GBLREF boolean_t		stop_non_mandatory_expansion, expansion_failed, retry_if_expansion_fails;
GBLREF zwr_hash_table		*zwrhtab;				/* How we track aliases during zwrites */
GBLREF int4			SPGC_since_LVGC;			/* stringpool GCs since the last dead-data GC */
GBLREF int4			LVGC_interval;				/* dead data GC done every LVGC_interval stringpool GCs */
GBLREF boolean_t		suspend_lvgcol;
GBLREF hash_table_str		*complits_hashtab;
GBLREF mval			*alias_retarg;
GBLREF io_pair			*io_std_device;
GTMTRIG_ONLY(GBLREF mval 	dollar_ztwormhole;)
GTMTRIG_ONLY(GBLREF mval 	dollar_ztslate;)
DEBUG_ONLY(GBLREF   boolean_t	ok_to_UNWIND_in_exit_handling;)
GBLREF intrpt_state_t		intrpt_ok_state;


#ifdef DOUBLECHECK_GCOL
#define DOUBLECHECK_GCOL_ONLY(X)	X
#define DOUBLECHECK_GCOL_ASSERT(X) DOUBLECHECK_GCOL_ASSERT_INTL(X)
#else
#define DOUBLECHECK_GCOL_ONLY(X)
#define DOUBLECHECK_GCOL_ASSERT(X)
#endif

OS_PAGE_SIZE_DECLARE

static mstr			**topstr, **array, **arraytop;
static boolean_t			ctime_gcol;
#ifdef DOUBLECHECK_GCOL
static unsigned long			old_ops;
#endif

error_def(ERR_STPEXPFAIL);
error_def(ERR_STPCRIT);
error_def(ERR_STPOFLOW);

/* See comment inside LV_NODE_KEY_STPG_ADD macro for why the ASSERT_LV_NODE_MSTR_EQUIVALENCE macro does what it does */

#define	MVAL_STPG_ADD(MVAL1)												\
MBSTART {														\
	mval		*lcl_mval;											\
															\
	lcl_mval = MVAL1;												\
	if (MV_IS_STRING(lcl_mval))											\
		MSTR_STPG_ADD(&lcl_mval->str);										\
	else														\
	{														\
		DOUBLECHECK_GCOL_ONLY(old_ops++;)									\
	}														\
} MBEND

#define MSTR_STPG_PUT(MSTR1)									\
MBSTART {											\
	ptrdiff_t	diff;									\
												\
	assert(glist_str_protected(MSTR1));							\
	TREE_DEBUG_ONLY(									\
	/* assert that we never add a duplicate mstr as otherwise we will			\
	 * face problems later in PROCESS_CONTIGUOUS_BLOCK macro.				\
	 * this code is currently commented out as it slows down stp_gcol			\
	 * tremendously (O(n^2) algorithm where n is # of mstrs added)				\
	 */											\
		mstr	**curstr;								\
		for (curstr = array; curstr < topstr; curstr++)					\
			assert(*curstr != (MSTR1));						\
	)											\
	assert(topstr < arraytop);								\
	assert(0 < (MSTR1)->len);								\
	/* It would be nice to test for maxlen as well here but that causes			\
	 * some usages of stringpool to fail as other types of stuff are			\
	 * built into the stringppool besides strings.						\
	 */											\
	*topstr++ = (MSTR1);									\
	DOUBLECHECK_GCOL_ONLY(old_ops++;)							\
	if (topstr >= arraytop)									\
	{											\
		diff = topstr - array;								\
		stp_expand_array(&stp_array_size, &stp_array, sizeof(*stp_array), 0);		\
		array = stp_array;								\
		topstr = array + diff;								\
		arraytop = array + stp_array_size;						\
		assert(topstr < arraytop);							\
	}											\
} MBEND

#ifdef STP_MOVE
#define STP_MOVE_ONLY(X)	X

#define	MSTR_STPG_ADD(MSTR1)										\
MBSTART {												\
	mstr		*lcl_mstr;									\
	char		*lcl_addr;									\
													\
	GBLREF	spdesc	stringpool;									\
													\
	lcl_mstr = MSTR1;										\
	if (lcl_mstr->len)										\
	{												\
		lcl_addr = lcl_mstr->addr;								\
		if (IS_PTR_IN_RANGE(lcl_addr, stringpool.base, stringpool.free))	/* BYPASSOK */	\
		{											\
			MSTR_STPG_PUT(lcl_mstr);							\
		} else if (IS_PTR_IN_RANGE(lcl_addr, stp_move_from, stp_move_to))			\
		{											\
			MSTR_STPG_PUT(lcl_mstr);							\
			stp_move_count_old++;								\
		} else											\
			DOUBLECHECK_GCOL_ONLY(old_ops++);						\
	} else												\
		DOUBLECHECK_GCOL_ONLY(old_ops++);							\
} MBEND

#else
#define STP_MOVE_ONLY(X)

#define	MSTR_STPG_ADD(MSTR1)										\
MBSTART {												\
	mstr		*lcl_mstr;									\
	char		*lcl_addr;									\
													\
	GBLREF spdesc	stringpool;									\
													\
	lcl_mstr = MSTR1;										\
	if (lcl_mstr->len)										\
	{												\
		lcl_addr = lcl_mstr->addr;								\
		if (IS_PTR_IN_RANGE(lcl_addr, stringpool.base, stringpool.free))	/* BYPASSOK */	\
			MSTR_STPG_PUT(lcl_mstr);							\
		else											\
			DOUBLECHECK_GCOL_ONLY(old_ops++);						\
	} else												\
		DOUBLECHECK_GCOL_ONLY(old_ops++);							\
} MBEND

#endif

/* Takes a sorted range of mstr_sort_array_elements and an addr, looks for the first mstr_sort_array_element whose ->addr is greater
 * than or equal to the given addr.
 */
static inline size_t glist_bsearch_sort_array(const mstr_sort_array *array_p, size_t l, size_t r, const char *addr)
{
	size_t base, m, ele, sz;

	base = l;
	sz = array_p->size;
	ele = array_p->start_ele;
	while (l < r)
	{
		m = l + ((r - l) >> 1);
		if (array_p->array[REAL_SORT_INDEX_ELE_SZ(m, ele, sz)].addr < addr)
			l = m + 1;
		else
			r = m;
	}
	return l - base;
}

/* stp_mask is not shifted, so equivalent to coords.arraytype or eg (RTS_MASK) or (INDIR_MASK), not << 48 */
static inline char *process_contiguous_block(char *start, size_t *cur_ele_i_p, size_t top_ele_i, ssize_t delta,
		uint_least64_t stp_mask)
{
	assert(RTS_MASK == stp_mask || INDR_MASK == stp_mask);
	size_t cur_ele_i;
	mstr_sort_array_element *cur_ele_p;
	char *top = start, *tmp;

	assert((*stringpool.sort_array_pp)->array[REAL_SORT_INDEX((*cur_ele_i_p), (*stringpool.sort_array_pp))].addr == start);
	for (cur_ele_i = *cur_ele_i_p; cur_ele_i < top_ele_i; cur_ele_i++)
	{
		cur_ele_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(cur_ele_i, (*stringpool.sort_array_pp))];
		assert(cur_ele_p->addr >= start);
		assert(cur_ele_p->str_p->addr == cur_ele_p->addr);
		if (cur_ele_p->addr > top)
			break;
		tmp = cur_ele_p->addr + cur_ele_p->len;
		if (tmp > top)
			top = tmp;
		SET_COORD_INDEX_AND_ARRAYTYPE(cur_ele_p->str_p->in_array, cur_ele_i, stp_mask | SORT_ARRAY_MASK);
		cur_ele_p->addr -= delta;
		cur_ele_p->str_p->addr = cur_ele_p->addr;
	}
	*cur_ele_i_p = cur_ele_i;
	return top;
}
static inline char *process_contiguous_block_check(char *start, size_t *cur_ele_i_p, size_t top_ele_i, ssize_t delta,
		uint_least64_t stp_mask, mstr ***cstr_p, mstr **topstr)
{
	assert(RTS_MASK == stp_mask || INDR_MASK == stp_mask);
	size_t cur_ele_i;
	mstr_sort_array_element *cur_ele_p;
	mstr **cstr;
	char *top = start, *tmp;

	assert((*stringpool.sort_array_pp)->array[REAL_SORT_INDEX((*cur_ele_i_p), (*stringpool.sort_array_pp))].addr == start);
	assert((topstr - (*cstr_p)) == (top_ele_i - *cur_ele_i_p));
	for (cur_ele_i = *cur_ele_i_p, cstr = *cstr_p; cur_ele_i < top_ele_i; cur_ele_i++, cstr++)
	{
		cur_ele_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(cur_ele_i, (*stringpool.sort_array_pp))];
		assert(*cstr == cur_ele_p->str_p || ((*cstr)->addr == cur_ele_p->str_p->addr) ||
				((*cstr)->addr == (cur_ele_p->str_p->addr - delta)));
		assert(cur_ele_p->str_p->addr == cur_ele_p->addr);
		assert(cur_ele_p->addr >= start);
		if (cur_ele_p->addr > top)
			break;
		tmp = cur_ele_p->addr + cur_ele_p->len;
		if (tmp > top)
			top = tmp;
		SET_COORD_INDEX_AND_ARRAYTYPE(cur_ele_p->str_p->in_array, cur_ele_i, stp_mask | SORT_ARRAY_MASK);
		cur_ele_p->addr -= delta;
		cur_ele_p->str_p->addr = cur_ele_p->addr;
	}
	*cur_ele_i_p = cur_ele_i;
	*cstr_p = cstr;
	return top;
}
static inline void copy_to_stringpool(size_t cur_ele_i, size_t top_ele_i, uint_least64_t stp_mask)
{
	char *start, *top;
	ssize_t delta;
	size_t blklen;
	mstr_sort_array_element *cur_ele_p;
	DEBUG_ONLY(size_t last_ele_i;)

	while (cur_ele_i < top_ele_i)
	{
		cur_ele_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(cur_ele_i, (*stringpool.sort_array_pp))];
		start = cur_ele_p->addr;
		delta = start - (char *)stringpool.free;
		DEBUG_ONLY(last_ele_i = cur_ele_i);
		top = process_contiguous_block(start, &cur_ele_i, top_ele_i, delta, stp_mask);
		assert(last_ele_i < cur_ele_i);
		assert(top > start);
		blklen = top - start;
		memcpy(stringpool.free, start, blklen);
		stringpool.free += blklen;
	}
}
static inline void copy_to_stringpool_check(size_t cur_ele_i, size_t top_ele_i,
		uint_least64_t stp_mask, mstr **cstr, mstr **topstr)
{
	char *start, *top;
	ssize_t delta;
	size_t blklen;
	mstr_sort_array_element *cur_ele_p;
	DEBUG_ONLY(size_t last_ele_i;)

	while (cur_ele_i < top_ele_i)
	{
		cur_ele_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(cur_ele_i, (*stringpool.sort_array_pp))];
		start = cur_ele_p->addr;
		delta = start - (char *)stringpool.free;
		DEBUG_ONLY(last_ele_i = cur_ele_i);
		top = process_contiguous_block_check(start, &cur_ele_i, top_ele_i, delta, stp_mask, &cstr, topstr);
		assert(last_ele_i < cur_ele_i);
		assert(top > start);
		blklen = top - start;
		memcpy(stringpool.free, start, blklen);
		stringpool.free += blklen;
	}
	assert(cstr == topstr);
	assert(cur_ele_i == top_ele_i);
}
static inline void move_within_stringpool(size_t cur_ele_i, size_t top_ele_i, uint_least64_t stp_mask)
{
	char *start, *top;
	ssize_t delta;
	size_t blklen;
	mstr_sort_array_element *cur_ele_p;
	DEBUG_ONLY(size_t last_ele_i;)

	while (cur_ele_i < top_ele_i)
	{
		cur_ele_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(cur_ele_i, (*stringpool.sort_array_pp))];
		start = cur_ele_p->addr;
		delta = start - (char *)stringpool.free;
		DEBUG_ONLY(last_ele_i = cur_ele_i;)
		top = process_contiguous_block(start, &cur_ele_i, top_ele_i, delta, stp_mask);
		assert(last_ele_i < cur_ele_i);
		assert(top > start);
		blklen = top - start;
		if (delta)
		{
			assert((stringpool.free + blklen) <= stringpool.top);
			memmove(stringpool.free, start, blklen);
		}
		stringpool.free += blklen;
	}
}
static inline void move_within_stringpool_check(size_t cur_ele_i, size_t top_ele_i, uint_least64_t stp_mask, mstr **cstr,
		mstr **topstr)
{
	char *start, *top;
	ssize_t delta;
	size_t blklen;
	mstr_sort_array_element *cur_ele_p;
	DEBUG_ONLY(size_t last_ele_i;)

	while (cur_ele_i < top_ele_i)
	{
		cur_ele_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(cur_ele_i, (*stringpool.sort_array_pp))];
		start = cur_ele_p->addr;
		delta = start - (char *)stringpool.free;
		DEBUG_ONLY(last_ele_i = cur_ele_i;)
		top = process_contiguous_block_check(start, &cur_ele_i, top_ele_i, delta, stp_mask, &cstr, topstr);
		assert(last_ele_i < cur_ele_i);
		assert(top > start);
		blklen = top - start;
		if (delta)
		{
			assert((stringpool.free + blklen) <= stringpool.top);
			memmove(stringpool.free, start, blklen);
		}
		stringpool.free += blklen;
	}
	assert(cstr == topstr);
	assert(cur_ele_i == top_ele_i);
}

/* Macro used intermittently in code to debug string pool garbage collection. Set to 1 to enable */
#if 0
/* Debug FPRINTF with pre and post requisite flushing of appropriate streams  */
#define DBGSTPGCOL(x) DBGFPF(x)
#else
#define DBGSTPGCOL(x)
#endif
static void expand_stp(size_t new_size)	/* BYPASSOK */
{
	if (retry_if_expansion_fails)
		ESTABLISH(stp_gcol_ch);
	assert(IS_GTM_IMAGE || IS_MUPIP_IMAGE);
	assert(!stringpool_unexpandable);
	DBGSTPGCOL((stderr, "expand_stp(new_size=%llu)\n", new_size));
	stp_init(new_size);
	if (retry_if_expansion_fails)
		REVERT;
	return;
}

#ifndef STP_MOVE
#ifdef DEBUG
/* Verify the current symbol table that we will be processing later in "stp_gcol". This version is callable from anywhere
 * and is a great debuging tool for corrupted mstrs in local variable trees. Uncomment the call near the top of stp_gcol to
 * verify it will catch what you want it to catch and sprinkle calls around other places as necessary. Be warned, this can
 * *really* slow things down so use judiciously. SE 10/2007
 */
void stp_vfy_mval(void)
{
	lv_blk		*lv_blk_ptr;
	lv_val		*lvp, *lvlimit;
	mstr		*x;
	mval		*mv;
	char		*addr;
	symval		*sym;
	symval		*symtab;

	for (symtab = curr_symval; NULL != symtab; symtab = symtab->last_tab)
	{
		for (lv_blk_ptr = symtab->lv_first_block; lv_blk_ptr; lv_blk_ptr = lv_blk_ptr->next)
		{
			for (lvp = (lv_val *)LV_BLK_GET_BASE(lv_blk_ptr), lvlimit = LV_BLK_GET_FREE(lv_blk_ptr, lvp);
				lvp < lvlimit; lvp++)
			{
				sym = LV_SYMVAL(lvp);
				if (NULL == sym)
					continue;
				assert(SYM_IS_SYMVAL(sym) && (sym == symtab));
				mv = &lvp->v;
				if (MV_IS_STRING(mv))
				{
					x = &mv->str;
					if (x->len)
					{
						addr = x->addr;
						if (IS_PTR_IN_RANGE(addr, stringpool.base, stringpool.free))	/* BYPASSOK */
							assert(0 < (x)->len);
					}
				}
				assert(LV_IS_BASE_VAR(lvp));
				assert(lvTreeIsWellFormed(LV_CHILD(lvp)));
			}
		}
	}
}

boolean_t is_stp_space_available(ssize_t space_needed)
{
	/* Asserts added here are tested every time any module in the codebase does stringpool space checks */
	assert(!stringpool_unusable);
	return IS_STP_SPACE_AVAILABLE_PRO(space_needed);
}

#endif  /* DEBUG */

void mv_parse_tree_collect(mvar *node);

void mv_parse_tree_collect(mvar *node)
{
	int stp_put_int;

	MSTR_STPG_ADD(&node->mvname);
	if (node->lson)
		mv_parse_tree_collect(node->lson);
	if (node->rson)
		mv_parse_tree_collect(node->rson);
}
#endif /* #ifndef STP_MOVE */

#ifdef STP_MOVE
/* garbage collect and move range [from,to) to stringpool adjusting all mvals/mstrs pointing in this range */
void stp_move(char *stp_move_from, char *stp_move_to)
#else
/* garbage collect and create enough space for space_asked bytes */
void stp_gcol(size_t space_asked)	/* BYPASSOK */
#endif
{
#	ifdef STP_MOVE
	int				space_asked = 0, stp_move_count = 0, stp_move_count_old = 0;
	boolean_t			in_spool;
	size_t				move_adjustment = 0;
#	endif
	unsigned long 			num_sorted, sort_room_needed;
	unsigned char			*strpool_base, *straddr, *tmpaddr, *begaddr, *endaddr;
	int				index, fixup_cnt;
	ssize_t				space_before_compact, space_after_compact, space_after_compact_old, blklen, delta;
	ssize_t				space_reclaim, reserved_bytes, space_needed, tmplen, tmplen2, totspace, totspace_old;
	ssize_t				prev_stp_size = 0;
	io_log_name			*l;		/* logical name pointer		*/
	lv_blk				*lv_blk_ptr;
	lv_val				*lvp, *lvlimit;
	lvTreeNode			*node, *node_limit;
	mstr				**cstr = NULL, *x, **tmp_topstr = NULL;
	mv_stent			*mvs;
	mval				*m, **mm, **mmtop, *mtop;
	intszofptr_t			lv_subs;
	stack_frame			*sf, *old_sf;
	tp_frame			*tf;
	zwr_sub_lst			*zwr_sub;
	ihdtyp				*ihdr;
	int				*incr_factor, killcnt, *low_reclaim_passes, save_errno;
	ssize_t				stp_incr;
	ht_ent_objcode	 		*tabent_objcode, *topent;
	ht_ent_mname			*tabent_mname, *topent_mname;
	ht_ent_addr			*tabent_addr, *topent_addr;
	ht_ent_str			*tabent_lit, *topent_lit;
	mliteral			*mlit;
	zwr_alias_var			*zav;
	cache_entry			*cp;
	var_tabent			*vent, *vartop;
	symval				*symtab;
	lv_xnew_var			*xnewvar;
	lvzwrite_datablk		*lvzwrblk;
	tp_var				*restore_ent;
	boolean_t			non_mandatory_expansion, exp_gt_spc_needed;
	routine_source			*rsptr;
	glvn_pool_entry			*slot, *top;
	int				i, n;
	unsigned int			temp_cnt;
	unsigned char			*old_free;
	d_rm_struct			*rm_ptr;
	unsigned long			list_count, gcol_list_count, fcount;
	intrpt_state_t		  	prev_intrpt_state;
	struct sort_array_element	*um_str;
	mstr_sort_array_element		*sort_ele_i_p, *sort_ele_j_p, *sort_ele_k_p, *sort_ele_l_p, *sort_ele_m_p, *sort_ele_top_p;
	mstr_sort_array_element		*sort_ele_new_p;
	size_t				sort_ele_i_i, sort_ele_j_i, sort_ele_k_i, sort_ele_l_i, sort_ele_m_i, sort_ele_top_i;
	mstr_protect_array_element	*protect_ele_j_p, *protect_ele_k_p, *protect_ele_top_p, *protect_ele_base_p;
	mstr_protect_array_element	*protect_ele_new_p;
	mstr_sort_array_element		temp_sort_array_ele;
	mstr_protect_array_element	temp_protect_array_ele;
	size_t				protect_ele_j_i, protect_ele_k_i, protect_ele_top_i;
	size_t				old_start_ele, new_start_ele, start_ele, sz, cnt;
	mstr_protect_array		*new_protect_array_p;
	char				*largest_presorted_addr;
	UTF8_ONLY(utfcgr		*utfcgrp;)
	const uint_least64_t stp_mask = (stringpool.sort_array_pp != rts_stringpool.sort_array_pp) ? INDR_MASK : RTS_MASK;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Asserts added here are tested every time any module in the codebase does stringpool garbage collection */
	assert(!stringpool_unusable);
	assert(!stringpool_unexpandable);
	assert(!timer_in_handler || process_exiting);
	stringpool.gcols++;
	assert(!stringpool.pending_moves);
	assert(!rts_stringpool.pending_moves);
	assert(!indr_stringpool.pending_moves);
#	ifdef DOUBLECHECK_GCOL
	old_ops = 0;
#	endif
	if ((stringpool.sort_array_pp != rts_stringpool.sort_array_pp))
	{
		ctime_gcol = true;
	} else
	{
		ctime_gcol = false;
	}
#	ifndef STP_MOVE
	/* Before we get cooking with our stringpool GC, check if it is appropriate to call lv_val garbage collection.
	 * This is data that can get orphaned with no way to access it when aliases are used. This form of GC is only done
	 * if aliases are actively being used. It is not called with every stringpool garbage collection but every "N"
	 * calls to this routine (when GCing the runtime stringpool only). The value of N is self-tuning to some extent.
	 * There are mins and maximums to be observed.
	 */
	if ((stringpool.base == rts_stringpool.base) && (NULL != curr_symval) && curr_symval->alias_activity)
	{	/* We have alias stuff going on so see if we need a GC */
		++SPGC_since_LVGC;
		DBGRFCT((stderr, "stp_gcol: Current interval: %d  Intervals till LVGC: %d\n",
			 SPGC_since_LVGC, MAX(0, LVGC_interval - SPGC_since_LVGC)));
		if (suspend_lvgcol)
		{
			DBGRFCT((stderr, "stp_gcol: Bypassing LVGC check due to suspend_lvgcol\n"));
		} else if (SPGC_since_LVGC >= LVGC_interval)
		{	/* Time for a GC */
			killcnt = als_lvval_gc();
			DBGRFCT((stderr, "stp_gcol: Previous interval: %d ", LVGC_interval));
			if (0 == killcnt)
				/* Nothing recovered .. be less aggresive */
				LVGC_interval = MIN((LVGC_interval + 1), MAX_SPGC_PER_LVGC);
			else
				/* Some data was recovered, be more aggresive */
				LVGC_interval = MAX((LVGC_interval - 5), MIN_SPGC_PER_LVGC);
			DBGRFCT((stderr, " New interval: %d\n", LVGC_interval));
			/* SPGC_since_LVGC is cleared by als_lv_gc() */
		}
	}
#	ifdef DEBUG_REFCNT
	else if ((stringpool.base == rts_stringpool.base) && (NULL != curr_symval))
	{
		DBGRFCT((stderr, "stp_gcol: lvgcol check bypassed for lack of aliasness\n"));
	}
#	endif	/* DEBUG_REFCNT */
	if (IS_LVMON_ACTIVE && (stringpool.base == rts_stringpool.base))
		lvmon_pull_values(1);			/* Pull set of values into index 1 if rts stringpool */
#	endif	/* !STP_MOVE */
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	assert(stringpool.invokestpgcollevel <= stringpool.top);
	assert(CHK_BOUNDARY_ALIGNMENT(stringpool.top) == 0);
	/* DEBUG_ONLY(stp_vfy_mval();) uncomment to debug lv corruption issues.. */
#	ifdef STP_MOVE
	assert(stp_move_from < stp_move_to); /* why did we call with zero length range, or a bad range? */
	/* Assert that range to be moved does not intersect with stringpool range.
	 * Since stp_move_from<->stp_move_to is a range, it can hit the edges of the string pool.
	 */
	assert((stp_move_to <= (char *)stringpool.base) || (stp_move_from >= (char *)stringpool.top));
	/* In the new algorithm we want the move-strs to sort after the stp-resident ones, and since we sort
	 * based on the local copy of the addr anyway for efficiency (since we need it anyway), just adjust the
	 * local copy to fall after the stringpool.
	 */
	if (stp_move_to <= (char *)stringpool.base)
		move_adjustment = (char *)stringpool.top - stp_move_from;
#	endif
	space_needed = ROUND_UP2(space_asked, NATIVE_WSIZE);
	assert(0 == (INTPTR_T)stringpool.base % NATIVE_WSIZE);
	if (stringpool.base == rts_stringpool.base)
	{
		low_reclaim_passes = &rts_stp_low_reclaim_passes;
		incr_factor = &rts_stp_incr_factor;
	} else if (stringpool.base == indr_stringpool.base)
	{
		low_reclaim_passes = &indr_stp_low_reclaim_passes;
		incr_factor = &indr_stp_incr_factor;
	} else
	{
		assertpro(FALSE && stringpool.base);	/* neither rts_stringpool, nor indr_stringpool */
	}
	if (NULL == *stringpool.sort_array_pp)
	{
		glist_new_sort_array(stringpool.sort_array_pp, STP_ARRAY_STARTITEMS, ctime_gcol ? INDR_SORT_ARRAY : RTS_SORT_ARRAY);
	}
	if (NULL == *stringpool.protect_array_pp)
	{
		glist_new_protect_array(stringpool.protect_array_pp, STP_ARRAY_STARTITEMS,
					ctime_gcol ? INDR_PROTECT_ARRAY : RTS_PROTECT_ARRAY);
	}
	if (NULL == stp_array)
	{
		stp_array_size = STP_MAXITEMS;
		stp_array = (mstr **)malloc(stp_array_size * SIZEOF(mstr *));
	}
	cstr = NULL;
	topstr = array = stp_array;
	arraytop = topstr + stp_array_size;
	/* If dqloop == 0 then we got here from mcompile. If literal_chain.que.fl == 0 then put_lit was never
	 * done as is true in gtcm_server. Test for cache_table.size is to check that we have not done a
	 * cache_init() which would be true if doing mumps standalone compile.
	 */
	/* Do this here in light of DOUBLECHECK asserts that old algorithm does not find an unprotected str in need */

	if (((stringpool.base == rts_stringpool.base) && (cache_table.size)))
	{
		if (frame_pointer)
		{
			for (sf = frame_pointer, old_sf = sf->old_frame_pointer; sf && (sf < (stack_frame *)stackbase); sf = old_sf)
			{ /* Cover temp mvals in use */
				old_sf = sf->old_frame_pointer;
				if (NULL == old_sf)
				{	/* If trigger enabled, may need to jump over a base frame */
					GTMTRIG_ONLY(if (SFT_TRIGR & sf->type) { /* We have a trigger base frame, back up over it */
							old_sf = *(stack_frame **)(sf + 1);
							assert(old_sf);
							assert(old_sf->old_frame_pointer);
					} else if (SFT_BASE & sf->type))
					NON_GTMTRIG_ONLY(if (SFT_BASE & sf->type))
					{
						old_sf = *(stack_frame **)(sf + 1);
						if ((old_sf >= (stack_frame *)stackbase) || (old_sf < (stack_frame *)stacktop))
							old_sf = NULL;
					}
				}
				assert(sf->temps_ptr);
#				ifndef STP_MOVE
				if (PTEMP_CNT(sf) != INVALID_PTEMP_CNT)	/* Only need to check the first
									 * frame that was on stack at last gcol
									 */
					old_sf = NULL;
#				endif
				m = (mval *)sf->temps_ptr;
				temp_cnt = 0;
				for (mtop = m + sf->temp_mvals; m < mtop; m++)
				{
					assert(!(sf->type & SFT_DM) || !MV_DEFINED(m));
#					ifdef STP_MOVE
					temp_cnt += glist_sync_temp_move(m, stp_move_from, stp_move_to);
#					else
					temp_cnt += glist_sync_temp(m);
#					endif
				}
				SET_PTEMP_CNT(sf, temp_cnt);
			}
		}
		glist_sync_temp(TADR(trestart_xpel_rtnlab));	/* Typically affected by zero garbage collections, so lazily
								 * protect only on first. Always in stringpool if needing
								 * protection, so reuse temp fn.
								 */
#		ifdef STP_MOVE
		for (symtab = curr_symval; NULL != symtab; symtab = symtab->last_tab)
		{
			for (tabent_mname = symtab->h_symtab.base, topent_mname = symtab->h_symtab.top;
			     tabent_mname < topent_mname; tabent_mname++)
			{
				if (HTENT_VALID_MNAME(tabent_mname, lv_val, lvp))
					glist_sync_static_str_move(&(tabent_mname->key.var_name), stp_move_from, stp_move_to);
			}
		}
		for (tabent_objcode = cache_table.base, topent = cache_table.top; tabent_objcode < topent; tabent_objcode++)
		{
			if (HTENT_VALID_OBJCODE(tabent_objcode, cache_entry, cp))
				glist_sync_static_str_move(&(tabent_objcode->key.str), stp_move_from, stp_move_to);
		}
		for (mvs = mv_chain; mvs < (mv_stent *)stackbase; mvs = (mv_stent *)((char *)mvs + mvs->mv_st_next))
		{
			switch (mvs->mv_st_type)
			{
				case MVST_STAB:
					if (NULL != (symtab = mvs->mv_st_cont.mvs_stab))
					{	/* if initalization of the table was successful */
						for (lv_blk_ptr = symtab->lvtreenode_first_block; NULL != lv_blk_ptr;
						     lv_blk_ptr = lv_blk_ptr->next)
						{
							for (node = (lvTreeNode *)LV_BLK_GET_BASE(lv_blk_ptr),
								     node_limit = LV_BLK_GET_FREE(lv_blk_ptr, node);
							     node < node_limit; node++)
							{
								/* node could be actively in use or free (added to the symval's
								 * lvtreenode_flist). Ignore the free ones. Those should have
								 * their "parent"  set to NULL.
								 */

								if (NULL == LV_PARENT(node))
								{
									assert(!node->v.str.in_array);
									assert(!glist_lvTreeNode_key_protected(node));
									continue;
								}
								assert(!LV_IS_BASE_VAR(node));
								/* For a subscripted variable, we need to protect the subscript
								 * (if string) as well as the subscripted node value (if exists
								 * and is of type string).
								 */

								if (LV_NODE_KEY_IS_STRING(node))
								{
									ASSERT_LV_NODE_MSTR_EQUIVALENCE;
									glist_sync_lvTreeNode_key_move(node, stp_move_from,
										stp_move_to);
								}
							}
						}
					}
					break;
				default:
					break;
			}
		}
#		endif
	}
#	ifdef DOUBLECHECK_GCOL
	if (((stringpool.base != rts_stringpool.base) || (0 == cache_table.size)))
	{
#		ifndef STP_MOVE
		FOR_EACH_REACHABLE_CTIME(MSTR_STPG_ADD, MVAL_STPG_ADD);
		if (NULL != mvartab)
			mv_parse_tree_collect(mvartab);
		if (NULL != mlabtab)
			mv_parse_tree_collect((mvar *)mlabtab);
#		endif
	} else
	{
		assert(0 != cache_table.size);	/* Must have done a cache_init() */
		FOR_EACH_REACHABLE_RTIME(MSTR_STPG_ADD, MVAL_STPG_ADD, MSTR_STPG_PUT);
	}
#endif /* DOUBLECHECK_GCOL */

	/* ele_j_p points to the earliest changed-resident slot, where all slots in
	 * between j and k are changed but in the stringpool.
	 * ele_k_p points to the earliest changed-nonresident slot, where all slots in between k and l no longer reside in stpool.
	 * ele_l_p points to the earliest empty slot, where all slots in between l and m are empty.
	 * all slots in between the start (&(*stringpool.sort_array_pp)->array[0]) and j are unchanged and (we assert) stp-resident.
	 * it follows from loop invariants that;
	 * 	j == k iff no changed-resident slot before m.
	 * 	k == l iff no changed-nonresident slot before m.
	 * 	l == m iff no empty slot before m.
	 * when the loop completes 'before m' becomes 'in the array' and ptrdiffs give
	 * number of changed-resident, changed-nonresident,
	 * and empty elements, and where those segments begin.
	 */
	sz = (*stringpool.sort_array_pp)->size; /* For efficiency, should not change (we assert this) */
	start_ele = (*stringpool.sort_array_pp)->start_ele; /* Same as above */
	sort_ele_top_i = (*stringpool.sort_array_pp)->count;
	sort_ele_j_i = sort_ele_k_i = sort_ele_l_i = 0U;
	sort_ele_j_p = sort_ele_k_p = sort_ele_l_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(0, start_ele, sz)];
        DEFER_INTERRUPTS(INTRPT_IN_GCOL, prev_intrpt_state);
	stringpool.pending_moves = TRUE;
	for (sort_ele_m_i = 0U; sort_ele_m_i < sort_ele_top_i; sort_ele_m_i++)
	{
		sort_ele_m_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_m_i, start_ele, sz)];
		if (glist_sort_element_active(sort_ele_m_p))
		{
			if (!glist_element_changed(sort_ele_m_p))
			{
				assert(sort_ele_m_p->len
					&& glist_ptr_span_in_range(
						sort_ele_m_p->addr, sort_ele_m_p->len, stringpool.base, stringpool.top));
				if (sort_ele_j_i != sort_ele_k_i)
				{
					sort_ele_j_p = &(*stringpool.sort_array_pp)
								->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_j_i, start_ele, sz)];
					glist_fast_swap_sort_elements(sort_ele_m_p, sort_ele_j_p);
				}
				/* Otherwise this will be identical to the swap with k_p. Fall through to it instead */
				sort_ele_j_i++;
				/* Less elegant to duplicate below logic here but it saves us a stringpool check */
				if (sort_ele_k_i != sort_ele_l_i)
				{
					sort_ele_k_p = &(*stringpool.sort_array_pp)
								->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_k_i, start_ele, sz)];
					glist_fast_swap_sort_elements(sort_ele_m_p, sort_ele_k_p);
				}
				/* Otherwise this will be identical to the swap with l_p. Fall through instead */
				sort_ele_k_i++;
			} else if ((STP_MOVE_ONLY(in_spool = )glist_str_in_stringpool(sort_ele_m_p->str_p))
					STP_MOVE_ONLY( || glist_str_in_range(sort_ele_m_p->str_p, stp_move_from, stp_move_to)))
			{
				sort_ele_m_p->len = sort_ele_m_p->str_p->len;
				sort_ele_m_p->addr = sort_ele_m_p->str_p->addr;
#				ifdef STP_MOVE
				if (!in_spool)
				{
					stp_move_count++;
					sort_ele_m_p->addr += move_adjustment;
				}
#				endif
				assert(sort_ele_m_p->char_len != CHARLEN_LVTREE);
				if (sort_ele_k_i != sort_ele_l_i)
				{
					sort_ele_k_p = &(*stringpool.sort_array_pp)
								->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_k_i, start_ele, sz)];
					glist_fast_swap_sort_elements(sort_ele_m_p, sort_ele_k_p);
				}
				sort_ele_k_i++;
			} else
				assert(sort_ele_m_p->char_len != CHARLEN_LVTREE);
			if (sort_ele_l_i != sort_ele_m_i)
			{
				sort_ele_l_p =
					&(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_l_i, start_ele, sz)];
				glist_fast_swap_sort_elements(sort_ele_m_p, sort_ele_l_p);
			}
			sort_ele_l_i++;
		} else
		{
			/* Deletion should have zeroed out the pointer on our side, in the array. If not, we overwrote the pointer
			 * without deleting it. This is handled appropriately in pro only if the backing memory is not unmapped
			 * (in which case there could be a segfault). So assert against it to detect and fix all such cases.
			 */
			assert(!sort_ele_m_p->str_p);
			assert(sort_ele_m_p->char_len == CHARLEN_DELETED);
			/* In pro, enforce these invariants here */
			memset(sort_ele_m_p, 0, SIZEOF(*sort_ele_m_p));
			/* Do not increment any of the trailing pointers in this branch since we just added an empty to the top of
			 * the stack. If there were previously no empties, for example, it is by not incrementing l that we maintain
			 * the invariant (now m - l == 1(ptrdiff_t)).
			 */
		}
	}
	assert(sz == (*stringpool.sort_array_pp)->size);
	assert(start_ele == (*stringpool.sort_array_pp)->start_ele);
	if (sort_ele_l_i != sort_ele_m_i)
		(*stringpool.sort_array_pp)->count = sort_ele_l_i;
	fcount = (*stringpool.protect_array_pp)->fcount;
	glist_clear_protect_flist(*stringpool.protect_array_pp);
	assert(!(*stringpool.protect_array_pp)->fcount);
	protect_ele_base_p = (*stringpool.protect_array_pp)->array;
	protect_ele_top_p = &(*stringpool.protect_array_pp)->array[(*stringpool.protect_array_pp)->count];
	protect_ele_j_p = (*stringpool.protect_array_pp)->array;
	for ( ; protect_ele_j_p < protect_ele_top_p; protect_ele_j_p++)
	{
		if (!glist_protect_element_active((*stringpool.protect_array_pp), protect_ele_j_p))
		{
			if (fcount == protect_ele_top_p - protect_ele_j_p)
			{
				protect_ele_top_p -= fcount;
				(*stringpool.protect_array_pp)->count -= fcount;
				protect_ele_j_p--;
				fcount = 0;
				continue;
			}
			fcount--;
			protect_ele_top_p--;
			(*stringpool.protect_array_pp)->count--;
			/* Future optimization - break out the final loop iteration to avoid check. */
			if (protect_ele_top_p != protect_ele_j_p)
			{
				if (!!protect_ele_top_p->str_p)
				{
					glist_fast_swap_protect_elements(protect_ele_j_p, protect_ele_top_p);
					glist_set_index(&protect_ele_j_p->str_p->in_array, protect_ele_j_p - protect_ele_base_p);
				}
			}
			protect_ele_top_p->str_p = NULL;
			DEBUG_GCOL_ONLY(protect_ele_top_p->callerid = NULL;)
			protect_ele_j_p--;
			continue;
		}
		if ((STP_MOVE_ONLY(in_spool = )glist_str_in_stringpool(protect_ele_j_p->str_p))
			STP_MOVE_ONLY( || glist_str_in_range(protect_ele_j_p->str_p, stp_move_from, stp_move_to)))
		{
#			ifdef STP_MOVE
			if (!in_spool)
				stp_move_count++;
#			endif
			assert(sort_ele_k_i <= sort_ele_l_i);
			if (sort_ele_k_i != sort_ele_l_i)
			{
				/* Swap with a changed-nonresident and become a changed-resident stringpool */
				temp_protect_array_ele = *protect_ele_j_p;
				sort_ele_k_p = &(*stringpool.sort_array_pp)->array[
					REAL_SORT_INDEX(sort_ele_k_i, (*stringpool.sort_array_pp))];
				protect_ele_j_p->str_p = sort_ele_k_p->str_p;
				DEBUG_GCOL_ONLY(protect_ele_j_p->callerid = sort_ele_k_p->callerid;)
				assert(glist_mval_in_sync_nointeg(&dollar_zgbldir));
				SET_COORD_INDEX_AND_ARRAYTYPE(protect_ele_j_p->str_p->in_array,
					protect_ele_j_p - protect_ele_base_p, stp_mask | PROTECT_ARRAY_MASK);
				assert(glist_mval_in_sync_nointeg(&dollar_zgbldir));
				sort_ele_k_p->str_p = temp_protect_array_ele.str_p;
				DEBUG_GCOL_ONLY(sort_ele_k_p->callerid = temp_protect_array_ele.callerid;)
				sort_ele_k_p->char_len = CHARLEN_NORMAL;
				sort_ele_k_p->addr = temp_protect_array_ele.str_p->addr;
				sort_ele_k_p->len = temp_protect_array_ele.str_p->len;
#				ifdef STP_MOVE
				if (!in_spool)
					sort_ele_k_p->addr += move_adjustment;
#				endif
				sort_ele_k_i++;
			} else
			{
				sort_ele_new_p = glist_new_sort_array_element(stringpool.sort_array_pp);
				sort_ele_new_p->str_p = protect_ele_j_p->str_p;
				DEBUG_GCOL_ONLY(sort_ele_new_p->callerid = protect_ele_j_p->callerid);
				sort_ele_new_p->addr = sort_ele_new_p->str_p->addr;
#				ifdef STP_MOVE
				if (!in_spool)
					sort_ele_new_p->addr += move_adjustment;
#				endif
				sort_ele_new_p->len = sort_ele_new_p->str_p->len;
				sort_ele_new_p->char_len = CHARLEN_NORMAL;
				fcount++; /* As if it was in the freelist from the beginning */
				protect_ele_j_p->str_p = NULL;
				protect_ele_j_p--;
			}
		}
	}
	assert(fcount == 0);
	while (sort_ele_k_i < sort_ele_l_i)
	{
		sort_ele_l_i--;
		sort_ele_l_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(sort_ele_l_i, (*stringpool.sort_array_pp))];
		protect_ele_new_p = glist_new_protect_array_element(stringpool.protect_array_pp);
		protect_ele_new_p->str_p = sort_ele_l_p->str_p;
		DEBUG_GCOL_ONLY(protect_ele_new_p->callerid = sort_ele_l_p->callerid;)
		sort_ele_l_p->str_p = NULL;
		sort_ele_l_p->char_len = CHARLEN_DELETED;
		(*stringpool.sort_array_pp)->count--;
		SET_COORD_INDEX_AND_ARRAYTYPE(protect_ele_new_p->str_p->in_array, (*stringpool.protect_array_pp)->count - 1,
			stp_mask | PROTECT_ARRAY_MASK);
	}
	/* At this point:
	 * Everything in the protect array is not pointed into the stringpool/[stp_move_from,stp_move_to), and its original string
	 * has the correct coords. The sort array has up-to-date caches of the string addr and len, but the original strings do
	 * not have correct coordinates. The reason for this is that we want to defer these writes to when we are updating the str
	 * addrs and lens later so it is contiguous with another write to (probably) the same cacheline.
	 * Both arrays are compact and have only active members up to count.
	 * In addition, all the sort_array_elements up to sort_ele_j_p are unchanged and will not be sorted. We only sort members
	 * of the array after that, then we merge the two virtual arrays in place (recently-sorted j->top with start->j.
	 */
	DOUBLECHECK_GCOL_ASSERT(old_ops >= (*stringpool.sort_array_pp)->count + (*stringpool.protect_array_pp)->count);
	/* Handle potential pointer changes */
	assert((*stringpool.sort_array_pp)->count >= sort_ele_j_i);
	num_sorted = (*stringpool.sort_array_pp)->count - sort_ele_j_i;
	straddr = NULL;
	totspace = 0;
	sort_ele_i_i = sort_ele_l_i = 0;
	sort_ele_top_i = (*stringpool.sort_array_pp)->count;
	old_start_ele = new_start_ele = (*stringpool.sort_array_pp)->start_ele;
	sz = (*stringpool.sort_array_pp)->size; /* For efficiency, should not change (we assert this) */
	cnt = (*stringpool.sort_array_pp)->count; /* Same as above */
	sort_room_needed = 0;
	if (num_sorted)
	{
		sort_ele_k_i = sort_ele_top_i - 1;
		glist_ensure_sort_array_room(stringpool.sort_array_pp, num_sorted);
		old_start_ele = new_start_ele = (*stringpool.sort_array_pp)->start_ele;
		sz = (*stringpool.sort_array_pp)->size; /* For efficiency, should not change (we assert this) */
		sort_ele_j_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(sort_ele_j_i, (*stringpool.sort_array_pp))];
		sort_ele_k_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(sort_ele_k_i, (*stringpool.sort_array_pp))];
		if (sort_ele_k_p >= sort_ele_j_p)
			stpg_sort_new_contig(sort_ele_j_p, sort_ele_k_p);
		else
			stpg_sort_new((*stringpool.sort_array_pp), sort_ele_j_i, sort_ele_k_i);
		/* Need to determine how much room we need before old_start_ele. The answer is: however many newly-sorted elements
		 * will be sorted before the last of the presorted elements. All the other elements will by construction go
		 * after the final presorted element.
		 */
		if (num_sorted != (*stringpool.sort_array_pp)->count)
		{
			largest_presorted_addr = (*stringpool.sort_array_pp)
							 ->array[REAL_SORT_INDEX(sort_ele_j_i - 1, (*stringpool.sort_array_pp))]
							 .addr;
			sort_room_needed = glist_bsearch_sort_array(
				(*stringpool.sort_array_pp), sort_ele_j_i, sort_ele_top_i, largest_presorted_addr);
		}
		/* The sort_array is now sorted from [sort_ele_i_p, sort_ele_j_p) and [sort_ele_j_p, sort_ele_top_p).
		 * And there is enough room at the end of the array to hold the second segment at least.
		 * We now re-imagine the circular buffer as starting at the beginning of that segment which is
		 * spacious enough to hold the newly-sorted section.
		 */
		/* There are two real segments of the array:
		 * [0, sort_ele_j_i) is the pre-sorted subarray
		 * [sort_ele_j_i, sort_ele_top_i) is the subarray we just sorted in stpg_sort_new.
		 */
		if (sort_room_needed > old_start_ele)
			(*stringpool.sort_array_pp)->start_ele = new_start_ele = old_start_ele + sz - sort_room_needed;
		else
			(*stringpool.sort_array_pp)->start_ele = new_start_ele = old_start_ele - sort_room_needed;
#ifdef	DEBUG
		assert(old_start_ele != REAL_SORT_INDEX_ELE_SZ(sort_ele_top_i, old_start_ele, sz));
		if (old_start_ele > REAL_SORT_INDEX_ELE_SZ(sort_ele_top_i, old_start_ele, sz))
			assert((new_start_ele >= REAL_SORT_INDEX_ELE_SZ(sort_ele_top_i, old_start_ele, sz))
				&& (new_start_ele <= old_start_ele));
		else
			assert(!((new_start_ele < REAL_SORT_INDEX_ELE_SZ(sort_ele_top_i, old_start_ele, sz))
				 && (new_start_ele > old_start_ele)));
		assert((new_start_ele != old_start_ele) || (sort_room_needed == 0));
#endif
		sort_ele_k_i = sort_ele_j_i;
		sort_ele_i_i = sort_ele_l_i = 0;
		sort_ele_i_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_i_i, old_start_ele, sz)];
		sort_ele_k_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_k_i, old_start_ele, sz)];
		sort_ele_l_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_l_i, new_start_ele, sz)];
		for (; sort_ele_l_p != sort_ele_i_p; sort_ele_l_i++,
			sort_ele_l_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_l_i, new_start_ele, sz)])
		{
			assert((sort_ele_l_p != sort_ele_k_p) && (sort_ele_l_p != sort_ele_i_p));
			if (GCOL_SORTS_BEFORE(sort_ele_k_p, sort_ele_i_p))
			{
				tmpaddr = (unsigned char *)sort_ele_k_p->addr;
				tmplen = sort_ele_k_p->len;
				*sort_ele_l_p = *sort_ele_k_p;
				sort_ele_k_i++;
				sort_ele_k_p = &(*stringpool.sort_array_pp)
							->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_k_i, old_start_ele, sz)];
			} else
			{
				tmpaddr = (unsigned char *)sort_ele_i_p->addr;
				tmplen = sort_ele_i_p->len;
				*sort_ele_l_p = *sort_ele_i_p;
				sort_ele_i_i++;
				sort_ele_i_p = &(*stringpool.sort_array_pp)
							->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_i_i, old_start_ele, sz)];
			}
			if (tmpaddr + tmplen > straddr)
			{
				tmplen2 = ((tmpaddr >= straddr) ? tmplen : tmpaddr + tmplen - straddr);
				assert(0 < tmplen2);
				totspace += tmplen2;
				straddr = tmpaddr + tmplen;
			}
		}
		assert(sort_ele_i_p == sort_ele_l_p);
		for (; sort_ele_i_i < sort_ele_j_i; sort_ele_i_i++, sort_ele_l_i++,
			sort_ele_l_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_l_i, new_start_ele, sz)])
		{
			tmpaddr = (unsigned char *)sort_ele_l_p->addr;
			tmplen = sort_ele_l_p->len;
			if (tmpaddr + tmplen > straddr)
			{
				tmplen2 = ((tmpaddr >= straddr) ? tmplen : tmpaddr + tmplen - straddr);
				assert(0 < tmplen2);
				totspace += tmplen2;
				straddr = tmpaddr + tmplen;
			}
		}
		if (sort_ele_k_i < sort_ele_top_i)
		{
			sort_ele_l_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_l_i, new_start_ele, sz)];
			sort_ele_k_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_k_i, old_start_ele, sz)];
			for (; sort_ele_l_i < cnt; sort_ele_l_i++, sort_ele_k_i++,
				sort_ele_l_p = &(*stringpool.sort_array_pp)
					->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_l_i, new_start_ele, sz)],
				sort_ele_k_p = &(*stringpool.sort_array_pp)
					->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_k_i, old_start_ele, sz)])
			{
				if (sort_ele_k_p != sort_ele_l_p)
					*sort_ele_l_p = *sort_ele_k_p;
				tmpaddr = (unsigned char *)sort_ele_k_p->addr;
				tmplen = sort_ele_k_p->len;
				if (tmpaddr + tmplen > straddr)
				{
					tmplen2 = ((tmpaddr >= straddr) ? tmplen : tmpaddr + tmplen - straddr);
					assert(0 < tmplen2);
					totspace += tmplen2;
					straddr = tmpaddr + tmplen;
				}
			}
		}
		assert(sort_ele_k_i == sort_ele_top_i);
		assert(cnt == (*stringpool.sort_array_pp)->count);
		assert(sz == (*stringpool.sort_array_pp)->size);
	} else if (sort_ele_i_i < sort_ele_j_i)
	{
		/* Assert that the only remaining 'unfinalized' portion is this one, the one that was already sorted */
		/* Assert that the new array up to sort_ele_i_i has been filled with the sorted combination of the first
		 * part (maybe len 0) of the presorted subarray and the entirety of the newly sorted one.
		 */
		assert((REAL_SORT_INDEX_ELE_SZ(sort_ele_i_i, old_start_ele, sz)
			== REAL_SORT_INDEX_ELE_SZ(sort_ele_l_i, new_start_ele, sz)));
		/* No copying necessary since the subarray between i,j is already in its rightful place, but loop
		 * to capture space_needed information. It might be possible to capture this information in the first loop
		 * over sort_elements, but we would need a way to roll off the totspace consumed by any elements which were merged
		 * into the final array above already, which is complicated (not clear it
		 * can be done without using additional space).
		 */
		sort_ele_l_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_l_i, new_start_ele, sz)];
		for ( ; sort_ele_l_i < cnt; sort_ele_l_i++,
			sort_ele_l_p = &(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX_ELE_SZ(sort_ele_l_i, new_start_ele, sz)])
		{
			tmpaddr = (unsigned char *)sort_ele_l_p->addr;
			tmplen = sort_ele_l_p->len;
			if (tmpaddr + tmplen > straddr)
			{
				tmplen2 = ((tmpaddr >= straddr) ? tmplen : tmpaddr + tmplen - straddr);
				assert(0 < tmplen2);
				totspace += tmplen2;
				straddr = tmpaddr + tmplen;
			}
		}
	}
	space_after_compact = stringpool.top - stringpool.base - totspace;
	space_before_compact = stringpool.top - stringpool.free; /* Available space before compaction */
	DEBUG_ONLY(blklen = stringpool.free - stringpool.base);
	old_free = stringpool.free;
	/* Reset stringpool.free in the hope stringpool garbage collection (and expansion if needed) would happen.
	 * If there are any errors in this process, we need to remember to restore stringpool.free as otherwise
	 * any mval in the stringpool could potentially be overwritten later (for a new mval since there is an
	 * incorrect view of available stringpool space) resulting in memory corruption.
	 */
	stringpool.free = stringpool.base;
#	ifdef DOUBLECHECK_GCOL
	totspace_old = 0;
	if (topstr != array)
	{
		stpg_sort(array, topstr - 1);
		sort_ele_top_i = (*stringpool.sort_array_pp)->count;
#		ifndef STP_MOVE
		sort_ele_i_i = 0;
#		else
		if (stp_move_count && move_adjustment)
			sort_ele_i_i = (*stringpool.sort_array_pp)->count - stp_move_count;
		else
			sort_ele_i_i = 0;
#		endif
		for (cstr = array, straddr = (unsigned char *)(*cstr)->addr; (cstr < topstr); cstr++, sort_ele_i_i++ )
		{
			assert((cstr == array) || ((*cstr)->addr >= ((*(cstr - 1))->addr)));
#			ifdef STP_MOVE
			if (sort_ele_i_i == sort_ele_top_i && stp_move_count && move_adjustment)
			{
				assert((cstr - array) == stp_move_count);
				sort_ele_i_i = 0;
				sort_ele_top_i = (*stringpool.sort_array_pp)->count - stp_move_count;
			}
#			endif
			assert(sort_ele_i_i < sort_ele_top_i);
			sort_ele_i_p =
				&(*stringpool.sort_array_pp)->array[REAL_SORT_INDEX(sort_ele_i_i, (*stringpool.sort_array_pp))];
			assert((*cstr == sort_ele_i_p->str_p)
				|| (((*cstr)->addr == sort_ele_i_p->addr))
				STP_MOVE_ONLY( || (((*cstr)->addr + move_adjustment) == sort_ele_i_p->addr)));
			tmpaddr = (unsigned char *)(*cstr)->addr;
			tmplen = (*cstr)->len;
			assert(0 < tmplen);
			if (tmpaddr + tmplen > straddr) /* if it is not a proper substring of previous one */
			{
				tmplen2 = ((tmpaddr >= straddr) ? tmplen : (ssize_t)(tmpaddr + tmplen - straddr));
				assert(0 < tmplen2);
				totspace_old += tmplen2;
				straddr = tmpaddr + tmplen;
			}
		}
		assert(sort_ele_i_i == sort_ele_top_i);
		/* Now totspace_old is the total space needed for all the current entries and any stp_move entries.
		 * Note that because of not doing exact calculation with substring, totspace_old may be little more
		 * than what is needed.
		 */
	}
	space_after_compact_old = stringpool.top - stringpool.base - totspace_old; /* can be -ve number */
	assert(space_after_compact == space_after_compact_old);
#	endif
#	ifndef STP_MOVE
	assert(space_after_compact >= space_before_compact);
#	endif
	space_reclaim = space_after_compact - space_before_compact; /* this can be -ve, if stp_move is adding strs */
	space_needed -= space_after_compact;
	DBGSTPGCOL((stderr, "space_needed=%li\n", space_needed));
	/* After compaction if less than 31.25% of space is avail, consider it a low reclaim pass */
	if (STP_LOWRECLAIM_LEVEL(stringpool.top - stringpool.base) > space_after_compact) /* BYPASSOK */
		(*low_reclaim_passes)++;
	else
		*low_reclaim_passes = 0;
	non_mandatory_expansion = FALSE;
	if (0 < space_needed /* i */
#	ifndef STP_MOVE /* do expansion only for stp_gcol, no forced expansion for stp_move */
	    || (non_mandatory_expansion =
	        (!stop_non_mandatory_expansion && /* ii */
	         (STP_MAXLOWRECLAIM_PASSES <= *low_reclaim_passes ))) /* iii */
#	endif
		)
	{	/* i   - more space needed than available so do a mandatory expansion
		 * ii  - non-mandatory expansions disabled because a previous attempt failed due to lack of memory
		 * iii - after compaction, if at least 31.25% of string pool is not free for STP_MAXLOWRECLAIM_PASSES
		 *       do a non-mandatory expansion. This is done to avoid scenarios where there is repeated
		 * 	     occurrences of the stringpool filling up, and compaction not reclaiming enough space.
		 *       In such cases, if the stringpool is expanded, we create more room, resulting in fewer calls
		 *       to stp_gcol.
		 *
		 * Note it is not uncommon for space_needed to be a negative value. That can occur in here if we
		 * recovered enough space from the GC to satisfy the immediate need but we have identified that
		 * so little space is still available that we are best served by expanding the stringpool so as to
		 * prevent another almost immediate need for another GC. This is also known as a non-mandatory
		 * expansion. Assert that if space_needed is negative, we have a non-mandatory expansion.
		 */
		assert((0 <= space_needed) || non_mandatory_expansion);
		strpool_base = stringpool.base;
		/* Grow stringpool geometrically */
		stp_incr = (stringpool.top - stringpool.base) * *incr_factor / STP_NUM_INCRS;
		do
		{
			if (stp_incr < space_needed)
				stp_incr = space_needed;
			/* If we are asking for more than is actually needed we want to try again if we do not get it. */
			retry_if_expansion_fails = (stp_incr > space_needed);
			/* If this is a non-mandatory expansion (which also means space_needed is likely negative but this is
			 * not *always* the case) and stp_incr has come down to less than the size of a page, then there's really
			 * no point in continuing this loop - probably all the way to stp_incr = 0. But for a non-mandatory
			 * expansion, leaving early carries no significiant penalty - especially since we're already operating
			 * on the the edge.
			 */
			if (non_mandatory_expansion && (0 == stp_incr))
				break;			/* stp_incr can't get smaller - give up and use what we have */
			expansion_failed = FALSE;	/* will be set to TRUE by condition handler if can't get memory */
			assert((stp_incr + stringpool.top - stringpool.base) >= (space_needed + blklen));
			DBGSTPGCOL((stderr, "incr_factor=%i stp_incr=%i space_needed=%li\n", *incr_factor, stp_incr, space_needed));
			if ((stringpool.strpllimwarned) /* previously warned */
				&& ((stp_incr + stringpool.top - stringpool.base) > stringpool.strpllim)) /* expanding larger */
			{
				assert(0 < stringpool.strpllim);	/* must have been watching stp limit */
				ENABLE_INTERRUPTS(INTRPT_IN_GCOL, prev_intrpt_state);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STPOFLOW);
			}
			DBGSTPGCOL((stderr, "base=%p top=%p (size=%llu)\n", stringpool.base, stringpool.top,
						(size_t)(stringpool.top - stringpool.base)));
			assert((ssize_t)(stringpool.top - stringpool.base)
					< ((ssize_t)(stp_incr + stringpool.top - stringpool.base)));
			prev_stp_size = stringpool.lastallocbytes;
			expand_stp((ssize_t)(stp_incr + stringpool.top - stringpool.base));
#			ifdef DEBUG
			/* If expansion failed and stp_gcol_ch did an UNWIND and we were already in exit handling code,
			 * ok_to_UNWIND_in_exit_handling would have been set to avoid an assert in UNWIND macro. Now that
			 * we are back from stp_gcol_ch, reset it so future UNWINDs in exit-handling are assert checked.
			 */
			if (expansion_failed && process_exiting)
				ok_to_UNWIND_in_exit_handling = FALSE;
#			endif
			if (!retry_if_expansion_fails || !expansion_failed)
				break;
			/* Once we hit the memory wall no more non_mandatory expansions */
			if (non_mandatory_expansion)
				stop_non_mandatory_expansion = TRUE;
			/* We were not able to get the memory we wanted. Plan B it to get as much of what we wanted
			 * (down to what was actually needed). If we can't get what was actually needed allow
			 * the user to get a memory error.
			 */
			if (1 < *incr_factor)
			{
				/* if we hit the memory wall with an elevated incr_factor drop back a notch and retry */
				*incr_factor = *incr_factor - 1;
				stp_incr = (stringpool.top - stringpool.base) * *incr_factor / STP_NUM_INCRS;
			} else
			{	/* if we are already at the lowest incr_factor half our way down */
				if (stp_incr > space_needed)
					stp_incr = stp_incr / 2;
			}
		} while (TRUE);
		if (strpool_base != stringpool.base) /* expanded successfully */
		{
			DOUBLECHECK_GCOL_ONLY(cstr = array;)
			sort_ele_i_i = 0;
			sort_ele_top_i = (*stringpool.sort_array_pp)->count;
#			ifdef STP_MOVE
			if (stp_move_count && move_adjustment)
			{
				sort_ele_top_i -= stp_move_count;
#				ifdef DOUBLECHECK_GCOL
				cstr += stp_move_count_old;
				assert(stp_move_count_old == stp_move_count);
#				endif
			}
#			endif /* STP_MOVE */
#			ifdef DOUBLECHECK_GCOL
			copy_to_stringpool_check(sort_ele_i_i, sort_ele_top_i, stp_mask, cstr, topstr);
#			else
			copy_to_stringpool(sort_ele_i_i, sort_ele_top_i, stp_mask);
#			endif
#			ifdef STP_MOVE
			if (stp_move_count && move_adjustment)
			{
				/* Need special handling to revert addr fixup
				 */
				sort_ele_top_i = (*stringpool.sort_array_pp)->count;
				assert(sort_ele_top_i >= stp_move_count);
				sort_ele_i_i = sort_ele_top_i - stp_move_count;
				for (sort_ele_j_i = sort_ele_i_i; sort_ele_j_i < sort_ele_top_i; sort_ele_j_i++)
				{
					sort_ele_j_p = &(*stringpool.sort_array_pp)
								->array[REAL_SORT_INDEX(sort_ele_j_i, (*stringpool.sort_array_pp))];
					sort_ele_j_p->addr -= move_adjustment;
					assert(sort_ele_j_p->addr == sort_ele_j_p->str_p->addr);
				}
#				ifdef DOUBLECHECK_GCOL
				cstr = array;
				tmp_topstr = cstr + stp_move_count_old;
				assert(stp_move_count_old == stp_move_count);
				copy_to_stringpool_check(sort_ele_i_i, sort_ele_top_i, stp_mask, cstr, tmp_topstr);
#				else
				copy_to_stringpool(sort_ele_i_i, sort_ele_top_i, stp_mask);
#				endif
			}
#			endif /* STP_MOVE */
			/* NOTE: rts_stringpool must be kept up-to-date because it tells whether the current
			 * stringpool is the run-time or indirection stringpool.
		 	 */
			stringpool.pending_moves = FALSE;
			if (strpool_base == rts_stringpool.base)
			{
				assert(stringpool.sort_array_pp == TADR(rts_sort_array_p));
				assert(stringpool.protect_array_pp == TADR(rts_protect_array_p));
				rts_stringpool = stringpool;
			} else
			{
				assert(strpool_base == indr_stringpool.base);
				assert(stringpool.sort_array_pp == TADR(indr_sort_array_p));
				assert(stringpool.protect_array_pp == TADR(indr_protect_array_p));
				indr_stringpool = stringpool;
			}
			assert(0 < prev_stp_size);
			stp_fini(strpool_base, prev_stp_size);
			/* Adjust incr_factor */
			if (*incr_factor < STP_NUM_INCRS)
				*incr_factor = *incr_factor + 1;
		} else
		{	/* Could not expand during forced expansion */
			assert(non_mandatory_expansion && stop_non_mandatory_expansion);
			sort_ele_i_i = 0;
			sort_ele_top_i = (*stringpool.sort_array_pp)->count;
			STP_MOVE_ONLY(assert(space_after_compact < space_needed);)
			if (space_after_compact < space_needed)
			{	/* Restore stringpool.free since no garbage collection happened. */
				stringpool.free = old_free;
				ENABLE_INTERRUPTS(INTRPT_IN_GCOL, prev_intrpt_state);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_STPEXPFAIL, 1,
					      (stp_incr + stringpool.top - stringpool.base));
			}
#			ifdef DOUBLECHECK_GCOL
			cstr = array;
			move_within_stringpool_check(sort_ele_i_i, sort_ele_top_i, stp_mask, cstr, topstr);
#			else
			move_within_stringpool(sort_ele_i_i, sort_ele_top_i, stp_mask);
#			endif
			stringpool.pending_moves = FALSE;
		}
		*low_reclaim_passes = 0;
	} else
	{
		assert(stringpool.free == stringpool.base);
		/* Adjust incr_factor */
		if (*incr_factor > 1)
			*incr_factor = *incr_factor - 1;
		DBGSTPGCOL((stderr, "incr_factor=%i space_needed=%li\n", *incr_factor, space_needed));
		DOUBLECHECK_GCOL_ONLY(STP_MOVE_ONLY(assert(stp_move_count == stp_move_count_old);))
		sort_ele_i_i = 0;
		sort_ele_top_i = sort_ele_j_i = (*stringpool.sort_array_pp)->count;
		if ((*stringpool.sort_array_pp)->count)
		{
#			ifdef STP_MOVE
			if (0 != stp_move_count)
			{	/* All stp_move elements must be contiguous in the 'array'. They point outside
				 * the range of stringpool.base and stringpool.top. In the 'array' of (mstr *) they
				 * must be either at the beginning, or at the end. In the new algorithm we guarantee they
				 * are all at the end.
				 */
				sort_ele_top_i -= stp_move_count;
				assert(!glist_str_in_stringpool((*stringpool.sort_array_pp)->array[
					REAL_SORT_INDEX(sort_ele_top_i,(*stringpool.sort_array_pp))].str_p));
#				ifdef DOUBLECHECK_GCOL
				tmpaddr = (unsigned char *)(*array)->addr;
				if (IS_PTR_IN_RANGE(tmpaddr, stringpool.base, stringpool.top))	/* BYPASSOK */
					topstr -= stp_move_count_old; /* stringpool elements before move elements in stp_array */
				else
					array += stp_move_count_old; /* stringpool elements after move elements or no stringpool
								 * elements in stp_array */
#				endif
			}
#			endif
			/* Skip over contiguous block, if any, at beginning of stringpool.
			 * Note that here we are not considering any stp_move() elements.
			 */
#			ifdef DOUBLECHECK_GCOL
			cstr = array;
			move_within_stringpool_check(sort_ele_i_i, sort_ele_top_i, stp_mask, cstr, topstr);
#			else
			move_within_stringpool(sort_ele_i_i, sort_ele_top_i, stp_mask);
#			endif
		}
#		ifdef STP_MOVE
		if (0 != stp_move_count)
		{	/* Copy stp_move elements into stringpool now */
			assert((*stringpool.sort_array_pp)->count);
			assert(sort_ele_top_i != sort_ele_j_i);
			if (move_adjustment)
			{
				for (sort_ele_i_i = sort_ele_top_i; sort_ele_i_i < sort_ele_j_i; sort_ele_i_i++)
				{
					sort_ele_i_p = &(*stringpool.sort_array_pp)
								->array[REAL_SORT_INDEX(sort_ele_i_i, (*stringpool.sort_array_pp))];
					sort_ele_i_p->addr -= move_adjustment;
					assert(sort_ele_i_p->addr == sort_ele_i_p->str_p->addr);
				}
			}
			sort_ele_i_i = sort_ele_top_i;
			sort_ele_top_i += stp_move_count;
			assert(sort_ele_top_i == sort_ele_j_i);
#			ifdef DOUBLECHECK_GCOL
			if (array == stp_array) /* stringpool elements before move elements in stp_array */
			{
				cstr = topstr;
				topstr += stp_move_count_old;
			} else
			{ /* stringpool elements after move elements OR no stringpool elements in stp_array */
				cstr = stp_array;
				topstr = array;
			}
			copy_to_stringpool_check(sort_ele_i_i, sort_ele_top_i, stp_mask, cstr, topstr);
#			else
			copy_to_stringpool(sort_ele_i_i, sort_ele_top_i, stp_mask);
#			endif
		}
#		endif
		stringpool.pending_moves = FALSE;
	}
	ENABLE_INTERRUPTS(INTRPT_IN_GCOL, prev_intrpt_state);
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	assert(!stringpool.pending_moves);
	stringpool.invokestpgcollevel = (STP_SPACE_USED_MULTIPLIER * (space_asked + stringpool.free - stringpool.base))
		+ stringpool.base;
	reserved_bytes = (ssize_t)(stringpool.top - stringpool.invokestpgcollevel);
	if (((stringpool.invokestpgcollevel - stringpool.base) < STP_GCOL_TRIGGER_FLOOR)
		|| (stringpool.invokestpgcollevel > stringpool.top))
	{
		stringpool.invokestpgcollevel = stringpool.top;
	} else if (( STP_MIN_CONTRACTION < reserved_bytes) && ((*stringpool.sort_array_pp)->count))
	{	/* Contract the stringpool */
		assert(stringpool.top > stringpool.invokestpgcollevel);
		strpool_base = stringpool.base;
		prev_stp_size = stringpool.lastallocbytes;
		stp_init((ssize_t)(stringpool.invokestpgcollevel - stringpool.base));
		assert(strpool_base != stringpool.base); /* Initiate a smaller space successfully */
		cstr = array;
		sort_ele_i_i = 0;
		sort_ele_top_i = (*stringpool.sort_array_pp)->count;
#		ifdef DOUBLECHECK_GCOL
		copy_to_stringpool_check(sort_ele_i_i, sort_ele_top_i, stp_mask, cstr, topstr);
#		else
		copy_to_stringpool(sort_ele_i_i, sort_ele_top_i, stp_mask);
#		endif
		assert(!stringpool.pending_moves);
		if (strpool_base == rts_stringpool.base)
		{
			assert(stringpool.sort_array_pp == TADR(rts_sort_array_p));
			assert(stringpool.protect_array_pp == TADR(rts_protect_array_p));
			rts_stringpool = stringpool;
		} else
		{
			assert(strpool_base == indr_stringpool.base);
			assert(stringpool.sort_array_pp == TADR(indr_sort_array_p));
			assert(stringpool.protect_array_pp == TADR(indr_protect_array_p));
			indr_stringpool = stringpool;
		}
		assert(0 < prev_stp_size);
		stp_fini(strpool_base, prev_stp_size);
	}
	assert(stringpool.invokestpgcollevel <= stringpool.top);
#	ifndef STP_MOVE
	assert(stringpool.top - stringpool.free >= space_asked);
	if (IS_LVMON_ACTIVE && (stringpool.base == rts_stringpool.base))
	{	/* Do monitoring only for runtime stringpool */
		lvmon_pull_values(2);				/* Pull values of monitored vars into index 2 */
		lvmon_compare_value_slots(1, 2);		/* Make sure they are the same */
	}
	if ((0 < stringpool.strpllim && !stringpool.strpllimwarned) /* monitoring stp limit and no prior warned */
		&& ((stringpool.top - stringpool.base) > stringpool.strpllim)) /* past the stp limit */
	{
		stringpool.strpllimwarned =  TRUE;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STPCRIT);
	}
	#	endif	/* !STP_MOVE */
	return;
}
