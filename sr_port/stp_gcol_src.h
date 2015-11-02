/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "fnpc.h"
#include "gdscc.h"
#include "cache.h"
#include "comline.h"
#include "compiler.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "io.h"
#include "jnl.h"
#include "lv_val.h"
#include "subscript.h"
#include "mdq.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "sbs_blk.h"
#include "stack_frame.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "mlkdef.h"
#include "zbreak.h"
#include "zshow.h"
#include "zwrite.h"
#include "error.h"
#include "longcpy.h"
#include "stpg_sort.h"
#include "hashtab_objcode.h"
#include "hashtab.h"

#ifndef STP_MOVE
GBLDEF symval		*first_symval;
#endif

GBLREF mvar 		*mvartab;
GBLREF mlabel 		*mlabtab;
GBLREF int 		mvmax;
GBLREF int		mlmax;
GBLREF int 		mvar_index;
GBLREF hash_table_objcode cache_table;
GBLREF bool		compile_time;
GBLREF unsigned char	*msp, *stackbase, *stacktop, *stackwarn;
GBLREF int		stp_array_size;
GBLREF gvzwrite_struct	gvzwrite_block;
GBLREF io_log_name	*io_root_log_name;
GBLREF lvzwrite_struct	lvzwrite_block;
GBLREF mliteral		literal_chain;
GBLREF mstr		*comline_base, dollar_zsource, **stp_array;
GBLREF mval		dollar_etrap, dollar_system, dollar_zerror, dollar_zgbldir, dollar_zstatus, dollar_zstep, dollar_ztrap;
GBLREF mval		dollar_zyerror, zstep_action, dollar_zinterrupt, dollar_ztexit;
GBLREF mv_stent		*mv_chain;
GBLREF sgm_info		*first_sgm_info;
GBLREF spdesc		indr_stringpool, rts_stringpool, stringpool;
GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;
GBLREF tp_frame		*tp_pointer;
GBLREF boolean_t	disallow_forced_expansion, forced_expansion;
GBLREF mval		last_fnquery_return_varname;			/* Return value of last $QUERY (on stringpool) (varname) */
GBLREF mval		last_fnquery_return_sub[MAX_LVSUBSCRIPTS];	/* .. (subscripts) */
GBLREF int		last_fnquery_return_subcnt;			/* .. (count of subscripts) */
GBLREF boolean_t	mstr_native_align;
GBLREF fnpc_area	fnpca;
OS_PAGE_SIZE_DECLARE

static mstr		**topstr, **array, **arraytop;

#ifdef STP_MOVE

#define MV_STPG_GET(x) \
	((((x)->mvtype & MV_STR) && (x)->str.len) ? \
		(((x)->str.addr >= (char *)stringpool.base && (x)->str.addr < (char *)stringpool.free) ? &((x)->str) \
		: (((x)->str.addr >= from && (x)->str.addr < to) ? move_count++, &((x)->str) : NULL))\
	: NULL)

#define STR_STPG_GET(x) \
	(((x)->len) ? \
	  	(((x)->addr >= (char *)stringpool.base && (x)->addr < (char *)stringpool.free) ? (x) \
	 	: (((x)->addr >= from && (x)->addr < to) ? move_count++, (x) : NULL)) \
	: NULL)

#else /* !STP_MOVE <=> STP_GCOL */

#define MV_STPG_GET(x) \
	((((x)->mvtype & MV_STR) && (x)->str.len && \
	(x)->str.addr >= (char *)stringpool.base && (x)->str.addr < (char *)stringpool.free) \
	? &((x)->str) : NULL)

#define STR_STPG_GET(x) (((x)->len && (x)->addr >= (char *)stringpool.base && (x)->addr < (char *)stringpool.free) ? (x) : NULL)

#endif /* end if STP_MOVE ... else ... */

#if defined(DEBUG) && !defined(STP_MOVE)
#  define MV_STPG_PUT(X) stp_gcol_mv_stpg_put(X) /* STP_GCOL only -- only updates statics in stp_gcol() */
#else
#  define MV_STPG_PUT(X) \
	  (*topstr++ = (X), topstr >= arraytop ?  \
           (stp_put_int = (int)(topstr - array), stp_expand_array(), array = stp_array, \
           topstr = array + stp_put_int, arraytop = array + stp_array_size) : 0)
#endif

