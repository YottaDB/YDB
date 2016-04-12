/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "change_reg.h"
#include "tp_set_sgm.h"

GBLREF gd_region        *gv_cur_region;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF uint4		dollar_tlevel;

void change_reg(void)
{
	if (!gv_cur_region)
	{
		cs_addrs = (sgmnt_addrs *)0;
		cs_data = (sgmnt_data_ptr_t)0;
		return;
	}

	switch (gv_cur_region->dyn.addr->acc_meth)
	{
		case dba_usr:
		case dba_cm:
			cs_addrs = (sgmnt_addrs *)0;
			cs_data = (sgmnt_data_ptr_t)0;
			break;
		case dba_mm:
		case dba_bg:
			cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
			cs_data = cs_addrs->hdr;
			if (dollar_tlevel)
				tp_set_sgm();
			break;
		default:
			GTMASSERT;
	}
}

