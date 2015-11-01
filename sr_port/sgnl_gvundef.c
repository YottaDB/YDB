/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "format_targ_key.h"
#include "sgnl.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;

void sgnl_gvundef(void)
{
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	error_def(ERR_GVUNDEF);

	if ((end = format_targ_key(&buff[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE)) == 0)
	{	end = &buff[MAX_ZWR_KEY_SZ - 1];
	}
	rts_error(VARLSTCNT(4) ERR_GVUNDEF, 2, end - &buff[0], &buff[0]);
}
