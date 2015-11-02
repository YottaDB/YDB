/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "error.h"
#include "util.h"
#ifdef UNIX
#include "ftok_sems.h"
GBLREF gd_region	*gv_cur_region;
#endif

GBLREF bool	region;
GBLREF uint4	mu_int_errknt;
GBLREF uint4	mu_int_skipreg_cnt;

error_def(ERR_ASSERT);
error_def(ERR_DBFILERR);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_TEXT);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(mu_int_reg_ch)
{
	START_CH
	mu_int_skipreg_cnt++;
	if (DUMPABLE)
		NEXTCH;
	PRN_ERROR;
	UNWIND(NULL,NULL);
}
