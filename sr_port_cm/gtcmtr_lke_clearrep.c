/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>

#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "mlkdef.h"
#include "cmi.h"
#include "util.h"
#include "iosp.h"
#include "gtcmtr_protos.h"
#include "gvcmz.h"
#include "gtmmsg.h"
#include "gtcm_find_region.h"
#include "lke_cleartree.h"
#include "interlock.h"
#include "rel_quant.h"

#define RESET		2

GBLREF	connection_struct	*curr_entry;
GBLREF	gd_region		*gv_cur_region;
GBLREF	short			crash_count;

char gtcmtr_lke_clearrep(struct CLB *lnk, clear_request	*creq)
{
	gd_region		*cur_region;
	sgmnt_addrs		*csa;
	mlk_ctldata_ptr_t	lke_ctl;
	mstr			dnode;
	show_reply		srep;
	uint4			status;
	boolean_t		was_crit;

	cur_region = gv_cur_region = gtcm_find_region(curr_entry, creq->rnum)->reghead->reg;
	if (IS_REG_BG_OR_MM(cur_region))
	{
		csa = &FILE_INFO(cur_region)->s_addrs;
		lke_ctl = (mlk_ctldata_ptr_t)csa->lock_addrs[0];
		util_cm_print(lnk, 0, NULL, RESET);
		dnode.len = creq->nodelength;
		dnode.addr = creq->node;
		GRAB_LOCK_CRIT(csa, gv_cur_region, was_crit);
		if (lke_ctl->blkroot != 0)
			/* Remote lock clears are not supported, so LKE CLEAR -EXACT qualifier will not be supported on GT.CM.*/
			lke_cleartree(cur_region, lnk, lke_ctl, (mlk_shrblk_ptr_t)R2A(lke_ctl->blkroot), creq->all,
				      creq->interactive, creq->pid, dnode, FALSE);
		REL_LOCK_CRIT(csa, gv_cur_region, was_crit);
	}
	srep.code = CMMS_U_LKEDELETE;
	lnk->cbl = SIZEOF(srep.code);
	lnk->ast = NULL;
#ifndef vinu_marker
	assert(0 == offsetof(show_reply, code));
	lnk->mbf = (unsigned char *)&srep; /* no need since lnk->mbf can be re-used. vinu 06/27/01 */
	status = cmi_write(lnk);
	if (CMI_ERROR(status))
	{ /* This routine is a server routine; not sure why it does error processing similar to a client. vinu 06/27/01 */
		((link_info *)(lnk->usr))->neterr = TRUE;
		gvcmz_error(CMMS_U_LKEDELETE, status);
	} else
		lnk->mbf = (unsigned char *)creq; /* don't restore if lnk->mbf isn't modified. vinu 06/27/01 */
#else
	/* server calls to cmi_* should do error processing as a callback. vinu 06/27/01 */
	*lnk->mbf = srep.code;
	cmi_write(lnk);
#endif
	return CM_READ;
}
