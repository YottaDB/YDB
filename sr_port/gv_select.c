/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

static readonly unsigned char	percent_lit = '%';
static readonly unsigned char	tilde_lit = '~';

void gv_select(char *cli_buff, int n_len, boolean_t freeze, char opname[], glist *gl_head,
	       int *reg_max_rec, int *reg_max_key, int *reg_max_blk, boolean_t restrict_reg)
{
	boolean_t			stashed = FALSE, append_gbl;
	int				num_quote, len, gmap_size, new_gmap_size, estimated_entries, count, rslt;
	char				*ptr, *ptr1, *c;
	mstr				gmap[512], *gmap_ptr, *gmap_ptr_base, gmap_beg, gmap_end;
	mval				val, curr_gbl_name;
	glist				*gl_tail, *gl_ptr;
	tp_region			*rptr;
#ifdef GTM64
	hash_table_int8	        	ext_hash;
	ht_ent_int8                   	*tabent;
#else
	hash_table_int4	        	ext_hash;
	ht_ent_int4                   	*tabent;
#endif /* GTM64 */

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
		GTM64_ONLY(init_hashtab_int8(&ext_hash, 0, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);)
		NON_GTM64_ONLY(init_hashtab_int4(&ext_hash, 0, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);)
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
		op_gvname(VARLSTCNT(1) &curr_gbl_name);
		append_gbl = TRUE;
		if (dba_cm == gv_cur_region->dyn.addr->acc_meth)
		{	util_out_print("Can not select globals from region !AD across network",TRUE,gv_cur_region->rname_len,
					gv_cur_region->rname);
			mupip_exit(ERR_MUNOFINISH);

		}
		if (dba_bg != gv_cur_region->dyn.addr->acc_meth && dba_mm != gv_cur_region->dyn.addr->acc_meth)
		{
			assert(gv_cur_region->dyn.addr->acc_meth == dba_usr);
			util_out_print("Can not select globals from non-GDS format region !AD",TRUE,gv_cur_region->rname_len,
					gv_cur_region->rname);
			mupip_exit(ERR_MUNOFINISH);
		}
		op_gvdata(&val);
		if (0 == val.m[1])
		{
			op_gvname(VARLSTCNT(1) &curr_gbl_name);
			op_gvorder(&curr_gbl_name);
			if (!curr_gbl_name.str.len)
				break;
			assert('^' == *curr_gbl_name.str.addr);
			curr_gbl_name.str.addr++;
			curr_gbl_name.str.len--;
		}
		for (;;)
		{
			MSTRP_CMP(&curr_gbl_name.str, gmap_ptr, rslt);
			if (0 < rslt)
				break;
			if (freeze)
                        {
				/* Note: We cannot use int4 hash when we will have 64-bit address.
				 * In that case we may choose to hash the region name or use int8 hash */

			        GTM64_ONLY(if(add_hashtab_int8(&ext_hash,(gtm_uint64_t *)&gv_cur_region, HT_VALUE_DUMMY, &tabent)))
			        NON_GTM64_ONLY(if (add_hashtab_int4(&ext_hash, (uint4 *)&gv_cur_region, HT_VALUE_DUMMY, &tabent)))
                                {
                                        if (cs_addrs->hdr->freeze)
                                        {
                                                gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FREEZE, 2, gv_cur_region->rname_len,
							gv_cur_region->rname);
                                                mupip_exit(ERR_MUNOFINISH);
                                        }
					/* Cannot proceed for read-only data files */
					if (gv_cur_region->read_only)
					{
						util_out_print("Cannot freeze the database",TRUE);
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRDONLY, 2,
							DB_LEN_STR(gv_cur_region));
						mupip_exit(ERR_MUNOFINISH);
					}
					while (REG_ALREADY_FROZEN == region_freeze(gv_cur_region, TRUE, FALSE, FALSE))
					{
						hiber_start(1000);
						if (mu_ctrly_occurred || mu_ctrlc_occurred)
						{
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_FREEZECTRL);
                                                	mupip_exit(ERR_MUNOFINISH);
						}
					}
					wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
                                }
                        }
			assert(0 < curr_gbl_name.str.len);
			gl_ptr = (glist*)malloc(SIZEOF(glist) - 1 + curr_gbl_name.str.len);
			gl_ptr->name.mvtype = MV_STR;
			gl_ptr->name.str.addr = (char*)gl_ptr->nbuf;
			gl_ptr->name.str.len = MIN(curr_gbl_name.str.len, MAX_MIDENT_LEN);
			memcpy(gl_ptr->nbuf, curr_gbl_name.str.addr, curr_gbl_name.str.len);
			gl_ptr->next = 0;
			if (append_gbl)
			{
				if (*reg_max_rec < cs_data->max_rec_size)
					*reg_max_rec = cs_data->max_rec_size;
				if (*reg_max_key < cs_data->max_key_size)
					*reg_max_key = cs_data->max_key_size;
				if (*reg_max_blk < cs_data->blk_size)
					*reg_max_blk = cs_data->blk_size;
			}
			op_gvname(VARLSTCNT(1) &gl_ptr->name);
			if (restrict_reg)
			{ /* Only select globals in specified regions */
				append_gbl = FALSE;
				for (rptr = grlist; NULL != rptr; rptr = rptr->fPtr)
				{
					if (gv_cur_region == rptr->reg)
					{
						append_gbl = TRUE;
						break;
					}
				}
			}
			if (append_gbl)
			{
				gl_tail->next = gl_ptr;
				gl_tail = gl_ptr;
			} else
				free(gl_ptr);
			op_gvorder(&curr_gbl_name);
			if (0 == curr_gbl_name.str.len)
			{
				(gmap_ptr + 1)->addr = 0;
				break;
			}
			assert('^' == *curr_gbl_name.str.addr);
			curr_gbl_name.str.addr++;
			curr_gbl_name.str.len--;
		}
	}
	if (gmap_ptr_base != &gmap[0])
		free(gmap_ptr_base);
}
