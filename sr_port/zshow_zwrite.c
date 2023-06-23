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
#include "zwrite.h"
#include "mlkdef.h"
#include "zshow.h"
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"

GBLREF uint4 pat_everything[];
GBLREF mstr_len_t sizeof_pat_everything;

LITREF mval literal_one;

void zshow_zwrite(zshow_out *output)
{
	mval pat;

	zshow_output(output,0);
	pat.mvtype = MV_STR;
	pat.str.addr = (char *)&pat_everything[0];
	pat.str.len = INTCAST(sizeof_pat_everything - 1);
	/* ZWRITE_ASTERISK is equivalent to literal_one */
	op_lvpatwrite(VARLSTCNT(3) (UINTPTR_T)output, &pat, &literal_one);
	return;
}
