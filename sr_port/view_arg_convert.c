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

#include "gtm_string.h"
#include "buddy_list.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "view.h"
#include "toktyp.h"
#include "targ_alloc.h"
#include "valid_mname.h"
#include "dpgbldir.h"
#include "lv_val.h"	/* needed for "symval" structure */
#include "min_max.h"
#include "gvnh_spanreg.h"
#include "process_gvt_pending_list.h"	/* for "is_gvt_in_pending_list" prototype used in ADD_TO_GVT_PENDING_LIST_IF_REG_NOT_OPEN */
#include "gtmimagename.h"
#include "gv_trigger_common.h"	/* for *HASHT* macros used inside GVNH_REG_INIT macro */
#include "filestruct.h"		/* needed for "jnl.h" */
#include "jnl.h"		/* needed for "jgbl" */
#include "zshow.h"		/* needed for format2zwr */

LITREF mval 		literal_one;

GBLREF	gd_addr 	*gd_header;
GBLREF	buddy_list	*gvt_pending_buddy_list;
GBLREF	symval		*curr_symval;
GBLREF	buddy_list	*noisolation_buddy_list;	/* a buddy_list for maintaining the globals that are noisolated */
GBLREF	volatile boolean_t	timer_in_handler;

error_def(ERR_NOREGION);
error_def(ERR_NOTGBL);
error_def(ERR_VIEWARGCNT);
error_def(ERR_VIEWGVN);
error_def(ERR_VIEWLVN);

