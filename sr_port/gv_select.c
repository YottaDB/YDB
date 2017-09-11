/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"		/* needed for WCSFLU_* macros */
#include "muextr.h"
#include "iosp.h"
#include "cli.h"
#include "util.h"
#include "op.h"
#include "gt_timer.h"
#include "mupip_exit.h"
#include "gv_select.h"
#include "global_map.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "min_max.h"
#include "hashtab.h"		/* needed for HT_VALUE_DUMMY */
#ifdef GTM64
#include "hashtab_int8.h"
#endif /* GTM64 */
#include "error.h"
#include "gdscc.h"
#include "gdskill.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "hashtab_mname.h"
#include "gvnh_spanreg.h"
#include "change_reg.h"
#include "io.h"
#include "gtmio.h"

#ifdef EXTRACT_HASHT_GLOBAL
# include "gv_trigger_common.h"	/* for IS_GVKEY_HASHT_GBLNAME and HASHT_GBL_CHAR1 macros */
# ifdef GTM_TRIGGER
#  include "gv_trigger.h"
#  include "targ_alloc.h"
#  include "gvcst_protos.h"	/* for gvcst_root_search prototype used in GVTR_SWITCH_REG_AND_HASHT_BIND_NAME macro */
# endif
#endif

#define	MAX_GMAP_ENTRIES_PER_ITER	2 /* maximum increase (could even be negative) in gmap array size per call to global_map */

error_def(ERR_DBRDONLY);
error_def(ERR_FREEZE);
error_def(ERR_FREEZECTRL);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_SELECTSYNTAX);

GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF tp_region	*grlist;
GBLREF gd_addr		*gd_header;

static readonly unsigned char	percent_lit = '%';
static readonly unsigned char	tilde_lit = '~';

STATICFNDCL void gv_select_reg(void *ext_hash, boolean_t freeze, int *reg_max_rec, int *reg_max_key,
				int *reg_max_blk, boolean_t restrict_reg, gvnh_reg_t *gvnh_reg, glist **gl_tail);