#define PROCESS_CACHE_ENTRY(cp)								\
        if (cp->src.str.len)	/* entry is used */					\
	{										\
		x = STR_STPG_GET(&cp->src.str);						\
		if (x)									\
			MV_STPG_PUT(x);							\
		/* Run list of mvals for each code stream that exists */		\
		if (cp->obj.len)							\
		{									\
			ihdr = (ihdtyp *)cp->obj.addr;					\
			fixup_cnt = ihdr->fixup_vals_num;				\
			if (fixup_cnt)							\
			{								\
				m = (mval *)((char *)ihdr + ihdr->fixup_vals_off);	\
				for (mtop = m + fixup_cnt;  m < mtop; m++)		\
				{							\
					x = MV_STPG_GET(m);				\
					if (x)						\
						MV_STPG_PUT(x);				\
				}							\
			}								\
			fixup_cnt = ihdr->vartab_len;					\
			if (fixup_cnt)							\
			{								\
				vent = (var_tabent *)((char *)ihdr + ihdr->vartab_off);	\
				for (vartop = vent + fixup_cnt;  vent < vartop; vent++)\
				{							\
					x = STR_STPG_GET(&vent->var_name);		\
					if (x)						\
						MV_STPG_PUT(x);				\
				}							\
			}								\
		}									\
	}

#define PROCESS_CONTIGUOUS_BLOCK(begaddr, endaddr, cstr, delta) \
{ \
	padlen = 0; \
	for (; cstr < topstr; cstr++) \
	{ \
		assert((*cstr)->addr >= (char *)begaddr); \
		if ((*cstr)->addr > (char *)endaddr && (*cstr)->addr != (char *)endaddr + padlen) \
			break; \
		tmpaddr = (unsigned char *)(*cstr)->addr + (*cstr)->len; \
		if (tmpaddr > endaddr) \
			endaddr = tmpaddr;\
		padlen = mstr_native_align ? PADLEN((*cstr)->len, NATIVE_WSIZE) : 0;\
		(*cstr)->addr -= delta; \
	} \
}

#define COPY2STPOOL(cstr, topstr) \
{ \
	while (cstr < topstr) \
	{ \
		if (mstr_native_align) \
			stringpool.free = (unsigned char *)ROUND_UP2((INTPTR_T)stringpool.free, NATIVE_WSIZE); \
		/* Determine extent of next contiguous block and copy it into new stringpool.base. */ \
		begaddr = endaddr = (unsigned char *)((*cstr)->addr); \
		delta = (*cstr)->addr - (char *)stringpool.free; \
		PROCESS_CONTIGUOUS_BLOCK(begaddr, endaddr, cstr, delta); \
		blklen = endaddr - begaddr; \
		memcpy(stringpool.free, begaddr, blklen); \
		stringpool.free += blklen; \
	} \
}

static void expand_stp(unsigned int new_size)
{
	ESTABLISH(stp_gcol_ch);
	stp_init(new_size);
	REVERT;
	return;
}


DEBUG_ONLY(mstr **stp_gcol_mv_stpg_put(mstr *X);)
#ifndef STP_MOVE
#ifdef DEBUG
/* MV_STP_PUT macro expansion as routine so we can add asserts without issues (and see vars in core easier) */
mstr **stp_gcol_mv_stpg_put(mstr *X)
{
	int	stp_put_int;

	assert(0 < (X)->len); /* It would be nice to test for maxlen here but that causes some usages of stringpool
				 to fail as other types of stuff are built into the stringppool besides strings. */
	*topstr++ = (X);
	return (topstr >= arraytop ? (stp_put_int = (int)(topstr - array), stp_expand_array(), array = stp_array,
				      topstr = array + stp_put_int, arraytop = array + stp_array_size) : 0);
}