void view_arg_convert(viewtab_entry *vtp, int vtp_parm, mval *parm, viewparm *parmblk, boolean_t is_dollar_view)
{
	static	int4		first_time = TRUE;
	char			*cptr;
	char			*strtokptr;
	gd_binding		*gd_map;
	gd_region		*gd_reg_start, *r_ptr, *r_top;
	gvnh_reg_t		*gvnh_reg;
	gvnh_spanreg_t		*gvspan;
	gv_namehead		*tmp_gvt;
	ht_ent_mname		*tabent;
	int			n, reg_index;
	mident_fixed		lcl_buff;
	mname_entry		gvent, lvent;
	mstr			namestr, tmpstr;
	unsigned char 		*c, *c_top, *dst, *dst_top, global_names[1024], *nextsrc, *src, *src_top, stashed, y;

	switch (vtp_parm)
	{
		case VTP_NULL:
			if (parm != 0)
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			break;
		case (VTP_NULL | VTP_VALUE):
			if (NULL == parm)
			{
				parmblk->value = (mval *)&literal_one;
				break;
			}
			/* caution:  fall through */
		case VTP_VALUE:
			if (NULL == parm)
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			parmblk->value = parm;
			break;
		case (VTP_NULL | VTP_DBREGION):
			if (!is_dollar_view && ((NULL == parm) || ((1 == parm->str.len) && ('*' == *parm->str.addr))))
			{
				parmblk->gv_ptr = NULL;
				break;
			}
			/* caution:  fall through */
		case VTP_DBREGION:
			if (NULL == parm)
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			if (!gd_header)		/* IF GD_HEADER ==0 THEN OPEN GBLDIR */
				gvinit();
			r_ptr = gd_header->regions;
			if (!parm->str.len && vtp->keycode == VTK_GVNEXT)	/* "" => 1st region */
				parmblk->gv_ptr = r_ptr;
			else
			{
				for (cptr = parm->str.addr, n = 0; n < parm->str.len; cptr++, n++)
					lcl_buff.c[n] = TOUPPER(*cptr);		/* Region names are upper-case ASCII */
				namestr.len = n;
				namestr.addr = &lcl_buff.c[0];
				for (r_top = r_ptr + gd_header->n_regions; ; r_ptr++)
				{
					if (r_ptr >= r_top)
					{
						format2zwr((sm_uc_ptr_t)parm->str.addr, parm->str.len, global_names, &n);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION,2, n, global_names);
					}
					tmpstr.len = r_ptr->rname_len;
					tmpstr.addr = (char *)r_ptr->rname;
					MSTR_CMP(tmpstr, namestr, n);
					if (0 == n)
						break;
				}
				parmblk->gv_ptr = r_ptr;
			}
			break;
		case VTP_DBKEY:
			if (NULL == parm)
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			if (!parm->str.len)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOTGBL, 2, parm->str.len, NULL);
			if (!gd_header)		/* IF GD_HEADER ==0 THEN OPEN GBLDIR */
				gvinit();
			c = (unsigned char *)parm->str.addr;
			if ('^' != *c)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOTGBL, 2, parm->str.len, c);
			c_top = c + parm->str.len;
			c++;				/* skip initial '^' */
			parmblk->str.addr = (char *)c;
			for ( ; (c < c_top) && ('(' != *c); c++)
				;
			parmblk->str.len = (char *)c - parmblk->str.addr;
			if (MAX_MIDENT_LEN < parmblk->str.len)
				parmblk->str.len = MAX_MIDENT_LEN;
			if (!valid_mname(&parmblk->str))
			{
				format2zwr((sm_uc_ptr_t)parm->str.addr, parm->str.len, global_names, &n);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWGVN, 2, n, global_names);
			}
			break;
		case VTP_RTNAME:
			if (NULL == parm)
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			memset(&parmblk->ident.c[0], 0, SIZEOF(parmblk->ident));
			if (parm->str.len > 0)
				memcpy(&parmblk->ident.c[0], parm->str.addr,
				       (parm->str.len <= MAX_MIDENT_LEN ? parm->str.len : MAX_MIDENT_LEN));
			break;
		case VTP_NULL | VTP_DBKEYLIST:
			if (NULL == parm || 0 == parm->str.len)
			{
				parmblk->ni_list.gvnh_list = NULL;
				parmblk->ni_list.type = NOISOLATION_NULL;
				break;
			}
			/* caution : explicit fall through */
		case VTP_DBKEYLIST:
			if (NULL == parm)
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			if (!gd_header)
				gvinit();
			if (first_time)
			{
				noisolation_buddy_list = (buddy_list *)malloc(SIZEOF(buddy_list));
				initialize_list(noisolation_buddy_list, SIZEOF(noisolation_element), NOISOLATION_INIT_ALLOC);
				gvt_pending_buddy_list = (buddy_list *)malloc(SIZEOF(buddy_list));
				initialize_list(gvt_pending_buddy_list, SIZEOF(gvt_container), NOISOLATION_INIT_ALLOC);
				first_time = FALSE;
			}
			assertpro(SIZEOF(global_names) > parm->str.len);
			tmpstr.len = parm->str.len;	/* we need to change len and should not change parm->str, so take a copy */
			tmpstr.addr = parm->str.addr;
			if (0 != tmpstr.len)
			{
				switch (*tmpstr.addr)
				{
					case '+' :
						parmblk->ni_list.type = NOISOLATION_PLUS;
						tmpstr.addr++;
						tmpstr.len--;
						break;
					case '-' :
						parmblk->ni_list.type = NOISOLATION_MINUS;
						tmpstr.addr++;
						tmpstr.len--;
						break;
					default :
						parmblk->ni_list.type = NOISOLATION_NULL;
						break;
				}
				if (!tmpstr.len)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWGVN, 2, tmpstr.len, NULL);
				memcpy(global_names, tmpstr.addr, tmpstr.len);
				global_names[tmpstr.len] = '\0';
				src = (unsigned char *)STRTOK_R((char *)global_names, ",", &strtokptr);
				REINITIALIZE_LIST(noisolation_buddy_list);	/* reinitialize the noisolation buddy_list */
				parmblk->ni_list.gvnh_list = NULL;
				for ( ; src < &global_names[tmpstr.len + 1]; src = nextsrc)
				{
					nextsrc = (unsigned char *)STRTOK_R(NULL, ",", &strtokptr);
					if (NULL == nextsrc)
						nextsrc = &global_names[tmpstr.len + 1];
					if (nextsrc - src >= 2 && '^' == *src)
					{
						namestr.addr = (char *)src + 1;		/* skip initial '^' */
						namestr.len = INTCAST(nextsrc - src - 2); /* don't count initial ^ and trailing 0 */
						if (namestr.len > MAX_MIDENT_LEN)
							namestr.len = MAX_MIDENT_LEN;
						if (valid_mname(&namestr))
						{
							memcpy(&lcl_buff.c[0], namestr.addr, namestr.len);
							gvent.var_name.len = namestr.len;
						} else
						{
							memcpy(&lcl_buff.c[0], src, nextsrc - src - 1);
							format2zwr((sm_uc_ptr_t)&lcl_buff.c, nextsrc - src - 1, global_names, &n);
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWGVN, 2, n, global_names);
						}
					} else
					{
						memcpy(&lcl_buff.c[0], src, nextsrc - src - 1);
						format2zwr((sm_uc_ptr_t)&lcl_buff.c, nextsrc - src - 1, global_names, &n);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWGVN, 2, n, global_names);
					}
					tmp_gvt = NULL;
					gvent.var_name.addr = &lcl_buff.c[0];
					COMPUTE_HASH_MNAME(&gvent);
					if (NULL != (tabent = lookup_hashtab_mname(gd_header->tab_ptr, &gvent)))
					{
						gvnh_reg = (gvnh_reg_t *)tabent->value;
						assert(NULL != gvnh_reg);
						tmp_gvt = gvnh_reg->gvt;
					} else
					{
						gd_map = gv_srch_map(gd_header, gvent.var_name.addr, gvent.var_name.len,
													SKIP_BASEDB_OPEN_FALSE);
						r_ptr = gd_map->reg.addr;
						tmp_gvt = (gv_namehead *)targ_alloc(r_ptr->max_key_size, &gvent, r_ptr);
						GVNH_REG_INIT(gd_header, gd_header->tab_ptr, gd_map, tmp_gvt,
											r_ptr, gvnh_reg, tabent);
						/* In case of a global spanning multiple regions, the gvt pointer corresponding to
						 * the region where the unsubscripted global reference maps to is stored in TWO
						 * locations (one in gvnh_reg->gvspan->gvt_array[index] and one in gvnh_reg->gvt.
						 * So pass in both these pointer addresses to be stored in the pending list in
						 * case this gvt gets reallocated (due to different keysizes between gld and db).
						 */
						if (NULL == (gvspan = gvnh_reg->gvspan))
						{
							ADD_TO_GVT_PENDING_LIST_IF_REG_NOT_OPEN(r_ptr, &gvnh_reg->gvt, NULL);
						} else
						{
							gd_reg_start = &gd_header->regions[0];
							GET_REG_INDEX(gd_header, gd_reg_start, r_ptr, reg_index);
								/* the above sets "reg_index" */
							assert(reg_index >= gvspan->min_reg_index);
							assert(reg_index <= gvspan->max_reg_index);
							reg_index -= gvspan->min_reg_index;
							ADD_TO_GVT_PENDING_LIST_IF_REG_NOT_OPEN(r_ptr,
								&gvspan->gvt_array[reg_index], &gvnh_reg->gvt);
						}
					}
					ADD_GVT_TO_VIEW_NOISOLATION_LIST(tmp_gvt, parmblk);
					if (!is_dollar_view && (NULL != gvnh_reg->gvspan))
					{	/* Global spans multiple regions. Make sure gv_targets corresponding to ALL
						 * spanned regions are allocated so NOISOLATION status can be set in all of
						 * them even if the corresponding regions are not open yet. Do this only for
						 * VIEW "NOISOLATION" commands which change the noisolation characteristic.
						 * $VIEW("NOISOLATION") only examines the characteristics and so no need to
						 * allocate all the gv-targets in that case. Just one is enough.
						 */
						gvnh_spanreg_subs_gvt_init(gvnh_reg, gd_header, parmblk);
					}
				}
			} else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWGVN, 2, tmpstr.len, tmpstr.addr);
			break;
		case VTP_LVN:
			if (NULL == parm)
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			if (0 < parm->str.len)
			{
				lvent.var_name.addr = parm->str.addr;
				lvent.var_name.len = parm->str.len;
				if (lvent.var_name.len > MAX_MIDENT_LEN)
					lvent.var_name.len = MAX_MIDENT_LEN;
				if (!valid_mname(&lvent.var_name))
				{
					format2zwr((sm_uc_ptr_t)parm->str.addr, parm->str.len, global_names, &n);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWLVN, 2, n, global_names);
				}
			} else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWLVN, 2, parm->str.len, parm->str.addr);
			/* Now look up the name.. */
			COMPUTE_HASH_MNAME(&lvent);
			if ((tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &lvent)) && (NULL != tabent->value))
				parmblk->value = (mval *)tabent->value;	/* Return lv_val ptr */
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWLVN, 2, parm->str.len, parm->str.addr);
			break;
		default:
			assertpro(FALSE && vtp_parm);
	}
}
