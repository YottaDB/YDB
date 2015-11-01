/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "hashdef.h"
#include "hashtab.h"		/* needed also for tp.h */
#include "io.h"
#include "jnl.h"
#include "lv_val.h"
#include "mdq.h"
#include "mv_stent.h"
#include "rtnhdr.h"
#include "sbs_blk.h"
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
#include "longcpy.h"
#include "stpg_sort.h"

/* Undefine memmove since the Vax shortcut defines a 64K max move which we can
   easily exceed with the stringpool moves */
#ifdef __vax
#undef memmove
#endif

GBLREF bool		compile_time, run_time;
GBLREF unsigned char	*msp, *stackbase, *stacktop, *stackwarn;
GBLREF int		stp_array_size;
GBLREF cache_entry	*cache_entry_base, *cache_entry_top;
GBLREF int4		cache_fails;
GBLREF gvzwrite_struct	gvzwrite_block;
GBLREF io_log_name	*io_root_log_name;
GBLREF hashtab		*stp_duptbl;
GBLREF lvzwrite_struct	lvzwrite_block;
GBLREF mliteral		literal_chain;
GBLREF mstr		*comline_base, dollar_zsource, **stp_array;
GBLREF mval		dollar_etrap, dollar_system, dollar_zerror, dollar_zgbldir, dollar_zstatus, dollar_zstep, dollar_ztrap;
GBLREF mval		dollar_zyerror, zstep_action, dollar_zinterrupt;
GBLREF mv_stent		*mv_chain;
GBLREF sgm_info		*first_sgm_info;
GBLREF spdesc		indr_stringpool, rts_stringpool, stringpool;
GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;
GBLREF tp_frame		*tp_pointer;
GBLREF boolean_t        lv_dupcheck;
GBLREF z_records	zbrk_recs;
#ifndef __vax
GBLREF fnpc_area	fnpca;
#endif
OS_PAGE_SIZE_DECLARE

static boolean_t	disallow_forced_expansion = FALSE, forced_expansion = FALSE;

#define MV_STPG_GET(x) \
	((((x)->mvtype & MV_STR) && (x)->str.len && \
	(x)->str.addr >= (char *)stringpool.base && (x)->str.addr < (char *)stringpool.free) \
	? &((x)->str) : 0)

#define MV_STPG_PUT(X) \
	(((lv_dupcheck && !add_hashtab_ent(&stp_duptbl, HASH_KEY_INVALID, (void *)X)) ? 0 : (*a++ = (X), a >= arraytop ? \
        (stp_put_int = a - array, stp_expand_array(), array = stp_array, \
        a = array + stp_put_int, arraytop = array + stp_array_size) : 0)))

#define STR_STPG_GET(x) \
	(((x)->len && \
	(x)->addr >= (char *)stringpool.base && (x)->addr < (char *)stringpool.free) \
	? (x) : 0)

#define PROCESS_CACHE_ENTRY(cp)								\
        if (cp->src.len)	/* entry is used */					\
	{										\
		x = STR_STPG_GET(&cp->src);						\
		if (x)									\
			MV_STPG_PUT(x);							\
		/* Run list of mvals for each code stream that exists */		\
		if (cp->obj.len)							\
		{									\
			ihdr = (ihdtyp *)cp->obj.addr;					\
			fixup_cnt = ihdr->fixup_vals_num;				\
			if (fixup_cnt)							\
			{								\
				m = (mval *)((char *)ihdr + ihdr->fixup_vals_ptr);	\
				for (mtop = m + fixup_cnt;  m < mtop;  m++)		\
				{							\
					x = MV_STPG_GET(m);				\
					if (x)						\
						MV_STPG_PUT(x);				\
				}							\
			}								\
		}									\
	}

CONDITION_HANDLER(stp_gcol_ch)
{
	/* If we cannot alloc memory while doing a forced expansion, disable all cases of forced expansion henceforth */
	error_def(ERR_MEMORY);
	error_def(ERR_VMSMEMORY);
	error_def(ERR_MEMORYRECURSIVE);

	START_CH;
	if ((ERR_MEMORY == SIGNAL || ERR_VMSMEMORY == SIGNAL || ERR_MEMORYRECURSIVE == SIGNAL) && forced_expansion)
	{
		disallow_forced_expansion = TRUE;
		UNWIND(NULL, NULL);
	}
	NEXTCH; /* we really need to expand, and there is no memory available, OR, non memory related error */
}