void gv_select(char *cli_buff, int n_len, boolean_t freeze, char opname[], glist *gl_head,
	       int *reg_max_rec, int *reg_max_key, int *reg_max_blk, boolean_t restrict_reg)
{
	int			num_quote, len, gmap_size, new_gmap_size, estimated_entries, count, rslt, hash_code;
	int			i, mini, maxi;
	char			*ptr, *ptr1, *c;
	mname_entry		gvname;
	mstr			gmap[512], *gmap_ptr, *gmap_ptr_base, gmap_beg, gmap_end;
	mval			curr_gbl_name;
	gd_region		*reg;
	gv_namehead		*gvt;
	gvnh_reg_t		*gvnh_reg;
	gvnh_spanreg_t		*gvspan;
	ht_ent_mname		*tabent_mname;
	glist			*gl_tail;
#	ifdef GTM64
	hash_table_int8		ext_hash;
#	else
	hash_table_int4		ext_hash;
#	endif
#	ifdef EXTRACT_HASHT_GLOBAL
	gvnh_reg_t		*hashgbl_gvnh_reg = NULL;
	gd_region		*r_top;
	sgmnt_addrs		*csa;
#	endif

	memset(gmap, 0, SIZEOF(gmap));
	gmap_size = SIZEOF(gmap) / SIZEOF(gmap[0]);
	gmap_ptr_base = &gmap[0];
	/* "estimated_entries" is a conservative estimate of the # of entries that could be used up in the gmap array */
	estimated_entries = 1;	/* take into account the NULL gmap entry at the end of the array */
	for (ptr = cli_buff; *ptr; ptr = ptr1)
	{
		for (ptr1 = ptr; ; ptr1++)
		{
			if (',' == *ptr1)
			{
				len = (int)(ptr1 - ptr);
				ptr1++;
				break;
			} else if (!*ptr1)
			{
				len = (int)(ptr1 - ptr);
				break;
			}
		}
		gmap_beg.addr = ptr;
		c = gmap_beg.addr + len - 1;
		num_quote = 0;
		while ('"' == *c)
		{
			len--;
			c--;
			num_quote++;
		}
		if (0 >= len)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SELECTSYNTAX, 2, LEN_AND_STR(opname));
			mupip_exit(ERR_MUNOACTION);
		}
		c = gmap_beg.addr;
		while (0 < num_quote)
		{
			if ('"' == *c)
			{
				c++;
				len--;
			} else
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SELECTSYNTAX, 2, LEN_AND_STR(opname));
				mupip_exit(ERR_MUNOACTION);
			}
			num_quote--;
		}
		gmap_beg.addr = c;
		if ('^' == *c)
		{
			gmap_beg.addr++;
			len--;
		}
		gmap_beg.len = len;
		c = mu_extr_ident(&gmap_beg);
		len -= INTCAST(c - gmap_beg.addr);
		assert(len >= 0);
		if (0 == len)
			gmap_end = gmap_beg;
		else if (gmap_beg.len == 1 && '*' == *c)
		{
			gmap_beg.addr = (char*)&percent_lit;
			gmap_beg.len = SIZEOF(percent_lit);
			gmap_end.addr =  (char*)&tilde_lit;
			gmap_end.len = SIZEOF(tilde_lit);
		} else if (1 == len && '*' == *c)
		{
			gmap_end = gmap_beg;
			gmap_beg.len--;
			*c = '~';
		} else if (':' != *c)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SELECTSYNTAX, 2, LEN_AND_STR(opname));
			mupip_exit(ERR_MUNOACTION);
		} else
		{
			gmap_beg.len = INTCAST(c - gmap_beg.addr);
			c++;
			gmap_end.addr = c;
			gmap_end.len = len - 1;
			if ('^' == *c)
			{
				gmap_end.addr++;
				gmap_end.len--;
			}
			c = mu_extr_ident(&gmap_end);
			MSTR_CMP(gmap_beg, gmap_end, rslt);
			if (((c - gmap_end.addr) != gmap_end.len) || (0 < rslt))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SELECTSYNTAX, 2, LEN_AND_STR(opname));
				mupip_exit(ERR_MUNOACTION);
			}
		}
		/* "estimated_entries" is the maximum number of entries that could be used up in the gmap array including the
		 * next global_map call. The actual number of used entries could be much lower than this.
		 * But since determining the actual number would mean scanning the gmap array for the first NULL pointer (a
		 * performance overhead), we do an approximate check instead.
		 */
		estimated_entries += MAX_GMAP_ENTRIES_PER_ITER;
		if (estimated_entries >= gmap_size)
		{	/* Current gmap array does not have enough space. Double size before calling global_map */
			new_gmap_size = gmap_size * 2;	/* double size of gmap array */
			gmap_ptr = (mstr *)malloc(SIZEOF(mstr) * new_gmap_size);
			memcpy(gmap_ptr, gmap_ptr_base, SIZEOF(mstr) * gmap_size);
			if (gmap_ptr_base != &gmap[0])
				free(gmap_ptr_base);
			gmap_size = new_gmap_size;
			gmap_ptr_base = gmap_ptr;
		}
		global_map(gmap_ptr_base, &gmap_beg, &gmap_end);
		DEBUG_ONLY(
			count = 1;
			for (gmap_ptr = gmap_ptr_base; gmap_ptr->addr; gmap_ptr++)
				count++;
			assert(count < gmap_size);
		)
	}
	if (freeze)
	{
		GTM64_ONLY(init_hashtab_int8(&ext_hash, gmap_size * (100.0 / HT_LOAD_FACTOR),
					HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);)
		NON_GTM64_ONLY(init_hashtab_int4(&ext_hash, gmap_size * (100.0 / HT_LOAD_FACTOR),
					HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);)
	}
	gl_head->next = NULL;
	gl_tail = gl_head;
	*reg_max_rec = 0;
        *reg_max_key = 0;
        *reg_max_blk = 0;
	for (gmap_ptr = gmap_ptr_base; gmap_ptr->addr ; gmap_ptr++)
	{
		curr_gbl_name.mvtype = MV_STR;
		curr_gbl_name.str = *gmap_ptr++;
		DEBUG_ONLY(MSTRP_CMP(&curr_gbl_name.str, gmap_ptr, rslt);)
		assert(0 >= rslt);
		do
		{
			/* User input global names could be arbitrarily large.
			 * Truncate to max supported length before computing hash.
			 */
			if (MAX_MIDENT_LEN < curr_gbl_name.str.len)
				curr_gbl_name.str.len = MAX_MIDENT_LEN;
#			if defined(GTM_TRIGGER) && defined(EXTRACT_HASHT_GLOBAL)
			if (HASHT_GBL_CHAR1 == curr_gbl_name.str.addr[0])
			{	/* Global names starting with "#" e.g. ^#t. Consider these as spanning across
				 * all regions of the gbldir for the purposes of the caller (MUPIP EXTRACT
				 * or MUPIP REORG or MUPIP SIZE).
				 */
				if (NULL == hashgbl_gvnh_reg)
				{
					hashgbl_gvnh_reg = (gvnh_reg_t *)malloc(SIZEOF(gvnh_reg_t));
					/* initialize gvnh_reg fields to NULL specifically gvnh_reg->gvspan
					 * as this is used by the caller of gv_select.
					 */
					memset(hashgbl_gvnh_reg, 0, SIZEOF(gvnh_reg_t));
				}
				/* At this time, only ^#t is supported. In the future ^#k or some such globals
				 * might be supported for other purposes. The following code needs to change if/when
				 * that happens.
				 */
				assert(IS_GVKEY_HASHT_GBLNAME(curr_gbl_name.str.len, curr_gbl_name.str.addr));
				if (!gd_header)
					gvinit();
				for (reg = gd_header->regions, r_top = reg + gd_header->n_regions; reg < r_top; reg++)
				{
					if (IS_STATSDB_REGNAME(reg))
						continue;
					GVTR_SWITCH_REG_AND_HASHT_BIND_NAME(reg);
					csa = cs_addrs;
					if (NULL == csa)	/* not BG or MM access method */
						continue;
					gv_select_reg((void *)&ext_hash, freeze, reg_max_rec, reg_max_key,
									reg_max_blk, restrict_reg, hashgbl_gvnh_reg, &gl_tail);
				}
				break;
			}
#			endif
			COMPUTE_HASH_MSTR(curr_gbl_name.str, hash_code);
			op_gvname_fast(VARLSTCNT(2) hash_code, &curr_gbl_name);
			assert(IS_REG_BG_OR_MM(gv_cur_region));
				/* for dba_cm or dba_usr, op_gvname_fast/gv_bind_name/gv_init_reg would have errored out */
			gvname.hash_code = hash_code;
			gvname.var_name = curr_gbl_name.str;
			tabent_mname = lookup_hashtab_mname((hash_table_mname *)gd_header->tab_ptr, &gvname);
			assert(NULL != tabent_mname);
			gvnh_reg = (gvnh_reg_t *)tabent_mname->value;
			assert(gv_cur_region == gvnh_reg->gd_reg);
			gvspan = gvnh_reg->gvspan;
			if (NULL == gvspan)
				gv_select_reg((void *)&ext_hash, freeze, reg_max_rec, reg_max_key, reg_max_blk,
										restrict_reg, gvnh_reg, &gl_tail);
			else
			{	/* If global spans multiple regions, make sure gv_targets corresponding to ALL
				 * spanned regions are allocated and gv_target->root is also initialized
				 * accordingly for all the spanned regions.
				 */
				gvnh_spanreg_subs_gvt_init(gvnh_reg, gd_header, NULL);
				maxi = gvspan->max_reg_index;
				mini = gvspan->min_reg_index;
				for (i = mini; i <= maxi; i++)
				{
					assert(i >= 0);
					assert(i < gd_header->n_regions);
					reg = gd_header->regions + i;
					gvt = GET_REAL_GVT(gvspan->gvt_array[i - mini]);
					if (NULL != gvt)
					{
						gv_target = gvt;
						gv_cur_region = reg;
						change_reg();
						gv_select_reg((void *)&ext_hash, freeze, reg_max_rec, reg_max_key,
									reg_max_blk, restrict_reg, gvnh_reg, &gl_tail);
					}
				}
			}
			op_gvorder(&curr_gbl_name);
			if (0 == curr_gbl_name.str.len)
			{
				(gmap_ptr + 1)->addr = 0;
				break;
			}
			assert('^' == *curr_gbl_name.str.addr);
			curr_gbl_name.str.addr++;
			curr_gbl_name.str.len--;
			MSTRP_CMP(&curr_gbl_name.str, gmap_ptr, rslt);
		} while (0 >= rslt);
	}
	if (gmap_ptr_base != &gmap[0])
		free(gmap_ptr_base);
}

