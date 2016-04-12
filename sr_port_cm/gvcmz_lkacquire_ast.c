/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gvcmz.h"
#include "cmi.h"

GBLREF unsigned short	lkresponse_count;
GBLREF unsigned char	lkerror;
GBLREF struct CLB	*lkerrlnk;

void gvcmz_lkacquire_ast(struct CLB *lnk)
{
	if (CMI_CLB_ERROR(lnk))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		lkerror = CMMS_L_LKACQUIRE;
		lkerrlnk = lnk;
	}
	lnk->ast = 0;
}
