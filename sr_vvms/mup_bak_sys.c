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
#include <ssdef.h>
#include <descrip.h>
#include "efn.h"
#include <dvidef.h>
#include "util.h"

GBLREF mstr directory;
GBLREF int4 mubmaxblk;
GBLREF bool error_mupip;

void mup_bak_mag(void)
{
	int4		item_code;
	$DESCRIPTOR(dir,"");
	uint4	devbufsiz;

	item_code = DVI$_DEVBUFSIZ;
	dir.dsc$a_pointer = directory.addr;
	dir.dsc$w_length =  directory.len;
	devbufsiz = 0;
	lib$getdvi(&item_code, 0, &dir, &devbufsiz, 0, 0);
	if (devbufsiz < mubmaxblk + 8)
	{
		util_out_print("!/Buffer size !UL may not accomodate maximum GDS block size of !UL.", FALSE,
			devbufsiz, mubmaxblk - 4);
		util_out_print("!/4 bytes/GDS block + 8 bytes/tape block in overhead required for device.", FALSE);
		util_out_print("!/MUPIP cannot start backup with above errors!/",TRUE);
		error_mupip = TRUE;
	}
	return;
}

void mup_bak_pause(void)
{
	int4	pause[2];

	pause[0] = 2 * -10000000;
	pause[1] = -1;
	if (sys$setimr( efn_immed_wait, &pause, 0, 0, 0) == SS$_NORMAL)	/* Safety wait to make sure that all blocks have been */
	{	sys$synch(efn_immed_wait, 0);				/* returned to the frozen queues before flushing      */
	}

	return;
}
