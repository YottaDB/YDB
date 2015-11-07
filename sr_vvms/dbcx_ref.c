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

/* This routine takes a pointer to a sgmnt_data struct with space for the BTs and
	locks attached.  The BTs and the lock space are initialized and then written
	to disk to the file specified by channel.
*/

#include "mdef.h"
#include <rms.h>
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include <ssdef.h>
#include <iodef.h>
#include <efndef.h>
#include "mlk_shr_init.h"
#include "dbcx_ref.h"


int dbcx_ref(sgmnt_data *sd, int chan)
{
	char		*qio_ptr, *qio_top;
	short		iosb[4];
	int		block, status;
	sgmnt_addrs	sa;


	sa.hdr = sd;
	bt_malloc(&sa);
	mlk_shr_init((char *)sd + (LOCK_BLOCK(sd) * DISK_BLOCK_SIZE), sd->lock_space_size, &sa, TRUE);
	qio_ptr = (char *)sd;
	qio_top = qio_ptr + (LOCK_BLOCK(sd) * DISK_BLOCK_SIZE) + LOCK_SPACE_SIZE(sd);
	for ( block = 1; qio_ptr < qio_top; block++, qio_ptr += DISK_BLOCK_SIZE)
	{
		if (SS$_NORMAL != (status = sys$qiow(EFN$C_ENF, chan, IO$_WRITEVBLK, iosb,
							0, 0, qio_ptr, DISK_BLOCK_SIZE, block, 0, 0, 0)))
			return status;
		if (!(iosb[0] & 1))
			return iosb[0];
	}
	return SS$_NORMAL;
}
