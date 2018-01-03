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

#include "gtm_stdlib.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "error.h"
#include "change_reg.h"
#include "gtm_logicals.h"
#include "min_max.h"

GBLREF gd_addr		*gd_header;
GBLREF gd_region	*gv_cur_region;

void region_open(void);

error_def (ERR_DBNOREGION);

boolean_t region_init(bool cm_regions)
{
	gd_region		*baseDBreg, *first_nonstatsdb_reg, *reg, *reg_top;
	boolean_t		is_cm, all_files_open;
	sgmnt_addrs		*baseDBcsa;
	node_local_ptr_t	baseDBnl;

	all_files_open = TRUE;
	first_nonstatsdb_reg = NULL;
	reg_top = gd_header->regions + gd_header->n_regions;
	for (gv_cur_region = gd_header->regions; gv_cur_region < reg_top; gv_cur_region++)
	{
		if (gv_cur_region->open)
			continue;
		if (!IS_REG_BG_OR_MM(gv_cur_region))
			continue;
		if (IS_STATSDB_REG(gv_cur_region))
			continue;			/* Bypass statsDB files */
		is_cm = reg_cmcheck(gv_cur_region);
		if (!is_cm || cm_regions)
		{
			region_open();
			if (gv_cur_region->open)
			{
				if (NULL == first_nonstatsdb_reg)
					first_nonstatsdb_reg = gv_cur_region;
			} else
				all_files_open = FALSE;
		}
	}
	if (NULL == first_nonstatsdb_reg)
	{
		gv_cur_region = NULL;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBNOREGION);
	}
	/* Fill in db file name of statsdb regions now that basedb has been opened in above "for" loop */
	for (reg = gd_header->regions; reg < reg_top; reg++)
	{
		if (reg_cmcheck(reg))
			continue;
		if (!IS_STATSDB_REG(reg))
			continue;			/* Bypass statsDB files */
		STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
		if (baseDBreg->open)
		{
			baseDBcsa = &FILE_INFO(baseDBreg)->s_addrs;
			baseDBnl = baseDBcsa->nl;
			COPY_STATSDB_FNAME_INTO_STATSREG(reg, baseDBnl->statsdb_fname, baseDBnl->statsdb_fname_len);
			/* If a statsdb has already been created (by some other process), then we (DSE or LKE are the only
			 * ones that can reach here) will open the statsdb too. Otherwise we will not.
			 */
			assert(IS_DSE_IMAGE || IS_LKE_IMAGE);
			if (baseDBnl->statsdb_created && !reg->open)
			{
				gv_cur_region = reg;
				region_open();
				assert(reg->open);
			}
		}
	}
	gv_cur_region = first_nonstatsdb_reg;
	change_reg();
	return all_files_open;
}

void region_open(void)
{
	ESTABLISH(region_init_ch);
#ifdef UNIX
	gv_cur_region->node = -1;
#endif
	gv_init_reg(gv_cur_region, NULL);
	REVERT;
}
