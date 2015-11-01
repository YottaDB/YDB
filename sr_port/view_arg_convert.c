/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "hashdef.h"		/* needed for mname */
#include "view.h"
#include "toktyp.h"
#include "targ_alloc.h"
#include "mstrcmp.h"

LITREF char 		ctypetab[128];
LITREF mval 		literal_one;

GBLREF	gd_addr 	*gd_header;

static	buddy_list	*noisolation_buddy_list;	/* a buddy_list for maintaining the globals that are noisolated */

void view_arg_convert(viewtab_entry *vtp, mval *parm, viewparm *parmblk)
{
static	int4			first_time = TRUE;
	int			n;
	ht_entry		*h;
	mstr			tmpstr;
	gd_region		*r_top, *r_ptr;
	mname			lcl_name;
	unsigned char		global_names[1024], stashed;
	noisolation_element	*gvnh_entry;
	unsigned char 		*src, *nextsrc, *src_top, *dst, *dst_top, y;
	gd_binding		*temp_gd_map, *temp_gd_map_top;
	gv_namehead		*temp_gv_target;

	error_def(ERR_VIEWARGCNT);
	error_def(ERR_NOREGION);
	error_def(ERR_VIEWGVN);

	switch(vtp->parm)
	{
	case VTP_NULL:
		if (parm != 0)
			rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
		break;
	case (VTP_NULL | VTP_VALUE):
		if (0 == parm)
		{
			parmblk->value = (mval *)&literal_one;
			break;
		}
		/* caution:  fall through */
	case VTP_VALUE:
		if (0 == parm)
			rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
		parmblk->value = parm;
		break;
	case (VTP_NULL | VTP_DBREGION):
		if (0 == parm)
		{
			parmblk->gv_ptr = 0;
			break;
		}
		/* caution:  fall through */
	case VTP_DBREGION:
		if (0 == parm)
			rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
		if (!gd_header)		/* IF GD_HEADER ==0 THEN OPEN GBLDIR */
			gvinit();

		r_ptr = gd_header->regions;
		if (!parm->str.len && vtp->keycode == VTK_GVNEXT)	/* "" => 1st region */
			parmblk->gv_ptr = r_ptr;
		else
		{	for (r_top = r_ptr + gd_header->n_regions; ; r_ptr++)
			{
				if (r_ptr >= r_top)
					rts_error(VARLSTCNT(4) ERR_NOREGION,2, parm->str.len, parm->str.addr);
				tmpstr.len = r_ptr->rname_len;
				tmpstr.addr = (char *) r_ptr->rname;
				n = mstrcmp(&tmpstr, &parm->str);
				if (n == 0)
					break;
			}
			parmblk->gv_ptr = r_ptr;
		}
		break;
	case VTP_DBKEY:
		if (0 == parm)
			rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
		if (!gd_header)		/* IF GD_HEADER ==0 THEN OPEN GBLDIR */
			gvinit();

		if (parm->str.len >= 2 && parm->str.len <= sizeof(mident) + 1 &&
			*parm->str.addr == '^')
		{
			dst = (unsigned char *) &parmblk->ident;
			dst_top = dst + sizeof(mident);
			src = (unsigned char *)(parm->str.addr + 1);
			src_top = src + parm->str.len - 1;
			if (*src < 0x7F &&
				((y = ctypetab[*dst = *src]) == TK_UPPER || y == TK_LOWER || y == TK_PERCENT))
			{
				dst++; src++;
				for ( ; src < src_top; src++,dst++)
				{	if (*src > 0x7F)
						break;
					y = ctypetab[*dst = *src];
					if (y != TK_UPPER && y != TK_DIGIT && y != TK_LOWER)
					{	break;
					}
				}
			}
			if (src == src_top)
			{	/* we have an ident */
				for (; dst < dst_top; dst++)
					*dst = 0;
			}
			else
				rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, parm->str.len, parm->str.addr);
		}
		else
			rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, parm->str.len, parm->str.addr);
		break;
	case VTP_RTNAME:
		if (0 == parm)
			rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
		memset(&parmblk->ident, 0, sizeof(mident));
		if (parm->str.len > 0)
		{	memcpy(&parmblk->ident, parm->str.addr,
				(parm->str.len <= sizeof(mident) ? parm->str.len : sizeof(mident)));
		}
		break;
	case VTP_NULL | VTP_DBKEYLIST:
		if (0 == parm || 0 == parm->str.len)
		{
			parmblk->ni_list.gvnh_list = NULL;
			parmblk->ni_list.type = NOISOLATION_NULL;
			break;
		}
		/* caution : explicit fall through */
	case VTP_DBKEYLIST:
		if (0 == parm)
			rts_error(VARLSTCNT(4) ERR_VIEWARGCNT, 2, strlen((const char *)vtp->keyword), vtp->keyword);
		if (!gd_header)
			gvinit();
		if (first_time)
		{
			noisolation_buddy_list = (buddy_list *)malloc(sizeof(buddy_list));
			initialize_list(noisolation_buddy_list, sizeof(noisolation_element), NOISOLATION_INIT_ALLOC);
			first_time = FALSE;
		}
		if (sizeof(global_names) <= parm->str.len)
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
			reinitialize_list(noisolation_buddy_list);	/* reinitialize the noisolation buddy_list */
			parmblk->ni_list.gvnh_list = NULL;
			for ( ; src < &global_names[tmpstr.len + 1]; src = nextsrc)
			{
				nextsrc = (unsigned char *)strtok(NULL, ",");
				if (NULL == nextsrc)
					nextsrc = &global_names[tmpstr.len + 1];
				if (nextsrc - src <= sizeof(mident) + 2 && '^' == *src)
				{
					dst = (unsigned char *)&lcl_name;
					dst_top = dst + sizeof(mident);
					src = (unsigned char *)src + 1;
					src_top = nextsrc - 1;
					if (*src < 0x7F &&
						((y = ctypetab[*dst = *src]) == TK_UPPER || y == TK_LOWER || y == TK_PERCENT))
					{
						dst++; src++;
						for ( ; src < src_top; src++, dst++)
						{
							if (*src > 0x7F)
								break;
							y = ctypetab[*dst = *src];
							if (y != TK_UPPER && y != TK_DIGIT && y != TK_LOWER)
								break;
						}
					}
					if (src == src_top)
					{	/* we have an ident */
						for (; dst < dst_top; dst++)
							*dst = 0;
					} else
						rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, nextsrc - src - 1, src);
				} else
					rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, nextsrc - src - 1, src);	/* to take care of NULL */
				h = ht_put(gd_header->tab_ptr, &lcl_name, (char *)&stashed);
				if (NULL == h->ptr)
				{
					temp_gd_map = gd_header->maps;
					temp_gd_map_top = temp_gd_map + gd_header->n_maps;
					temp_gd_map++;	/* get past local locks */
					for ( ; memcmp (&lcl_name, &(temp_gd_map->name[0]), sizeof(mident)) >= 0; temp_gd_map++)
						assert (temp_gd_map < temp_gd_map_top);
					assert(temp_gd_map->reg.addr->max_key_size <= MAX_KEY_SZ);
					temp_gv_target = (gv_namehead *)targ_alloc(temp_gd_map->reg.addr->max_key_size);
					temp_gv_target->gd_reg = temp_gd_map->reg.addr;
					h->ptr = (char *)temp_gv_target;
					memcpy(&temp_gv_target->gvname, &lcl_name, sizeof(mident));
				}
				gvnh_entry = (noisolation_element *)get_new_element(noisolation_buddy_list, 1);
				gvnh_entry->gvnh = (gv_namehead *)h->ptr;
				gvnh_entry->next = parmblk->ni_list.gvnh_list;
				parmblk->ni_list.gvnh_list = gvnh_entry;
			}
		} else
			rts_error(VARLSTCNT(4) ERR_VIEWGVN, 2, tmpstr.len, tmpstr.addr);
		break;
	default:
		GTMASSERT;
	}
}
