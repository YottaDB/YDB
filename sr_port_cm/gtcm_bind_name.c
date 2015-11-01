/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "filestruct.h"
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"

#define DIR_ROOT 1

GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_data	*cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_namehead	*gv_target;

void gtcm_bind_name(cm_region_head *rh)
{
	ht_entry		*hte_ptr, *ht_put();
	char			stashed;
	register unsigned char	*c0, *c_top;
	unsigned int		idx;
	mname			lcl_name;

	gv_cur_region = rh->reg;
	for (c0 = (unsigned char *)&lcl_name, c_top = c0 + sizeof(lcl_name), idx = 0;
		gv_currkey->base[idx] && idx < sizeof(lcl_name.txt);  idx++)
		*c0++ = gv_currkey->base[idx];
	while (c0 < c_top)
		*c0++ = 0;
	hte_ptr = ht_put(rh->reg_hash, &lcl_name, &stashed);
	if (stashed || !hte_ptr->ptr)
		hte_ptr->ptr = (char *)targ_alloc(FILE_INFO(gv_cur_region)->s_addrs.hdr->max_key_size);
	gv_target = (gv_namehead *)hte_ptr->ptr;
	if ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth))
	{
		cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
		cs_data = cs_addrs->hdr;
	}
	else
		GTMASSERT;
	if ((NULL == gv_target->root) || (DIR_ROOT == gv_target->root))
		gvcst_root_search();
	if (gv_target->collseq || gv_target->nct)
		gv_xform_key(gv_currkey, FALSE);
	return;
}
