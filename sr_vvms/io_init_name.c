/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "outofband.h"
#include "io.h"
#include "gtm_logicals.h"

#define mstr_set_str(x,y) ((x)->addr = (y), (x)->len = SIZEOF(y) - 1)

GBLDEF uint4	std_dev_outofband_msk;
GBLDEF mstr		sys_input;
GBLDEF mstr		sys_output;
GBLDEF mstr		gtm_principal;

void io_init_name(void)
{
	uint4   disable_mask;

	disable_mask = CTRLY_MSK ;
	lib$disable_ctrl(&disable_mask, &std_dev_outofband_msk);
	std_dev_outofband_msk &= CTRLY_MSK;
	mstr_set_str(&gtm_principal, GTM_PRINCIPAL);
	mstr_set_str(&sys_input,"SYS$INPUT");
	mstr_set_str(&sys_output ,"SYS$OUTPUT");
}
