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
#include <jpidef.h>
#include "vmsdtype.h"
#include "efn.h"
#include "job.h"

void ojdefbaspri (int4 *baspri)
{
	int4		status;
	unsigned short	ret;
	short		iosb[4];
	struct
	{
		item_list_3	le[1];
		int4		terminator;
	}		item_list;

	item_list.le[0].buffer_length		= sizeof *baspri;
	item_list.le[0].item_code		= JPI$_PRIB;
	item_list.le[0].buffer_address		= baspri;
	item_list.le[0].return_length_address	= &ret;
	item_list.terminator			= 0;

	status = sys$getjpi (0, 0, 0, &item_list, &iosb[0], 0, 0);
	if (!(status & 1))	rts_error(VARLSTCNT(1) status);
	sys$synch (efn_immed_wait, &iosb[0]);
	if (!(iosb[0] & 1))	rts_error(VARLSTCNT(1) iosb[0]);
	return;
}
