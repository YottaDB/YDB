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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "error.h"
#include "change_reg.h"

GBLREF gd_addr		*gd_header;
GBLREF gd_region	*gv_cur_region;

void region_open(void);

error_def (ERR_DBNOREGION);

boolean_t region_init(bool cm_regions)
{
	gd_region		*region_top;
	boolean_t		file_open, is_cm, all_files_open;

	file_open = FALSE;
	all_files_open = TRUE;
	region_top = gd_header->regions + gd_header->n_regions;
	for (gv_cur_region = gd_header->regions; gv_cur_region < region_top; gv_cur_region++)
	{
		if (gv_cur_region->open == FALSE
			&& (gv_cur_region->dyn.addr->acc_meth == dba_bg || gv_cur_region->dyn.addr->acc_meth == dba_mm))
		{
			is_cm = reg_cmcheck(gv_cur_region);
			if (!is_cm || cm_regions)
			{
				region_open();
				if (gv_cur_region->open)
					file_open = TRUE;
				else
					all_files_open = FALSE;
			}
		}
	}
	if (!file_open)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBNOREGION);

	/* arbitrary assignment of the first region */
	for (gv_cur_region = gd_header->regions; gv_cur_region < region_top; gv_cur_region++)
	{
		if (gv_cur_region->open)
		{
			change_reg();
			break;
		}
	}
	return all_files_open;
}

void region_open(void)
{
	ESTABLISH(region_init_ch);
#ifdef UNIX
	gv_cur_region->node = -1;
#endif
	gv_init_reg(gv_cur_region);
	REVERT;
}
