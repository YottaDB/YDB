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
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gvcmz.h"

GBLREF int 		zdef_sent, zdef_rcv;
GBLREF struct CLB	*zdeferr;

void gvcmz_zdefw_ast(struct CLB *lnk)
{
	uint4 status;

	if (CMI_CLB_ERROR(lnk))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		zdeferr = lnk;
		zdef_rcv++;
	} else
	{
		lnk->ast = gvcmz_zdefr_ast;
		status = cmi_read(lnk);
		if (CMI_ERROR(status))
		{
			((link_info *)(lnk->usr))->neterr = TRUE;
			zdeferr = lnk;
			zdef_rcv++;
		}
	}
}

void gvcmz_zdefr_ast(struct CLB *lnk)
{
	if (CMI_CLB_ERROR(lnk))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		zdeferr = lnk;
	} else if (*lnk->mbf != CMMS_C_BUFFLUSH)
		zdeferr = lnk;
	zdef_rcv++;
}