/* Verify the saved symbol table that we will be processing later in stp_gcol(). This version is callable from anywhere
   and is a great debuging tool for corrupted mstrs in local variable trees. Uncomment the call near the top of stp_gcol to
   verify it will catch what you want it to catch and sprinkle calls around other places as necessary. Be warned, this can
   *really* slow things down so use judiciously. SE 10/2007
*/
void stp_vfy_mval(void)
{
	lv_blk			*lv_blk_ptr;
	lv_val			*lvp, *lvlimit;
	mstr			*x;

	if (!first_symval)
	{	/* Must be setting up first symbol table now */
		first_symval = (symval *)1;	/* Only allow first time, else this will cause sig-10/11 next time through */
		return;
	}
	for (lv_blk_ptr = &first_symval->first_block;
	     lv_blk_ptr;
	     lv_blk_ptr = lv_blk_ptr->next)
	{
		for (lvp = lv_blk_ptr->lv_base, lvlimit = lv_blk_ptr->lv_free;
		     lvp < lvlimit;  lvp++)
		{
			if (lvp->v.mvtype != MV_SBS)
			{
				x = MV_STPG_GET(&(lvp->v));
				if (x)
				{
					assert(0 < (x)->len);
				}
			}
		}
	}
}
#endif  /* DEBUG */

void mv_parse_tree_collect(mvar *node);
void mv_parse_tree_collect(mvar *node)
{
	mstr		*string;
	int		stp_put_int;

	string = (mstr *)STR_STPG_GET(&node->mvname);
	if (string)
		MV_STPG_PUT(string);
	if (node->lson)
		mv_parse_tree_collect(node->lson);
	if (node->rson)
		mv_parse_tree_collect(node->rson);
}
#endif /* #ifndef STP_MOVE */


#ifdef STP_MOVE
void stp_move(char *from, char *to) /* garbage collect and move range (from,to] to stringpool adjusting all mvals/mstrs pointing
				     * in this range */