static void expand_stp(unsigned int new_size)
{
	ESTABLISH(stp_gcol_ch);
	stp_init(new_size);
	REVERT;
	return;
}

void stp_gcol(int space_needed)
{
	unsigned char		*e, *e1, i, *s, *top;
	int			delta, n, n1, stp_put_int, fixup_cnt;
	cache_entry		*cp;
	io_log_name		*l;		/* logical name pointer		*/
	lv_blk			*lv_blk_ptr;
	lv_sbs_tbl		*tbl;
	lv_val			*lvp, *lvlimit;
	mstr			**a, **array, **arraytop, **b, *x;
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
	int			stp_incr, space_reclaimed;
	boolean_t		maxnoexp_growth;

	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	assert(stringpool.top - stringpool.free < space_needed || space_needed == 0);
	assert(((unsigned)stringpool.top & 0x00000003) == 0);

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

	if (lv_dupcheck)
		init_hashtab(&stp_duptbl, stp_array_size);
	a = array = stp_array;
	arraytop = a + stp_array_size;

	/* if dqloop == 0 then we got here from mcompile
	 * if literal_chain.que.fl=0 then put_lit was never done
	 * as is true in gtcm_server. Test for cache_entry_base is to
	 * check that we have not done a cache_init() which would be true
	 * if doing mumps standalone compile.
	 */
	if (((stringpool.base != rts_stringpool.base) || (NULL == cache_entry_base)) &&
		(literal_chain.que.fl !=0))
	{
		mliteral *p;
		dqloop(&literal_chain, que, p)
		{
			x = MV_STPG_GET(&(p->v));
			if (x)
				MV_STPG_PUT(x);
		}
	} else
	{
		/* Some house keeping since we are garbage collecting. Clear out all the lookaside
		   arrays for the simple $piece function if this isn't a vax (no lookaside on vax). */
#ifndef __vax
#  ifdef DEBUG
		GBLREF int c_clear;
		++c_clear;		/* Count clearing operations */
#  endif
		for (n = 0; FNPC_MAX > n; n++)
		{
			fnpca.fnpcs[n].last_str.addr = NULL;
			fnpca.fnpcs[n].last_str.len = 0;
			fnpca.fnpcs[n].delim = 0;
		}
#endif
		assert(cache_entry_base != NULL);	/* Must have done a cache_init() */
		/* These cache entries have mvals in them we need to keep */
		for (cp = cache_entry_base; cp < cache_entry_top; cp++)
		{
			PROCESS_CACHE_ENTRY(cp);
		}
		/* Run the zbreak chain to collect any mvals in it's own cache entries */
		for (z_ptr = (zbrk_struct *)zbrk_recs.beg; z_ptr && z_ptr < (zbrk_struct *)zbrk_recs.free; ++z_ptr)
		{
			if (cp = z_ptr->action)
			{
				PROCESS_CACHE_ENTRY(cp);
			}
		}
		if (x = comline_base)
		{
			for (n = MAX_RECALL;  n > 0 && x->len;  n--, x++)
				MV_STPG_PUT(x);
		}

		if (lvzwrite_block.curr_subsc)
		{
			assert(lvzwrite_block.sub);
			zwr_sub = (zwr_sub_lst *)lvzwrite_block.sub;
			for (n = 0;  n < (int)lvzwrite_block.curr_subsc;  n++)
			{
				assert(zwr_sub->subsc_list[n].actual != zwr_sub->subsc_list[n].first
					|| !zwr_sub->subsc_list[n].second);
				/*
				 * we cannot garbage collect duplicate mval pointers.
				 * So make sure zwr_sub->subsc_list[n].actual is not pointing to an
				 * existing (mval *) which  is already protected
				 */
				if (zwr_sub->subsc_list[n].actual &&
					zwr_sub->subsc_list[n].actual != zwr_sub->subsc_list[n].first)
				{
					x = MV_STPG_GET(zwr_sub->subsc_list[n].actual);
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
		x = MV_STPG_GET(&dollar_zyerror);
		if (x)
			MV_STPG_PUT(x);
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
										top = (unsigned char *)&blk->ptr.sbs_str[blk->cnt];
										s_sbs < (sbs_str_struct *)top;
										s_sbs++)
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
			case MVST_NVAL:
			case MVST_TPHOLD:
				continue;
			case MVST_ZINTR:
				m = &mvs->mv_st_cont.mvs_zintr.savtarg;
				break;
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
	n1 = stringpool.top - stringpool.free; /* Available space before compaction */
	stringpool.free = stringpool.base;
	if (a != array)
	{
		stpg_sort(array, a - 1);
		/* Skip over contiguous block, if any, at beginning of stringpool. */
		for (b = array;  (b < a) && ((*b)->addr <= (char *)stringpool.free);  b++)
		{
			e = (unsigned char *)(*b)->addr;
			e += (*b)->len;
			if (e > stringpool.free)
				stringpool.free = e;
		}
		while (b < a)
		{
			/* Determine extent of next contiguous block to move and move it. */
			delta = (*b)->addr - (char *)stringpool.free;
			for (s = e = (unsigned char *)((*b)->addr);  (b < a) && ((*b)->addr <= (char *)e);  b++)
			{
				assert((*b)->addr >= (char *)s);
				e1 = (unsigned char *)(*b)->addr;
				e1 += (*b)->len;
				if (e1 > e)
					e = e1;
				(*b)->addr -= delta;
				assert(((*b)->addr >= (char *)stringpool.free) && ((*b)->addr <= (char *)(e - delta)));
			}
			n = e - s;
			memmove(stringpool.free, s, n);
			stringpool.free += n;
		}
	}
	n = stringpool.top - stringpool.free; /* Available space after compaction */
	assert(n >= n1);
	space_reclaimed = n - n1; /* Space reclaimed due to compaction */
	if (space_needed && STP_RECLAIMLIMIT > space_reclaimed)
	{
		(*spc_needed_passes)++;
		if (STP_MINRECLAIM > space_reclaimed)
			(*low_reclaim_passes)++;
		else
		{
			*low_reclaim_passes = 0;
			if (STP_ENOUGHRECLAIMED <= space_reclaimed)
				*spc_needed_passes = 0;
		}
	}

	forced_expansion = maxnoexp_growth = FALSE;
	if (n < space_needed || /* i */
	    STP_MINFREE > n || /* ii */
	    (forced_expansion =
	     (!disallow_forced_expansion && /* iii */
	      STP_LIMITFRCDEXPN >= stringpool.top - stringpool.base && /* iv */
	      (STP_MAXLOWRECLAIM_PASSES <= *low_reclaim_passes || /* v */
	      (maxnoexp_growth = (*maxnoexp_passes < STP_MAXNOEXP_PASSES && *spc_needed_passes >= *maxnoexp_passes)))))) /* vi */
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
		e = stringpool.base;
		n = stringpool.free - stringpool.base;
		if ((stringpool.top - stringpool.base) < STP_MAXGEOMGROWTH)
		{
			stp_incr = STP_INCREMENT * (*incr_factor);
			*incr_factor *= STP_GEOMGROWTH_FACTOR;
		} else
			stp_incr = STP_INCREMENT;
		stp_incr = ROUND_UP(stp_incr, OS_PAGE_SIZE);
		if (stp_incr < space_needed)
			stp_incr = ROUND_UP(space_needed, OS_PAGE_SIZE);
		n1 = stp_incr + stringpool.top - stringpool.base;
		assert(n1 >= space_needed + n);
		expand_stp(n1);
		if (e != stringpool.base) /* expanded successfully */
		{
			stringpool.free = stringpool.base + n;
			longcpy(stringpool.base, e, n);
			free(e);
			/*
		 	 * NOTE: rts_stringpool must be kept up-to-date because it is used to tell whether the current
			 * stringpool is the run-time or indirection stringpool.
		 	 */
			(e == rts_stringpool.base) ? (rts_stringpool = stringpool) : (indr_stringpool = stringpool);
			delta = stringpool.base - e;
			for (b = array;  b < a;  b++)
			{
				(*b)->addr += delta;
				assert(((*b)->addr >= (char *)stringpool.base) && ((*b)->addr < (char *)stringpool.free));
			}
		} else
		{
			/* could not expand during forced expansion */
			assert(forced_expansion && disallow_forced_expansion);
		}
		*spc_needed_passes = 0;
		if (maxnoexp_growth && (*maxnoexp_passes < STP_MAXNOEXP_PASSES))
			*maxnoexp_passes += STP_NOEXP_PASSES_INCR;
		*low_reclaim_passes = 0;
	}
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	assert(stringpool.top - stringpool.free >= space_needed);
	return;
}
