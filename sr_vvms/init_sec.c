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

#include <descrip.h>
#include <prvdef.h>
#include <psldef.h>
#include <secdef.h>
#include <ssdef.h>

#include "gtmsecshr.h"

uint4	init_sec(uint4 *retadr, struct dsc$descriptor_s	*gsdnam, uint4 chan, uint4 pagcnt, uint4 flags)
{
	uint4	inadr[2], status;
	uint4	prvadr[2], prvprv[2];

	GTMSECSHR_SET_PRIV((PRV$M_SYSGBL | PRV$M_PRMGBL), status);
	if (SS$_NORMAL == status)
	{
		inadr[0] = retadr[0];
		inadr[1] = retadr[1];
		status = sys$crmpsc(inadr, retadr, PSL$C_USER, flags, gsdnam, NULL, 0,
				    flags & SEC$M_PAGFIL ? 0 : chan,
				    pagcnt, 0, 0, 0);
		GTMSECSHR_REL_PRIV;
	}
	return status;
}
