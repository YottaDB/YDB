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
#include "error.h"
#include "util.h"
#ifdef UNIX
#include "ftok_sems.h"
GBLREF gd_region	*gv_cur_region;
#endif

GBLREF bool	region;
GBLREF uint4	mu_int_errknt;

CONDITION_HANDLER(mu_int_reg_ch)
{
	error_def(ERR_TEXT);
	error_def(ERR_DBFILERR);
	START_CH

#ifdef UNIX
	if (!region)
		db_ipcs_reset(gv_cur_region, TRUE);
#endif
	mu_int_errknt++;
	PRN_ERROR;
	UNWIND(NULL,NULL);
}
