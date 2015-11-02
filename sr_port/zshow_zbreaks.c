/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>
#include "zbreak.h"
#include "zshow.h"

GBLREF z_records	zbrk_recs;

void zshow_zbreaks(zshow_out *output)
{
	zbrk_struct	*z_ptr;
	mval		entryref;
	char		entryrefbuff[MAX_ENTRYREF_LEN];

	if (zbrk_recs.beg == zbrk_recs.free)
		return;
	assert(NULL != zbrk_recs.beg);
	assert(NULL != zbrk_recs.free);
	assert(NULL != zbrk_recs.end);

	entryref.mvtype = MV_STR;
	for (z_ptr = zbrk_recs.beg; z_ptr < zbrk_recs.free; z_ptr++)
	{
		entryref.str.len = INTCAST(rtnlaboff2entryref(entryrefbuff, z_ptr->rtn, z_ptr->lab, z_ptr->offset) - entryrefbuff);
		entryref.str.addr = entryrefbuff;
		output->flush = TRUE;
		zshow_output(output, &entryref.str);
	}
	return;
}
