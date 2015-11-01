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

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "copy.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "mv_stent.h"		/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "op.h"
#include "gvcst_root_search.h"
#include "format_targ_key.h"
#include "gvsub2str.h"
#include "sgnl.h"
#include "mvalconv.h"
#include "tp_set_sgm.h"
#include <varargs.h>

GBLDEF bool		gv_curr_subsc_null;
GBLDEF gd_addr		*gd_targ_addr = 0;

GBLREF gd_addr		*gd_header;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gd_binding	*gd_map;
GBLREF sgm_info		*first_sgm_info;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF short		dollar_tlevel;
GBLREF bool		transform;
GBLREF mstr		extnam_str;

void op_gvname(va_alist)
va_dcl
{
	int		count, len;
	bool		was_null, is_null;
	boolean_t	bgormm;
	mval		*val;
	short int	max_key;
	va_list		var;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;

	error_def(ERR_GVSUBOFLOW);
	error_def(ERR_GVIS);

	extnam_str.len = 0;
	if (!gd_header)
		gvinit();
	gd_targ_addr = gd_header;
	VAR_START(var);
	count = va_arg(var, int) - 1;
	val = va_arg(var, mval *);
	if ((gd_header->maps == gd_map) && gv_currkey && (0 == gv_currkey->base[val->str.len]) &&
			(0 == memcmp(gv_currkey->base, val->str.addr, val->str.len)))
	{
		gv_currkey->end = val->str.len + 1;
		gv_currkey->base[gv_currkey->end] = 0;
		gv_currkey->prev = 0;
		bgormm = ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth));
		if (bgormm)
		{
			if (dollar_tlevel && !first_sgm_info)
				tp_set_sgm();
			assert(INVALID_GV_TARGET != gv_target);
			if ((!gv_target->root) || (DIR_ROOT == gv_target->root))
				gvcst_root_search();
		}
	} else
	{
		gv_bind_name(gd_header, &(val->str));
		DEBUG_ONLY(
			bgormm = ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth));)
	}
	assert(!bgormm || (gv_cur_region && &FILE_INFO(gv_cur_region)->s_addrs == cs_addrs && cs_addrs->hdr == cs_data));
	assert(bgormm || !dollar_tlevel);
	assert(!dollar_tlevel || sgm_info_ptr && sgm_info_ptr->gv_cur_region == gv_cur_region);
	assert(gd_targ_addr == gd_header);
	was_null = is_null = FALSE;
	max_key = gv_cur_region->max_key_size;
	for ( ; count > 0; count--)
	{
		COPY_SUBS_TO_GVCURRKEY(var, gv_currkey, was_null, is_null);	/* updates gv_currkey, was_null, is_null */
	}
	gv_curr_subsc_null = is_null;
	if (was_null && !gv_cur_region->null_subs)
		sgnl_gvnulsubsc();
	return;
}