/* Assumes "gv_target" and "gv_cur_region" are properly setup at function entry */
STATICFNDEF void gv_select_reg(void *ext_hash, boolean_t freeze, int *reg_max_rec, int *reg_max_key,
				int *reg_max_blk, boolean_t restrict_reg, gvnh_reg_t *gvnh_reg, glist **gl_tail)
{
	tp_region	*rptr;
	mval		val;
	glist		*gl_ptr;
#	ifdef GTM64
	ht_ent_int8	*tabent;
#	else
	ht_ent_int4	*tabent;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(gv_target->gd_csa == cs_addrs);
	if (restrict_reg)
	{	/* Only select globals in specified regions */
		for (rptr = grlist; NULL != rptr; rptr = rptr->fPtr)
		{
			if (gv_cur_region == rptr->reg)
				break;
		}
		if (NULL == rptr)
			return;	/* this region is not part of specified regions. return right away */
	}
	TREF(gd_targ_gvnh_reg) = NULL;	/* needed so op_gvdata goes through gvcst_data (i.e. focuses only on the current region)
					 * and NOT through gvcst_spr_data (which would focus on all spanned regions if any).
					 */
	op_gvdata(&val);
	/* It is okay not to restore TREF(gd_targ_gvnh_reg) since it will be initialized again once gv_select is done
	 * and the next op_gvname/op_gvextnam/op_gvnaked call is made. It is not needed until then anyways.
	 */
	if ((0 == val.m[1]) && ((0 == gv_target->root) || !TREF(want_empty_gvts)))
		return;	/* global has an empty GVT in this region. return right away */
	if (freeze)
        {	/* Note: gv_cur_region pointer is 64-bits on 64-bit platforms. So use hashtable accordingly. */
	        GTM64_ONLY(if(add_hashtab_int8((hash_table_int8 *)ext_hash,(gtm_uint64_t *)&gv_cur_region,
											HT_VALUE_DUMMY, &tabent)))
	        NON_GTM64_ONLY(if (add_hashtab_int4((hash_table_int4 *)ext_hash, (uint4 *)&gv_cur_region,
											HT_VALUE_DUMMY, &tabent)))
                {
                        if (FROZEN(cs_addrs->hdr))
                        {
                                gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_FREEZE, 2, REG_LEN_STR(gv_cur_region));
                                mupip_exit(ERR_MUNOFINISH);
                        }
			/* Cannot proceed for read-only data files */
			if (gv_cur_region->read_only)
			{
				util_out_print("Cannot freeze the database",TRUE);
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2,
					DB_LEN_STR(gv_cur_region));
				mupip_exit(ERR_MUNOFINISH);
			}
			while (REG_ALREADY_FROZEN == region_freeze(gv_cur_region, TRUE, FALSE, FALSE, FALSE, TRUE))
			{
				hiber_start(1000);
				if (mu_ctrly_occurred || mu_ctrlc_occurred)
				{
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_FREEZECTRL);
                                	mupip_exit(ERR_MUNOFINISH);
				}
			}
                }
        }
	if (*reg_max_rec < cs_data->max_rec_size)
		*reg_max_rec = cs_data->max_rec_size;
	if (*reg_max_key < cs_data->max_key_size)
		*reg_max_key = cs_data->max_key_size;
	if (*reg_max_blk < cs_data->blk_size)
		*reg_max_blk = cs_data->blk_size;
	gl_ptr = (glist *)malloc(SIZEOF(glist));
	gl_ptr->next = NULL;
	gl_ptr->reg = gv_cur_region;
	gl_ptr->gvt = gv_target;
	gl_ptr->gvnh_reg = gvnh_reg;
	(*gl_tail)->next = gl_ptr;
	*gl_tail = gl_ptr;
}
