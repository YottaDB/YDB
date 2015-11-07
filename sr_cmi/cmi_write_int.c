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

#include <iodef.h>
#include <efndef.h>
#include "cmihdr.h"
#include "cmidef.h"

uint4 cmi_write_int(struct CLB *lnk)
{
	int4 status;
        qio_iosb  iosb;

        /* Note:  there is an outstanding read so use unique efn and iosb */
        status = sys$qiow(EFN$C_ENF, lnk->dch, IO$_WRITEVBLK | IO$M_INTERRUPT,
                &iosb, 0, 0, lnk->mbf, lnk->cbl, 0, 0, 0, 0);
	if (status & 1)
                status = iosb.status;
	return status;
}