#else
void stp_gcol(int space_asked) /* garbage collect and create enough space for space_asked bytes */
#endif
{
#ifdef STP_MOVE
	int			space_asked = 0, move_count = 0;
#endif
	unsigned char		*strpool_base, *straddr, *tmpaddr, *begaddr, *endaddr;
	int			index, space_needed, fixup_cnt, tmplen, totspace;
	long			space_before_compact, space_after_compact, blklen, delta, space_reclaim, padlen;
	int			stp_put_int;
	io_log_name		*l;		/* logical name pointer		*/
	lv_blk			*lv_blk_ptr;
	lv_sbs_tbl		*tbl;
	lv_val			*lvp, *lvlimit;
	mstr			**cstr, *x;
	mv_stent		*mvs;
	mval			*m, *mtop;
	sbs_blk			*blk;
	sbs_str_struct		*s_sbs;
	sgm_info		*si;
	stack_frame		*sf;
	tp_frame		*tf;
	jnl_format_buffer	*jfb;
	zwr_sub_lst		*zwr_sub;
	ihdtyp			*ihdr;
	zbrk_struct		*z_ptr;
	static int		indr_stp_low_reclaim_passes = 0;
	static int		rts_stp_low_reclaim_passes = 0;
	int			*low_reclaim_passes;
	static int		indr_stp_spc_needed_passes = 0;
	static int		rts_stp_spc_needed_passes = 0;
	int			*spc_needed_passes;
	static int		indr_stp_incr_factor = 1;
	static int		rts_stp_incr_factor = 1;
	int			*incr_factor;
	static int		indr_stp_maxnoexp_passes = STP_INITMAXNOEXP_PASSES;
	static int		rts_stp_maxnoexp_passes = STP_INITMAXNOEXP_PASSES;
	int			*maxnoexp_passes;
	int			stp_incr;
	boolean_t		maxnoexp_growth;
	ht_ent_objcode 		*tabent_objcode, *topent;
	ht_ent_mname		*tabent_mname, *topent_mname;
	cache_entry		*cp;
	var_tabent		*vent, *vartop;
	symval			*symtab;
	error_def		(ERR_STPEXPFAIL);

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	assert(stringpool.top - stringpool.free < space_asked || space_asked == 0);
        assert(CHK_BOUNDARY_ALIGNMENT(stringpool.top) == 0);
#ifdef STP_MOVE
	assert(from < to); /* why did we call with zero length range, or a bad range? */
	assert((from <  (char *)stringpool.base && to <  (char *)stringpool.base) || /* range to be moved should not intersect */
	       (from >= (char *)stringpool.top  && to >= (char *)stringpool.top));   /* with stringpool range */
#else
	/* stp_vfy_mval(); / * uncomment to debug lv corruption issues.. */
#endif

	space_needed = ROUND_UP2(space_asked, NATIVE_WSIZE);
	assert(0 == (INTPTR_T)stringpool.base % NATIVE_WSIZE);
	if (stringpool.base == rts_stringpool.base)
	{
		low_reclaim_passes = &rts_stp_low_reclaim_passes;
		spc_needed_passes = &rts_stp_spc_needed_passes;
		incr_factor = &rts_stp_incr_factor;
		maxnoexp_passes = &rts_stp_maxnoexp_passes;
	} else if (stringpool.base == indr_stringpool.base)
	{
		low_reclaim_passes = &indr_stp_low_reclaim_passes;
		spc_needed_passes = &indr_stp_spc_needed_passes;
		incr_factor = &indr_stp_incr_factor;
		maxnoexp_passes = &indr_stp_maxnoexp_passes;
	} else
	{
		GTMASSERT; /* neither rts_stringpool, nor indr_stringpool */
	}

	if (stp_array == 0)
		stp_array = (mstr **)malloc((stp_array_size = STP_MAXITEMS) * sizeof(mstr *));

	topstr = array = stp_array;
	arraytop = topstr + stp_array_size;

	/* if dqloop == 0 then we got here from mcompile
	 * if literal_chain.que.fl=0 then put_lit was never done
	 * as is true in gtcm_server. Test for cache_table.size is to
	 * check that we have not done a cache_init() which would be true
	 * if doing mumps standalone compile.
	 */
	if (((stringpool.base != rts_stringpool.base) || (0 == cache_table.size)))
	{
#ifndef STP_MOVE
		mliteral *p;
		if (literal_chain.que.fl != 0)
		{
			dqloop(&literal_chain, que, p)
			{
				x = MV_STPG_GET(&(p->v));
				if (x)
					MV_STPG_PUT(x);
			}
		}
		assert(offsetof(mvar, mvname) == offsetof(mlabel, mvname));
		if (NULL != mvartab)
			mv_parse_tree_collect(mvartab);
		if (NULL != mlabtab)
			mv_parse_tree_collect((mvar *)mlabtab);
#endif
	} else
	{
		/* Some house keeping since we are garbage collecting. Clear out all the lookaside
		   arrays for the simple $piece function if this isn't a vax (no lookaside on vax). */
#ifndef __vax
#  ifdef DEBUG
		GBLREF int c_clear;
		++c_clear;		/* Count clearing operations */
#  endif
		for (index = 0; FNPC_MAX > index; index++)
		{
			fnpca.fnpcs[index].last_str.addr = NULL;
			fnpca.fnpcs[index].last_str.len = 0;
			fnpca.fnpcs[index].delim = 0;
		}
#endif
		assert(0 != cache_table.size);	/* Must have done a cache_init() */
		/* These cache entries have mvals in them we need to keep */
		for (tabent_objcode = cache_table.base, topent = cache_table.top; tabent_objcode < topent; tabent_objcode++)
		{
			if (HTENT_VALID_OBJCODE(tabent_objcode, cache_entry, cp))
			{
				x = (mstr *)STR_STPG_GET(&(tabent_objcode->key.str));
				if (x)
					MV_STPG_PUT(x);
				PROCESS_CACHE_ENTRY(cp);
			}
		}

		for (symtab = curr_symval; NULL != symtab; symtab = symtab->last_tab)
		{
			for (tabent_mname = symtab->h_symtab.base, topent_mname = symtab->h_symtab.top;
					tabent_mname < topent_mname; tabent_mname++)
			{
				if (HTENT_VALID_MNAME(tabent_mname, lv_val, lvp))
				{
					x = (mstr *)STR_STPG_GET(&(tabent_mname->key.var_name));
					if (x)
						MV_STPG_PUT(x);
				}
			}
		}

		if (x = comline_base)
		{
			for (index = MAX_RECALL;  index > 0 && x->len;  index--, x++)
				MV_STPG_PUT(x);
		}

		if (lvzwrite_block.curr_subsc)
		{
			assert(lvzwrite_block.sub);
			zwr_sub = (zwr_sub_lst *)lvzwrite_block.sub;
			for (index = 0;  index < (int)lvzwrite_block.curr_subsc;  index++)
			{
				assert(zwr_sub->subsc_list[index].actual != zwr_sub->subsc_list[index].first
					|| !zwr_sub->subsc_list[index].second);
				/*
				 * we cannot garbage collect duplicate mval pointers.
				 * So make sure zwr_sub->subsc_list[index].actual is not pointing to an
				 * existing (mval *) which  is already protected
				 */
				if (zwr_sub->subsc_list[index].actual &&
					zwr_sub->subsc_list[index].actual != zwr_sub->subsc_list[index].first)
				{
					x = MV_STPG_GET(zwr_sub->subsc_list[index].actual);
					if (x)
						MV_STPG_PUT(x);
				}
			}
		}
		for (l = io_root_log_name;  0 != l;  l = l->next)
		{
			if ((IO_ESC != l->dollar_io[0]) && (l->iod->trans_name == l))
			{
				x = STR_STPG_GET(&l->iod->error_handler);
				if (x)
					MV_STPG_PUT(x);
			}
		}
		x = MV_STPG_GET(&dollar_etrap);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&dollar_system);
		if (x)
			MV_STPG_PUT(x);
		x = STR_STPG_GET(&dollar_zsource);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&dollar_ztrap);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&dollar_zstatus);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&dollar_zgbldir);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&dollar_zinterrupt);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&dollar_zstep);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&zstep_action);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&dollar_zerror);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&dollar_ztexit);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&dollar_zyerror);
		if (x)
			MV_STPG_PUT(x);
		x = MV_STPG_GET(&last_fnquery_return_varname);
		if (x)
			MV_STPG_PUT(x);
		for (index = 0; index < last_fnquery_return_subcnt; index++);
		{
			x = MV_STPG_GET(&last_fnquery_return_sub[index]);
			if (x)
				MV_STPG_PUT(x);
		}
		for (mvs = mv_chain;  mvs < (mv_stent *)stackbase;  mvs = (mv_stent *)((char *)mvs + mvs->mv_st_next))
		{
			switch (mvs->mv_st_type)
			{
			case MVST_MVAL:
				m = &mvs->mv_st_cont.mvs_mval;
				break;
			case MVST_MSAV:
				m = &mvs->mv_st_cont.mvs_msav.v;
				break;
			case MVST_STAB:
				if (mvs->mv_st_cont.mvs_stab)
				{	/* if initalization of the table was successful */
					for (lv_blk_ptr = &mvs->mv_st_cont.mvs_stab->first_block;
						lv_blk_ptr;
						lv_blk_ptr = lv_blk_ptr->next)
					{
						for (lvp = lv_blk_ptr->lv_base, lvlimit = lv_blk_ptr->lv_free;
							lvp < lvlimit;  lvp++)
						{
							if ((lvp->v.mvtype == MV_SBS)
								&& (tbl = (lv_sbs_tbl *)lvp)
								&& (blk = tbl->str))
							{
								assert(tbl->ident == MV_SBS);
								for (;  blk;  blk = blk->nxt)
								{
									for (s_sbs = &blk->ptr.sbs_str[0],
									    straddr = (unsigned char *)&blk->ptr.sbs_str[blk->cnt];
									    s_sbs < (sbs_str_struct *)straddr; s_sbs++)
									{
										x = STR_STPG_GET(&s_sbs->str);
										if (x)
											MV_STPG_PUT(x);
									}
								}
							} else
							{
								x = MV_STPG_GET(&(lvp->v));
								if (x)
									MV_STPG_PUT(x);
							}
						}
					}
				}
				continue;
			case MVST_IARR:
				m = (mval *)mvs->mv_st_cont.mvs_iarr.iarr_base;
				for (mtop = m + mvs->mv_st_cont.mvs_iarr.iarr_mvals;  m < mtop;  m++)
				{
					x = MV_STPG_GET(m);
					if (x)
						MV_STPG_PUT(x);
				}
				continue;
			case MVST_NTAB:
			case MVST_PARM:
			case MVST_TVAL:
			case MVST_STCK:
			case MVST_PVAL:
			case MVST_TPHOLD:
				continue;
			case MVST_NVAL:
				x = (mstr *)STR_STPG_GET(&mvs->mv_st_cont.mvs_nval.name.var_name);
				if (x)
					MV_STPG_PUT(x);
				continue;
			case MVST_ZINTR:
				m = &mvs->mv_st_cont.mvs_zintr.savtarg;
				break;
			case MVST_ZINTDEV:
				if (mvs->mv_st_cont.mvs_zintdev.buffer_valid)
				{
					x = (mstr *)STR_STPG_GET(&mvs->mv_st_cont.mvs_zintdev.curr_sp_buffer);
					if (x)
						MV_STPG_PUT(x);
				}
				continue;
			default:
				GTMASSERT;
			}
			x = MV_STPG_GET(m);
			if (x)
				MV_STPG_PUT(x);
		}
		for (sf = frame_pointer;  sf < (stack_frame *)stackbase && sf->old_frame_pointer;  sf = sf->old_frame_pointer)
		{	/* Cover temp mvals in use */
			assert(sf->temps_ptr);
			if (sf->temps_ptr >= (unsigned char *)sf)
				continue;
			m = (mval *)sf->temps_ptr;
			for (mtop = m + sf->temp_mvals;  m < mtop;  m++)
			{
				x = MV_STPG_GET(m);
				if (x)
				{
					/* we don't expect DM frames to have temps */
					assert(!(sf->type & SFT_DM));
					MV_STPG_PUT(x);
				}
			}
		}

		if (tp_pointer)
		{
			tf = tp_pointer;
			while(tf)
			{
				x = MV_STPG_GET(&tf->trans_id);
				if (x)
					MV_STPG_PUT(x);
				x = MV_STPG_GET(&tf->zgbldir);
				if (x)
					MV_STPG_PUT(x);
				tf = tf->old_tp_frame;
			}
		}
	}
	space_before_compact = stringpool.top - stringpool.free; /* Available space before compaction */
	if (topstr != array)
	{
		stpg_sort(array, topstr - 1);
		for (totspace = 0, cstr = array, straddr = (unsigned char *)(*cstr)->addr; (cstr < topstr); cstr++ )
		{
			assert(cstr == array || (*cstr)->addr >= ((*(cstr - 1))->addr));
			tmpaddr = (unsigned char *)(*cstr)->addr;
			tmplen = (*cstr)->len;
			assert(0 < tmplen);
			if (tmpaddr + tmplen > straddr) /* if it is not a proper substring of previous one */
			{
				int tmplen2;
				tmplen2 = ((tmpaddr >= straddr) ? tmplen : (tmpaddr + tmplen - straddr));
				assert(0 < tmplen2);
				totspace += tmplen2;
				if (mstr_native_align)
					totspace += PADLEN(totspace, NATIVE_WSIZE);
				straddr = tmpaddr + tmplen;
			}
		}
		/* Now totspace is the total space needed for all the current entries and any stp_move entries.
		 * Note that because of not doing exact calculation with substring,
		 * totspace may be little more than what is needed */
		space_after_compact = stringpool.top - stringpool.base - totspace; /* can be -ve number */
	} else
		space_after_compact = stringpool.top - stringpool.free;
