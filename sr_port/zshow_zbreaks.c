/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 52a92dfd (GT.M V7.0-001)
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
<<<<<<< HEAD

=======
#include "gtm_string.h"
#include <rtnhdr.h>
>>>>>>> 52a92dfd (GT.M V7.0-001)
#include "zbreak.h"
#include "zshow.h"
#include "compiler.h"

GBLREF z_records	zbrk_recs;

/* dig up and format information on current ZBREAKs for zshow_output to present as results for ZSHOW "B" or "*" */

void zshow_zbreaks(zshow_out *output)
{
	zbrk_struct	*z_ptr;
	mval		zbreak;
	char		zbreakbuff[MAX_ENTRYREF_LEN + MAX_SRCLINE + 1];

	if (zbrk_recs.beg == zbrk_recs.free)
		return;
	assert(NULL != zbrk_recs.beg);
	assert(NULL != zbrk_recs.free);
	assert(NULL != zbrk_recs.end);

	output->flush = TRUE;
	zbreak.mvtype = MV_STR;
	zbreak.str.addr = zbreakbuff;
	for (z_ptr = zbrk_recs.beg; z_ptr < zbrk_recs.free; z_ptr++)
	{
		zbreak.str.len = INTCAST(rtnlaboff2entryref(zbreakbuff, z_ptr->rtn, z_ptr->lab, z_ptr->offset) - zbreakbuff);
		zbreak.str.addr[zbreak.str.len++] = '>';
		memcpy(&zbreak.str.addr[zbreak.str.len], z_ptr->action->src.str.addr, z_ptr->action->src.str.len);
		zbreak.str.len += z_ptr->action->src.str.len;
		output->flush = TRUE;
		zshow_output(output, &zbreak.str);
	}
	return;
}
