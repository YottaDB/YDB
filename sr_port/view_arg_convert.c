/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "cli.h"
#include "stringpool.h"
#include "mv_stent.h"

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
	char			*cptr, *cptr_start, *cptr_top;
	gd_binding		*gd_map;
	gd_region		*gd_reg_start, *r_ptr, *r_top;
	gvnh_reg_t		*gvnh_reg;
	gvnh_spanreg_t		*gvspan;
	gv_namehead		*tmp_gvt;
	ht_ent_mname		*tabent;
	int			cmp, i, n, reg_index;
	mname_entry		gvent, lvent;
	mident_fixed		lcl_buff;
	mstr			namestr, tmpstr;
	mval			*tmpmv;
	tp_region		*vr, *vr_nxt;
	unsigned char 		*c, *c_top, *dst, *dst_top, global_names[MAX_PARMS], *nextsrc, *src, *src_top, stashed, y;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (vtp_parm)
	{
		case VTP_NULL:
			if (parm != 0)
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			break;
		case (VTP_NULL | VTP_VALUE):
			if ((NULL == parm) && (VTK_JNLERROR != vtp->keycode))
			{
				parmblk->value = (mval *)&literal_one;
				break;
			}
			/* caution:  fall through */
		case VTP_VALUE:
			if ((NULL == parm) && (VTK_JNLERROR != vtp->keycode))
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			parmblk->value = parm;
			if (is_dollar_view || (VTK_JNLERROR != vtp->keycode))
				break;
			parm = NULL;		/* WARNING: fall through for JNLERROR */
		case (VTP_NULL | VTP_DBREGION):
			switch (vtp->keycode)
			{
			case VTK_DBFLUSH:
			case VTK_DBSYNC:
			case VTK_EPOCH:
			case VTK_FLUSH:
			case VTK_GVSRESET:
			case VTK_JNLERROR:
			case VTK_JNLFLUSH:
			case VTK_NOSTATSHARE:
			case VTK_POOLLIMIT:
			case VTK_STATSHARE:
				for (vr = TREF(view_region_list); NULL != vr; vr = vr_nxt)
				{	/* start with empty list, place all existing entries on free list */
					TREF(view_region_list) = vr_nxt = vr->fPtr;	/* Remove from queue */
					vr->fPtr = TREF(view_region_free_list);
					TREF(view_region_free_list) = vr; 		/* Place on free queue */
				}
				parmblk->gv_ptr = NULL;
				if (!is_dollar_view && (NULL != parm)
					&& ((0 == parm->str.len) || ((1 == parm->str.len) && ('*' == parm->str.addr[0]))))
					parm = NULL;					/* WARNING: fall through */
			default:
				break;
			}
			if ((NULL == parm) && is_dollar_view && (VTK_STATSHARE == vtp->keycode))
				break;
			if (!is_dollar_view && (NULL == parm))
			{
				if (!gd_header)		/* IF GD_HEADER == 0 THEN OPEN GBLDIR */
					gvinit();
				for (r_ptr = gd_header->regions, r_top = r_ptr + gd_header->n_regions; r_ptr < r_top; r_ptr++)
				{	/* Operate on all qualifying regions  */
					if ((IS_REG_BG_OR_MM(r_ptr)) && (!(IS_STATSDB_REG(r_ptr))))
					{
						if (!r_ptr->open)
							gv_init_reg(r_ptr, NULL);
						insert_region(r_ptr, &TREF(view_region_list), &(TREF(view_region_free_list)),
							SIZEOF(tp_region));
					}
				}
				parmblk->gv_ptr = (gd_region *)TREF(view_region_list);
				break;
			}/* WARNING: possible fall through - to operate on 1 or more selected regions */
		case VTP_DBREGION:
			if ((NULL == parm) && (VTK_JNLERROR != vtp->keycode))
				rts_error_csa(CSA_ARG(NULL)
					VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			assert(NULL != parm);
			if (!gd_header)							/* IF GD_HEADER == 0 THEN OPEN GBLDIR */
				gvinit();
			if (!parm->str.len)
			{								/* No region */
				if (vtp->keycode != VTK_GVNEXT)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION, 2, LEN_AND_LIT("\"\""));
				parmblk->gv_ptr = gd_header->regions;	 		/* "" => 1st region */
			} else
			{
				namestr.addr = &lcl_buff.c[0];
				cptr = parm->str.addr;
				cptr_top = cptr + parm->str.len;
				for ( ; ; )
				{
					cptr_start = cptr;
					for ( ; ; )
					{
						if (cptr == cptr_top)
							break;
						if (',' == *cptr)
							break;
						cptr++;
					}
					n = cptr - cptr_start;
					if (MAX_MIDENT_LEN < n)
						n = MAX_MIDENT_LEN;
					namestr.len = n;
					for (i = 0; i < n; i++)
						lcl_buff.c[i] = TOUPPER(cptr_start[i]);	/* Region names are upper-case ASCII */
					for (r_ptr = gd_header->regions, r_top = r_ptr + gd_header->n_regions; ; r_ptr++)
					{
						if ((r_ptr >= r_top) || ((cptr_start != parm->str.addr) && is_dollar_view))
						{
							assert((MAX_MIDENT_LEN * MAX_ZWR_EXP_RATIO) < ARRAYSIZE(global_names));
								/* so below "format2zwr" is guaranteed not to overflow */
							format2zwr((sm_uc_ptr_t)namestr.addr, namestr.len, global_names, &n);
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION,2, n, global_names);
						}
						tmpstr.len = r_ptr->rname_len;
						tmpstr.addr = (char *)r_ptr->rname;
						MSTR_CMP(tmpstr, namestr, cmp);
						if (0 == cmp)
						{
							if (!is_dollar_view)
							{
								if (!r_ptr->open)
									gv_init_reg(r_ptr, NULL);
								insert_region(r_ptr, &(TREF(view_region_list)),
									&(TREF(view_region_free_list)), SIZEOF(tp_region));
							}
							break;
						}
					}
					if (cptr == cptr_top)
						break;
					if (',' == *cptr)
						cptr++;	/* for next iteration of for loop */
				}
				parmblk->gv_ptr = is_dollar_view ? r_ptr : (gd_region *)TREF(view_region_list);
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
				assert((MAX_MIDENT_LEN * MAX_ZWR_EXP_RATIO) < ARRAYSIZE(global_names));
					/* so below "format2zwr" is guaranteed not to overflow */
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
			if (NULL == noisolation_buddy_list)
			{
				noisolation_buddy_list = (buddy_list *)malloc(SIZEOF(buddy_list));
				initialize_list(noisolation_buddy_list, SIZEOF(noisolation_element), NOISOLATION_INIT_ALLOC);
				gvt_pending_buddy_list = (buddy_list *)malloc(SIZEOF(buddy_list));
				initialize_list(gvt_pending_buddy_list, SIZEOF(gvt_container), NOISOLATION_INIT_ALLOC);
			}
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
			}
			if (0 == tmpstr.len)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWGVN, 2, tmpstr.len, tmpstr.addr);
				assert(FALSE);
			}
			REINITIALIZE_LIST(noisolation_buddy_list);	/* reinitialize the noisolation buddy_list */
			parmblk->ni_list.gvnh_list = NULL;
			nextsrc = (unsigned char *)tmpstr.addr;
			src_top = nextsrc + tmpstr.len;
			do
			{
				src = nextsrc;
				assert(src < src_top);
				for ( ; ; )
				{
					if (',' == *nextsrc)
						break;
					nextsrc++;
					if (nextsrc == src_top)
						break;
				}
				if ('^' != *src)
				{
					namestr.addr = (char *)src;
					namestr.len = nextsrc - src;
					if (MAX_MIDENT_LEN < namestr.len)
						namestr.len = MAX_MIDENT_LEN;	/* to avoid overflow in "format2zwr" */
					assert((MAX_MIDENT_LEN * MAX_ZWR_EXP_RATIO) < ARRAYSIZE(global_names));
						/* so below "format2zwr" is guaranteed not to overflow */
					format2zwr((sm_uc_ptr_t)namestr.addr, namestr.len, global_names, &n);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWGVN, 2, n, global_names);
					assert(FALSE);
				}
				src++;				/* skip initial '^' */
				namestr.addr = (char *)src;
				namestr.len = nextsrc - src;
				if (MAX_MIDENT_LEN < namestr.len)
					namestr.len = MAX_MIDENT_LEN;	/* to avoid overflow in "format2zwr" */
				if (!valid_mname(&namestr))
				{
					assert((MAX_MIDENT_LEN * MAX_ZWR_EXP_RATIO) < ARRAYSIZE(global_names));
						/* so below "format2zwr" is guaranteed not to overflow */
					format2zwr((sm_uc_ptr_t)namestr.addr, namestr.len, global_names, &n);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_VIEWGVN, 2, n, global_names);
				}
				tmp_gvt = NULL;
				gvent.var_name.len = namestr.len;
				gvent.var_name.addr = namestr.addr;
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
				if (nextsrc == src_top)
					break;
				assert(',' == *nextsrc);
				nextsrc++;
			} while (TRUE);
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
