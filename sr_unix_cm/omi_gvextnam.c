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

/*
 *  omi_gvextnam.c ---
 *
 *
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include "gtm_string.h"

#include "omi.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "error.h"
#include "parse_file.h"
#include "gbldirnam.h"
#include "dpgbldir.h"
#include "gvcst_protos.h"	/* for gvcst_root_search in GV_BIND_NAME_AND_ROOT_SEARCH macro */
#include "hashtab_mname.h"

GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*gd_header;

int	omi_gvextnam (omi_conn *cptr, uns_short len, char *ref)
{
	bool		was_null, is_null;
	mval		v;
	mname_entry	gvname;
	char		*ptr, *end, c[MAX_FBUFF + 1];
	omi_li		li;
	omi_si		si;
	parse_blk	pblk;
	int4		status;
	gd_segment	*cur_seg, *last_seg;
	gvnh_reg_t	*gvnh_reg;
	gd_region	*reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
/*	Pointers into the global reference */
	ptr = ref;
	end = ref + len;

/*	Initialize part of the mval */
	v.mvtype = MV_STR;

/*	Refine the gd_addr given this environment */
	OMI_LI_READ(&li, ptr);
	if (ptr + li.value > end)
		return -OMI_ER_PR_INVGLOBREF;
	v.str.len   = li.value;
	v.str.addr  = ptr;
	cptr->ga    = zgbldir(&v);
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = c;
	pblk.buff_size = MAX_FBUFF;
	pblk.def1_buf = DEF_GDR_EXT;
	pblk.def1_size = SIZEOF(DEF_GDR_EXT) - 1;
	status = parse_file(&v.str, &pblk);

	/* for all segments insert the full path in the segment fname */
	cur_seg = cptr->ga->segments;
	last_seg  = cur_seg + cptr->ga->n_segments;
	for( ; cur_seg < last_seg ; cur_seg++)
	{
		if ('/' != cur_seg->fname[0])
		{	/* doesn't contains full path ; specify full path */
			memmove(&cur_seg->fname[0] + pblk.b_dir, cur_seg->fname, cur_seg->fname_len);
			memcpy(cur_seg->fname, pblk.l_dir, pblk.b_dir);
			cur_seg->fname_len += pblk.b_dir;
		}
	}
	ptr += li.value;
	/* Refine the gd_addr given this name */
	OMI_SI_READ(&si, ptr);
	if (si.value <= 1  ||  *ptr != '^')
		return -OMI_ER_PR_INVGLOBREF;
	ptr++;
	si.value--;
	if (ptr + si.value > end)
		return -OMI_ER_PR_INVGLOBREF;
	gd_header   = cptr->ga;
	gvname.var_name.len   = si.value;
	gvname.var_name.addr  = ptr;
	COMPUTE_HASH_MNAME(&gvname);
	GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &gvname, gvnh_reg);
	/* gv_cur_region will not be set in case gvnh_reg->gvspan is non-NULL. So use region from gvnh_reg */
	reg = gvnh_reg->gd_reg;
	ptr        += si.value;
	/* Refine the gd_addr given these subscripts */
	was_null = is_null  = FALSE;
	while (ptr < end)
	{
		was_null  |= is_null;
		OMI_SI_READ(&si, ptr);
		if (ptr + si.value > end)
			return -OMI_ER_PR_INVGLOBREF;
		v.mvtype   = MV_STR;
		v.str.len  = si.value;
		v.str.addr = ptr;
		is_null    = (si.value == 0);
		mval2subsc(&v, gv_currkey, reg->std_null_coll);
		ptr       += si.value;
	}
	TREF(gv_some_subsc_null) = was_null; /* if true, it indicates there is a null subscript (except the last subscript)
						in current key */
	TREF(gv_last_subsc_null) = is_null; /* if true, it indicates that last subscript in current key is null */
	if (was_null && (NEVER == reg->null_subs))
		return -OMI_ER_DB_INVGLOBREF;
	/* The below macro finishes the task of GV_BIND_NAME_AND_ROOT_SEARCH (e.g. setting gv_cur_region for spanning globals). */
	GV_BIND_SUBSNAME_IF_GVSPAN(gvnh_reg, gd_header, gv_currkey, reg);
	return 0;
}
