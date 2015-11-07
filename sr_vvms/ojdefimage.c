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
#include "job.h"
#include "efn.h"

void ojdefimage (mstr *image)
{
	static mstr imagebuf = {0, 0};
	int4 status;
	unsigned char local_buff[MAX_FILSPC_LEN];
	short iosb[4];
	unsigned short length;
	struct
	{
		item_list_3	le[1];
		int4		terminator;
	}		item_list;

	if (!imagebuf.addr)
	{
		item_list.le[0].buffer_length = MAX_FILSPC_LEN;
		item_list.le[0].item_code = JPI$_IMAGNAME;
		item_list.le[0].buffer_address = local_buff;
		item_list.le[0].return_length_address = &length;
		item_list.terminator = 0;
		status = sys$getjpi (0, 0, 0, &item_list, &iosb[0], 0, 0);
		if (!(status & 1))
			rts_error(VARLSTCNT(1) status);
		sys$synch (efn_immed_wait, &iosb[0]);
		if (!(iosb[0] & 1))
			rts_error(VARLSTCNT(1) iosb[0]);
		imagebuf.addr = malloc(length);
		imagebuf.len = length;
		memcpy(imagebuf.addr, local_buff, length);
	}
	*image = imagebuf;
	return;
}
