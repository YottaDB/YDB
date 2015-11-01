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
#include "format_targ_key.h"
#include "sgnl.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;

#define LCL_BUF_SIZE 256

void sgnl_gvundef(void)
{
	unsigned char	buff[LCL_BUF_SIZE], *end;
	error_def(ERR_GVUNDEF);

	if ((end = format_targ_key(&buff[0], LCL_BUF_SIZE, gv_currkey, TRUE)) == 0)
	{	end = &buff[LCL_BUF_SIZE - 1];
	}
	rts_error(VARLSTCNT(4) ERR_GVUNDEF, 2, end - &buff[0], &buff[0]);
}
