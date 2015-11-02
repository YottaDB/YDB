/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "mu_gv_cur_reg_init.h"
#include "db_ipcs_reset.h"

GBLREF bool		region;
GBLREF gd_region	*gv_cur_region;

CONDITION_HANDLER(mu_int_ch)
{
	START_CH
	if (!region)
	{
		db_ipcs_reset(gv_cur_region);
		mu_gv_cur_reg_free(); /* mu_gv_cur_reg_init was done in mu_int_init() */
	}
	NEXTCH;
}
