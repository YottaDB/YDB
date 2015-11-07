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

#include <prvdef.h>
#include <ssdef.h>

#include "gtmsecshr.h"

uint4 gtm_getlkiw(uint4 efn, uint4 *lkid, void *itmlst, void *iosb, void *astadr, uint4 astprm, uint4 dummy)
{
	uint4	status;
	uint4	prvadr[2], prvprv[2];

	GTMSECSHR_SET_DBG_PRIV(PRV$M_SYSLCK, status);
	if (status == SS$_NORMAL)
	{
		status = sys$getlkiw(efn, lkid, itmlst, iosb, astadr, astprm, dummy);
		GTMSECSHR_REL_DBG_PRIV;
	}
	return status;
}
