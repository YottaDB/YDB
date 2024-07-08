/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zshow.h"
#include "op.h"
#include "format_targ_key.h"
#include "mlkdef.h"
#include "zwrite.h"
#include "gvt_inline.h"

GBLREF gd_addr			*gd_header;
GBLREF gd_region		*gv_cur_region;
GBLREF gv_key			*gv_currkey;
GBLREF gv_namehead		*gv_target;
GBLREF gvzwrite_datablk	*gvzwrite_block;
GBLREF zshow_out		*zwr_output;

void gvzwr_out(void)
{
	int	n;
	mval	val;
	mval	outdesc;
	mstr	one;
	char	buff[MAX_ZWR_KEY_SZ], *end;

	if ((end = (char *)format_targ_key((uchar_ptr_t)&buff[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE)) == 0)
		end = &buff[MAX_ZWR_KEY_SZ - 1];
	op_gvget(&val);
	if (!MV_DEFINED(&val))
		return;
	MV_FORCE_STRD(&val);
	outdesc.mvtype = MV_STR;
	outdesc.str.addr = &buff[0];
	outdesc.str.len = INTCAST(end - outdesc.str.addr);
	zshow_output(zwr_output, &outdesc.str);
	buff[0] = '=';
	one.addr = &buff[0];
	one.len = 1;
	zshow_output(zwr_output, &one);
	mval_write(zwr_output, &val, TRUE);
	gvzwrite_block->ref_gbldir = gd_header;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	COPY_KEY(gvzwrite_block->ref_key, gv_currkey);
	gvzwrite_block->ref_reg = gv_cur_region;
	assert(gvzwrite_block->ref_targ == reset_gv_target);
	DEBUG_ONLY(gvzwrite_block->ref_targ = gv_target);
	reset_gv_target = gv_target;
}
