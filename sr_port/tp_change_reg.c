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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"		/* needed for gtmsource.h */
#include "gtmsource.h"		/* needed for jnlpool_addrs typedef */
#include "tp_change_reg.h"

GBLREF gd_region 		*gv_cur_region;
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF sgmnt_addrs	 	*cs_addrs;
GBLREF jnlpool_addrs_ptr_t	jnlpool;

void
tp_change_reg(void)
{
	if (gv_cur_region)
	{
		switch (gv_cur_region->dyn.addr->acc_meth)
		{
		    case dba_mm:
		    case dba_bg:
			cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
			cs_data = cs_addrs->hdr;
			if (cs_addrs->jnlpool && (jnlpool != cs_addrs->jnlpool))
				jnlpool = cs_addrs->jnlpool;
			return;
		    case dba_usr:
		    case dba_cm:
			cs_addrs = (sgmnt_addrs *)0;
			cs_data = (sgmnt_data_ptr_t)0;
			return;
		    default:
			assertpro(gv_cur_region->dyn.addr->acc_meth != gv_cur_region->dyn.addr->acc_meth);
		}
	}

	cs_addrs = (sgmnt_addrs *)0;
	cs_data = (sgmnt_data_ptr_t)0;
	return;
}