#ifndef STP_MOVE
	assert(mstr_native_align || space_after_compact >= space_before_compact);
#endif
	space_reclaim = space_after_compact - space_before_compact; /* this can be -ve, if alignment causes expansion */
	space_needed -= space_after_compact;
	if (0 < space_needed && STP_RECLAIMLIMIT > space_reclaim)
	{
		(*spc_needed_passes)++;
		if (STP_MINRECLAIM > space_reclaim)
			(*low_reclaim_passes)++;
		else
		{
			*low_reclaim_passes = 0;
			if (STP_ENOUGHRECLAIMED <= space_reclaim)
				*spc_needed_passes = 0;
		}
	}
	forced_expansion = maxnoexp_growth = FALSE;
	if (0 < space_needed || /* i */
	    STP_MINFREE > space_after_compact /* ii */
#ifndef STP_MOVE /* do forced expansion only for stp_gcol, no forced expansion for stp_move */
	    || (forced_expansion =
	        (!disallow_forced_expansion && /* iii */
	         STP_LIMITFRCDEXPN >= stringpool.top - stringpool.base && /* iv */
	         (STP_MAXLOWRECLAIM_PASSES <= *low_reclaim_passes || /* v */
	         (maxnoexp_growth = (*maxnoexp_passes < STP_MAXNOEXP_PASSES && *spc_needed_passes >= *maxnoexp_passes))))) /* vi */
#endif
	     )
	{
		/* i   - more space needed than available
		 * ii  - very less space available
		 * iii - all cases of forced expansions disabled 'cos a previous attempt at forced expansion failed due to lack of
		 * 	 memory
		 * iv  - no forced expansions after the size of the stringpool crosses a threshold
		 * v   - not much reclaimed the past low_reclaim_passes, stringpool is relatively compact, time to grow
		 * vi  - stringpool was found lacking in space maxnoexp_passes times and we reclaimed only between STP_MINFREE
		 * 	 and STP_ENOUGHRECLAIMED, time to grow. The motivation is to avoid scenarios where there is repeated
		 * 	 occurrences of the stringpool filling up, and compaction reclaiming enough space. In such cases,
		 *       if the stringpool is expanded, we create more room, resulting in fewer calls to stp_gcol.
		 */
		strpool_base = stringpool.base;
		blklen = stringpool.free - stringpool.base;
		if ((stringpool.top - stringpool.base) < STP_MAXGEOMGROWTH)
		{
			stp_incr = STP_GEOM_INCREMENT * (*incr_factor);
			*incr_factor *= STP_GEOMGROWTH_FACTOR;
		} else
			stp_incr = STP_LINEAR_INCREMENT;
		stp_incr = ROUND_UP(stp_incr, OS_PAGE_SIZE);
		if (stp_incr < space_needed)
			stp_incr = ROUND_UP(space_needed, OS_PAGE_SIZE);
		assert(stp_incr + stringpool.top - stringpool.base >= space_needed + blklen);
		expand_stp((unsigned int)(stp_incr + stringpool.top - stringpool.base));
		if (strpool_base != stringpool.base) /* expanded successfully */
		{
			cstr = array;
			COPY2STPOOL(cstr, topstr);
			/*
		 	 * NOTE: rts_stringpool must be kept up-to-date because it tells whether the current
			 * stringpool is the run-time or indirection stringpool.
		 	 */
			(strpool_base == rts_stringpool.base) ? (rts_stringpool = stringpool) : (indr_stringpool = stringpool);
			free(strpool_base);
		} else
		{
			/* could not expand during forced expansion */
			assert(forced_expansion && disallow_forced_expansion);
			if (space_after_compact < space_needed)
				rts_error(VARLSTCNT(3) ERR_STPEXPFAIL, 1, stp_incr + stringpool.top - stringpool.base);
		}
		*spc_needed_passes = 0;
		if (maxnoexp_growth && (*maxnoexp_passes < STP_MAXNOEXP_PASSES))
			*maxnoexp_passes += STP_NOEXP_PASSES_INCR;
		*low_reclaim_passes = 0;
	} else
	{
		stringpool.free = stringpool.base;
		if (topstr != array)
		{
#ifdef STP_MOVE
			if (0 != move_count)
			{	/* All stp_move elements must be contiguous in the 'array'. They point outside
				 * the range of stringpool.base and stringpool.top. In the 'array' of (mstr *) they
				 * must be either at the beginning, or at the end. */
				if ((*array)->addr >= (char *)stringpool.base && (*array)->addr < (char *)stringpool.top)
					topstr -= move_count; /* stringpool elements before move elements in stp_array */
				else
					array += move_count; /* stringpool elements after move elements or no stringpool elements in
							      * stp_array */
			}
#endif
			/* Skip over contiguous block, if any, at beginning of stringpool.
			 * Note that here we are not considering any stp_move() elements.  */
			cstr = array;
			begaddr = endaddr = (unsigned char *)((*cstr)->addr);
			while (cstr < topstr)
			{
				/* Determine extent of next contiguous block to move and move it. */
				if (mstr_native_align)
					stringpool.free = (unsigned char *)ROUND_UP2((INTPTR_T)stringpool.free, NATIVE_WSIZE);
				begaddr = endaddr = (unsigned char *)((*cstr)->addr);
				delta = (*cstr)->addr - (char *)stringpool.free;
				PROCESS_CONTIGUOUS_BLOCK(begaddr, endaddr, cstr, delta);
				blklen = endaddr - begaddr;
				if (delta)
					memmove(stringpool.free, begaddr, blklen);
				stringpool.free += blklen;
			}
		}
#ifdef STP_MOVE
		if (0 != move_count)
		{	/* Copy stp_move elements into stringpool now */
			assert(topstr == cstr); /* all stringpool elements garbage collected */
			if (array == stp_array) /* stringpool elements before move elements in stp_array */
				topstr += move_count;
			else
			{ /* stringpool elements after move elements OR no stringpool elements in stp_array */
				cstr = stp_array;
				topstr = array;
			}
			COPY2STPOOL(cstr, topstr);
		}
#endif
	}
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
#ifndef STP_MOVE
	assert(stringpool.top - stringpool.free >= space_asked);
#endif
	return;
}
