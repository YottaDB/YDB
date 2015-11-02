/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * ------------------------------------------------------------------------------------------------
 * gtcmtr_lke_showreq : displays the lock tree received from a gtcm_server running on a remote node
 * used in            : lke_show.c
 * ------------------------------------------------------------------------------------------------
 */

#include "mdef.h"

#include "gtm_string.h"

#include <stddef.h>

#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "lke.h"
#include "cmi.h"
#include "util.h"
#include "gtcmtr_protos.h"
#include "iosp.h"
#include "gtcm_find_region.h"
#include "gvcmz.h"
#include "longcpy.h"

#define FLUSH	1

bool gtcmtr_lke_showreq(struct CLB *lnk, char rnum, bool all, bool wait, int4 pid, mstr *node)
{
	show_request	sreq;
	show_reply	srep;
	bool		locks = FALSE;
	uint4		status;

	sreq.code = CMMS_U_LKESHOW;
	sreq.rnum = rnum;
	sreq.all = all;
	sreq.wait = wait;
	sreq.pid = pid;
	assert(node && node->addr);
	sreq.nodelength = node->len;
	memcpy(sreq.node, node->addr, node->len);
	lnk->cbl = SIZEOF(sreq);
	lnk->mbf = (unsigned char *)&sreq;
	lnk->ast = NULL;
	status = cmi_write(lnk);
	if (CMI_ERROR(status))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		gvcmz_error(CMMS_U_LKESHOW, status);
		return FALSE;
	}
	lnk->mbl = SIZEOF(srep);
	lnk->mbf = (unsigned char *)&srep;
	for (;;)
	{
		status = cmi_read(lnk);
		if (CMI_ERROR(status))
		{
			((link_info *)(lnk->usr))->neterr = TRUE;
			gvcmz_error(CMMS_V_LKESHOW, status);
			return FALSE;
		}
		if (srep.code != CMMS_V_LKESHOW)
			return locks;
		util_out_print(srep.line, FLUSH);
		locks = TRUE;
	}
}

