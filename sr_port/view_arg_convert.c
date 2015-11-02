/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

LITREF mval 		literal_one;

GBLREF	gd_addr 	*gd_header;
GBLREF	gvt_container	*gvt_pending_list;
GBLREF	buddy_list	*gvt_pending_buddy_list;
GBLREF	symval		*curr_symval;

static	buddy_list	*noisolation_buddy_list;	/* a buddy_list for maintaining the globals that are noisolated */

void view_arg_convert(viewtab_entry *vtp, mval *parm, viewparm *parmblk)
{
	static	int4		first_time = TRUE;
	int			n, res;
	ht_ent_mname		*tabent;
	mstr			tmpstr, namestr;
	gd_region		*r_top, *r_ptr;
	mname_entry		gvent, lvent;
	mident_fixed		lcl_buff;
	unsigned char		global_names[1024], stashed;
	noisolation_element	*gvnh_entry;
	unsigned char 		*src, *nextsrc, *src_top, *dst, *dst_top, y;
	gd_binding		*temp_gd_map, *temp_gd_map_top;
	gv_namehead		*temp_gv_target;
	gvt_container		*gvtc;
	gvnh_reg_t		*gvnh_reg;

	error_def(ERR_VIEWARGCNT);
	error_def(ERR_NOREGION);
	error_def(ERR_VIEWGVN);
	error_def(ERR_VIEWLVN);

	switch(vtp->parm)
	{
		case VTP_NULL:
			if (parm != 0)
				rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
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
				rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			parmblk->value = parm;
			break;
		case (VTP_NULL | VTP_DBREGION):
			if (NULL == parm)
			{
				parmblk->gv_ptr = 0;
				break;
			}
			/* caution:  fall through */
		case VTP_DBREGION:
			if (NULL == parm)
				rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			if (!gd_header)		/* IF GD_HEADER ==0 THEN OPEN GBLDIR */
				gvinit();
			r_ptr = gd_header->regions;
			if (!parm->str.len && vtp->keycode == VTK_GVNEXT)	/* "" => 1st region */
				parmblk->gv_ptr = r_ptr;
			else
			{
				for (r_top = r_ptr + gd_header->n_regions; ; r_ptr++)
				{
					if (r_ptr >= r_top)
						rts_error(VARLSTCNT(4) ERR_NOREGION,2, parm->str.len, parm->str.addr);
					tmpstr.len = r_ptr->rname_len;
					tmpstr.addr = (char *) r_ptr->rname;
					MSTR_CMP(tmpstr, parm->str, n);
					if (0 == n)
						break;
				}
				parmblk->gv_ptr = r_ptr;
			}
			break;
		case VTP_DBKEY:
			if (NULL == parm)
				rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			if (!gd_header)		/* IF GD_HEADER ==0 THEN OPEN GBLDIR */
				gvinit();
			memset(&parmblk->ident.c[0], 0, SIZEOF(parmblk->ident));
			if (parm->str.len >= 2 && *parm->str.addr == '^')
			{
				namestr.addr = parm->str.addr + 1;	/* skip initial '^' */
				namestr.len = parm->str.len - 1;
				if (namestr.len > MAX_MIDENT_LEN)
					namestr.len = MAX_MIDENT_LEN;
				if (valid_mname(&namestr))
					memcpy(&parmblk->ident.c[0], namestr.addr, namestr.len);
				else
					rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, parm->str.len, parm->str.addr);
			} else
				rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, parm->str.len, parm->str.addr);
			break;
		case VTP_RTNAME:
			if (NULL == parm)
				rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
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
				rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
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
			if (SIZEOF(global_names) <= parm->str.len)
				GTMASSERT;
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
				memcpy(global_names, tmpstr.addr, tmpstr.len);
				global_names[tmpstr.len] = '\0';
				src = (unsigned char *)strtok((char *)global_names, ",");
				REINITIALIZE_LIST(noisolation_buddy_list);	/* reinitialize the noisolation buddy_list */
				parmblk->ni_list.gvnh_list = NULL;
				for ( ; src < &global_names[tmpstr.len + 1]; src = nextsrc)
				{
					nextsrc = (unsigned char *)strtok(NULL, ",");
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
							rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, nextsrc - src - 1, src);
					} else
						rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, nextsrc - src - 1, src);
					temp_gv_target = NULL;
					gvent.var_name.addr = &lcl_buff.c[0];
					COMPUTE_HASH_MNAME(&gvent);
					if (NULL == (tabent = lookup_hashtab_mname(gd_header->tab_ptr, &gvent))
					    || (NULL == (gvnh_reg = (gvnh_reg_t *)tabent->value)))
					{
						temp_gd_map = gd_header->maps;
						temp_gd_map_top = temp_gd_map + gd_header->n_maps;
						temp_gd_map++;	/* get past local locks */
						for (; (res = memcmp(gvent.var_name.addr, &(temp_gd_map->name[0]),
								     gvent.var_name.len)) >= 0; temp_gd_map++)
						{
							assert (temp_gd_map < temp_gd_map_top);
							if (0 == res && 0 != temp_gd_map->name[gvent.var_name.len])
								break;
						}
						r_ptr = temp_gd_map->reg.addr;
						assert(r_ptr->max_key_size <= MAX_KEY_SZ);
						temp_gv_target = (gv_namehead *)targ_alloc(r_ptr->max_key_size, &gvent, r_ptr);
						gvnh_reg = (gvnh_reg_t *)malloc(SIZEOF(gvnh_reg_t));
						gvnh_reg->gvt = temp_gv_target;
						gvnh_reg->gd_reg = r_ptr;
						if (NULL != tabent)
						{	/* Since the global name was found but gv_target was null and
							 * now we created a new gv_target, the hash table key must point
							 * to the newly created gv_target->gvname. */
							tabent->key = temp_gv_target->gvname;
							tabent->value = (char *)gvnh_reg;
						} else
						{
							if (!add_hashtab_mname((hash_table_mname *)gd_header->tab_ptr,
									       &temp_gv_target->gvname, gvnh_reg, &tabent))
								GTMASSERT;
						}
						if (!r_ptr->open)
						{	/* Record list of all gv_targets that have been allocated BEFORE the
							 * region has been opened. Once the region gets opened, we will re-examine
							 * this list and reallocate them (if needed) since they have now been
							 * allocated using the region's max_key_size value which could potentially
							 * be different from the max_key_size value in the corresponding database
							 * file header.
							 */
							gvtc = (gvt_container *)get_new_free_element(gvt_pending_buddy_list);
							gvtc->gvnh_reg = gvnh_reg;
							assert(!gvtc->gvnh_reg->gd_reg->open);
							gvtc->next_gvtc = (struct gvt_container_struct *)gvt_pending_list;
							gvt_pending_list = gvtc;
						}
					} else
						temp_gv_target = gvnh_reg->gvt;
					gvnh_entry = (noisolation_element *)get_new_element(noisolation_buddy_list, 1);
					gvnh_entry->gvnh = temp_gv_target;
					gvnh_entry->next = parmblk->ni_list.gvnh_list;
					parmblk->ni_list.gvnh_list = gvnh_entry;
				}
			} else
				rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, tmpstr.len, tmpstr.addr);
			break;
		case VTP_LVN:
			if (NULL == parm)
				rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
			if (0 < parm->str.len)
			{
				lvent.var_name.addr = parm->str.addr;
				lvent.var_name.len = parm->str.len;
				if (lvent.var_name.len > MAX_MIDENT_LEN)
					lvent.var_name.len = MAX_MIDENT_LEN;
				if (!valid_mname(&lvent.var_name))
					rts_error(VARLSTCNT(4) ERR_VIEWLVN, 2, parm->str.len, parm->str.addr);
			} else
				rts_error(VARLSTCNT(4) ERR_VIEWLVN, 2, parm->str.len, parm->str.addr);
			/* Now look up the name.. */
			COMPUTE_HASH_MNAME(&lvent);
			if ((tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &lvent)) && (NULL != tabent->value))
				parmblk->value = (mval *)tabent->value;	/* Return lv_val ptr */
			else
				rts_error(VARLSTCNT(4) ERR_VIEWLVN, 2, parm->str.len, parm->str.addr);
			break;
		default:
			GTMASSERT;
	}
}
