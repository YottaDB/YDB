/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
#include "mstrcmp.h"
#include "global_map.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "hashtab_int4.h"
#include "hashtab.h"


GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs      *cs_addrs;


static readonly unsigned char	percent_lit = '%';
static readonly unsigned char	tilde_lit = '~';

void gv_select(char *cli_buff, int n_len, boolean_t freeze, char opname[], glist *gl_head,
	int *reg_max_rec, int *reg_max_key, int *reg_max_blk)
{
	bool				stashed = FALSE;
	int				num_quote, len;
	char				*ptr, *ptr1, *c;
	mstr				gmap[256], *gmap_ptr, gmap_beg, gmap_end;
	mval				val, curr_gbl_name;
	glist				*gl_tail, *gl_ptr;
	hash_table_int4	        	ext_hash;
	ht_ent_int4                   	*tabent;

	error_def(ERR_FREEZE);
	error_def(ERR_DBRDONLY);
	error_def(ERR_SELECTSYNTAX);
	error_def(ERR_MUNOFINISH);
	error_def(ERR_MUNOACTION);

	memset(gmap, 0, sizeof(gmap));
	for (ptr = cli_buff; *ptr; ptr = ptr1)
	{
		for (ptr1 = ptr; ; ptr1++)
		{
			if (',' == *ptr1)
			{
				len = ptr1 - ptr;
				ptr1++;
				break;
			}
			else if (!*ptr1)
			{
				len = ptr1 - ptr;
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
			gtm_putmsg(VARLSTCNT(4) ERR_SELECTSYNTAX, 2, LEN_AND_STR(opname));
			mupip_exit(ERR_MUNOACTION);
		}
		c = gmap_beg.addr;
		while (0 < num_quote)
		{
			if ('"' == *c)
			{
				c++;
				len--;
			}
			else
			{
				gtm_putmsg(VARLSTCNT(4) ERR_SELECTSYNTAX, 2, LEN_AND_STR(opname));
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
		len -= (c - gmap_beg.addr);
		assert(len >= 0);
		if (0 == len)
			gmap_end = gmap_beg;
		else if (gmap_beg.len == 1 && '*' == *c)
		{
			gmap_beg.addr = (char*)&percent_lit;
			gmap_beg.len = sizeof(percent_lit);
			gmap_end.addr =  (char*)&tilde_lit;
			gmap_end.len = sizeof(tilde_lit);
		}
		else if (1 == len && '*' == *c)
		{
			gmap_end = gmap_beg;
			gmap_beg.len--;
			*c = '~';
		}
		else if (':' != *c)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_SELECTSYNTAX, 2, LEN_AND_STR(opname));
			mupip_exit(ERR_MUNOACTION);
		}
		else
		{
			gmap_beg.len = c - gmap_beg.addr;
			c++;
			gmap_end.addr = c;
			gmap_end.len = len - 1;
			if ('^' == *c)
			{
				gmap_end.addr++;
				gmap_end.len--;
			}
			c = mu_extr_ident(&gmap_end);
			if (c - gmap_end.addr != gmap_end.len || mstrcmp(&gmap_beg, &gmap_end) > 0)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_SELECTSYNTAX, 2, LEN_AND_STR(opname));
				mupip_exit(ERR_MUNOACTION);
			}
		}
		global_map(gmap, &gmap_beg, &gmap_end);
	}

	if (freeze)
		init_hashtab_int4(&ext_hash, 0);
	gl_head->next = NULL;
	gl_tail = gl_head;
	*reg_max_rec = 0;
        *reg_max_key = 0;
        *reg_max_blk = 0;
	for (gmap_ptr = gmap; gmap_ptr->addr ; gmap_ptr++)
	{
		curr_gbl_name.mvtype = MV_STR;
		curr_gbl_name.str = *gmap_ptr++;
		op_gvname(VARLSTCNT(1) &curr_gbl_name);
		if (dba_cm == gv_cur_region->dyn.addr->acc_meth)
		{	util_out_print("Can not select globals from region !AD across network",TRUE,gv_cur_region->rname_len,
					gv_cur_region->rname);
			mupip_exit(ERR_MUNOFINISH);

		}
		if (dba_bg != gv_cur_region->dyn.addr->acc_meth && dba_mm != gv_cur_region->dyn.addr->acc_meth)
		{
			assert(gv_cur_region->dyn.addr->acc_meth == dba_usr);
			util_out_print("Can not select globals from non-GTC region !AD",TRUE,gv_cur_region->rname_len,
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
			if (mstrcmp(&curr_gbl_name.str, gmap_ptr) > 0)
				break;
			if (freeze)
                        {
				/* Note: We cannot use int4 hash when we will have 64-bit address.
				 * In that case we may choose to hash the region name or use int8 hash */
				if (add_hashtab_int4(&ext_hash, (uint4 *)&gv_cur_region, HT_VALUE_DUMMY, &tabent))
                                {
                                        if (cs_addrs->hdr->freeze)
                                        {
                                                gtm_putmsg(VARLSTCNT(4) ERR_FREEZE, 2, gv_cur_region->rname_len,
							gv_cur_region->rname);
                                                mupip_exit(ERR_MUNOFINISH);
                                        }
					/* Cannot proceed for read-only data files */
					if (gv_cur_region->read_only)
					{
						util_out_print("Cannot freeze the database",TRUE);
						gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2,
							DB_LEN_STR(gv_cur_region));
						mupip_exit(ERR_MUNOFINISH);
					}
                                        while (FALSE == region_freeze(gv_cur_region, TRUE, FALSE))
                                                hiber_start(1000);
					wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
                                }
                        }
			assert(curr_gbl_name.str.len > 0);
			gl_ptr = (glist*)malloc(sizeof(glist) - 1 + curr_gbl_name.str.len);
			gl_ptr->name.mvtype = MV_STR;
			gl_ptr->name.str.addr = (char*)gl_ptr->nbuf;
			gl_ptr->name.str.len = curr_gbl_name.str.len;
			memcpy(gl_ptr->nbuf, curr_gbl_name.str.addr, curr_gbl_name.str.len);
			gl_ptr->next = 0;
			gl_tail->next = gl_ptr;
			gl_tail = gl_ptr;
			if (*reg_max_rec < cs_data->max_rec_size) *reg_max_rec = cs_data->max_rec_size;
                        if (*reg_max_key < cs_data->max_key_size) *reg_max_key = cs_data->max_key_size;
                        if (*reg_max_blk < cs_data->blk_size) *reg_max_blk = cs_data->blk_size;
			op_gvname(VARLSTCNT(1) &gl_tail->name);
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
}
