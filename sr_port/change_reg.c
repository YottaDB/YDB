/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "repl_msg.h"		/* needed for gtmsource.h */
#include "gtmsource.h"		/* needed for jnlpool_addrs typedef */

GBLREF gd_region        	*gv_cur_region;
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF sgmnt_addrs      	*cs_addrs;
GBLREF uint4			dollar_tlevel;
GBLREF jnlpool_addrs_ptr_t	jnlpool;

inline void change_reg(void)
{
	if ((NULL == gv_cur_region) || (FALSE == gv_cur_region->open))
	{
		cs_addrs = (sgmnt_addrs *)0;
		cs_data = (sgmnt_data_ptr_t)0;
		return;
	}
	if (IS_REG_BG_OR_MM(gv_cur_region))
	{
		cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
		cs_data = cs_addrs->hdr;
		if (cs_addrs->jnlpool && (jnlpool != cs_addrs->jnlpool))
			jnlpool = cs_addrs->jnlpool;
		if (dollar_tlevel)
			tp_set_sgm();
	} else
	{
		assert(REG_ACC_METH(gv_cur_region) == dba_cm);
		cs_addrs = (sgmnt_addrs *)0;
		cs_data = (sgmnt_data_ptr_t)0;
	}
}

