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
#include "cmidef.h"
#include "gtcm_link_accept.h"


#ifdef VMS
GBLREF	short	gtcm_ast_avail;
#endif


bool gtcm_link_accept(struct CLB *lnk)
{
#if defined(VMS)
	if (gtcm_ast_avail > 0)
	{
		gtcm_ast_avail--;
		return TRUE;
	}
	return FALSE;
#elif defined(UNIX)
	return TRUE;
#else
#error "Unsupported platform"
#endif
}
