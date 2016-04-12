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
#include "iosp.h"

GBLREF unsigned short	lkresponse_count;
GBLREF unsigned char	lkerror;
GBLREF struct CLB	*lkerrlnk;
GBLREF int		lkcancel_count;

void gvcmz_lkcancel_ast(struct CLB *lnk)
{
	uint4 status;

	if (CMI_CLB_ERROR(lnk))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		lkerror = CMMS_L_LKCANCEL;
		lkerrlnk = lnk;
		return;
	}
	if (*(lnk->mbf) == CMMS_M_LKDELETED ||
	    *(lnk->mbf) == CMMS_M_LKABORT ||         /* per spec */
	    *(lnk->mbf) == CMMS_L_LKCANCEL)
	{      /* above are end of conversation */
		lnk->ast = 0;
		lkcancel_count++;
	} else if (*(lnk->mbf) == CMMS_M_LKSUSPENDED || *(lnk->mbf) == CMMS_M_LKGRANTED || *(lnk->mbf) == CMMS_M_LKBLOCKED)
	{      /* above are response from prior message, ours is to come */
		status = cmi_read(lnk);
		if (CMI_ERROR(status))
		{
			((link_info *)(lnk->usr))->neterr = TRUE;
			lkerror = CMMS_L_LKCANCEL;
			lkerrlnk = lnk;
		} else
		        lkresponse_count++;
	} else
	{
		lkerror = CMMS_L_LKCANCEL;
		lkerrlnk = lnk;
	}
}
