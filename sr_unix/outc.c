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
#include "gtm_unistd.h"

#include <errno.h>

#include "io.h"
#include "iottdef.h"
#include "gtmio.h"

GBLREF io_pair io_curr_device;

#ifdef __sparc
int outc(char intch)
#else
int outc(int intch)
#endif
{
	char		ch;
	d_tt_struct	*tt_ptr;
	int		status;

	ch = intch;
	tt_ptr = (d_tt_struct*) io_curr_device.out->dev_sp;
	DOWRITERC(tt_ptr->fildes, &ch, 1, status);
	if(0 != status)
		rts_error(VARLSTCNT(1) status);

	return 0;
}
