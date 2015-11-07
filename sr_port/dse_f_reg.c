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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "util.h"
#include "cli.h"
#include "dse.h"

GBLREF block_id		patch_curr_blk;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF short		crash_count;
GBLREF mval		dollar_zgbldir;
GBLREF gd_addr		*original_header;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;

void dse_f_reg(void)
{
	char		rn[MAX_RN_LEN];
	unsigned short	rnlen;
	int		i;
	boolean_t	found;
	gd_region	*ptr;

	rnlen = SIZEOF(rn);
	if (!cli_get_str("REGION", rn, &rnlen))
		return;
	if (('*' == rn[0]) && (1 == rnlen))
	{
		util_out_print("List of global directory:!_!AD!/", TRUE, dollar_zgbldir.str.len, dollar_zgbldir.str.addr);
		for (i = 0, ptr = original_header->regions; i < original_header->n_regions; i++, ptr++)
		{
			util_out_print("!/File  !_!AD", TRUE, ptr->dyn.addr->fname_len, &ptr->dyn.addr->fname[0]);
			util_out_print("Region!_!AD", TRUE, REG_LEN_STR(ptr));
		}
		return;
	}
	assert(rn[0]);
	found = FALSE;
	for (i = 0, ptr = original_header->regions; i < original_header->n_regions ;i++, ptr++)
	{
		if (found = !memcmp(&ptr->rname[0], &rn[0], MAX_RN_LEN))
			break;
	}
	if (!found)
	{
		util_out_print("Error:  region not found.", TRUE);
		return;
	}
	if (ptr == gv_cur_region)
	{
		util_out_print("Error:  already in region: !AD", TRUE, REG_LEN_STR(gv_cur_region));
		return;
	}
	if (dba_cm == REG_ACC_METH(ptr))
	{
		util_out_print("Error:  Cannot edit an GT.CM database file.", TRUE);
		return;
	}
	if (dba_usr == REG_ACC_METH(ptr))
	{
		util_out_print("Error:  Cannot edit a non-GDS format database file.", TRUE);
		return;
	}
	if (!ptr->open)
	{
		util_out_print("Error:  that region was not opened because it is not bound to any namespace.", TRUE);
		return;
	}
	if (cs_addrs->now_crit)
	{
		util_out_print("Warning:  now leaving region in critical section: !AD", TRUE, gv_cur_region->rname_len,
				gv_cur_region->rname);
	}
	gv_cur_region = ptr;
	gv_target = NULL;	/* to prevent out-of-sync situations between gv_target and cs_addrs */
	assert((dba_mm == REG_ACC_METH(gv_cur_region)) || (dba_bg == REG_ACC_METH(gv_cur_region)));
	cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
	cs_data = cs_addrs->hdr;
	if (cs_addrs && cs_addrs->critical)
		crash_count = cs_addrs->critical->crashcnt;
	util_out_print("!/File  !_!AD", TRUE, DB_LEN_STR(gv_cur_region));
	util_out_print("Region!_!AD!/", TRUE, REG_LEN_STR(gv_cur_region));
	patch_curr_blk = get_dir_root();
	gv_init_reg(gv_cur_region);
	return;
}
