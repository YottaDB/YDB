/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * ------------------------------------------------------------------------------------------------
 * gtcmtr_lke_showrep : sends a lock tree to the requesting node, one lock at a time
 * used in            : gtcm_server.c
 * ------------------------------------------------------------------------------------------------
 */

#include "mdef.h"

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

#define RESET	2

GBLREF	connection_struct	*curr_entry;
GBLREF	gd_region		*gv_cur_region;
GBLREF	short			crash_count;

char gtcmtr_lke_showrep(struct CLB *lnk, show_request *sreq)
{
	gd_region		*cur_region;
	sgmnt_addrs		*cs_adr;
	mlk_ctldata		*lke_ctl;
	ssize_t			ls_len;
	mstr 			dnode;
	show_reply		srep;
	uint4			status;
	boolean_t		was_crit;

	cur_region = gv_cur_region = gtcm_find_region(curr_entry, sreq->rnum)->reghead->reg;
	if (dba_bg == cur_region->dyn.addr->acc_meth || dba_mm == cur_region->dyn.addr->acc_meth)
	{
		cs_adr = &FILE_INFO(cur_region)->s_addrs;
		ls_len = cs_adr->lock_addrs[1] - cs_adr->lock_addrs[0];
		lke_ctl = (mlk_ctldata *)malloc(ls_len);
		/* Prevent any modification of the lock space while we make a local copy of it */
		if (cs_adr->critical != NULL)
			crash_count = cs_adr->critical->crashcnt;
		was_crit = cs_adr->now_crit;
		if (!was_crit)
			grab_crit(cur_region);
		longcpy((uchar_ptr_t)lke_ctl, cs_adr->lock_addrs[0], ls_len);
		if (!was_crit)
			rel_crit(cur_region);
		util_cm_print(lnk, 0, NULL, RESET);
		dnode.len = sreq->nodelength;
		dnode.addr = sreq->node;
		if (lke_ctl->blkroot != 0)
			(void)lke_showtree(lnk, (mlk_shrblk_ptr_t)R2A(lke_ctl->blkroot), sreq->all, sreq->wait, sreq->pid, dnode,
					   FALSE, NULL);
		free(lke_ctl);
	}
	srep.code = CMMS_U_LKESHOW;
	lnk->cbl = SIZEOF(srep.code);
	lnk->ast = NULL;
#ifndef vinu_marker
	assert(0 == offsetof(show_reply, code));
	lnk->mbf = (unsigned char *)&srep; /* no need since lnk->mbf can be re-used. vinu 06/27/01 */
	status = cmi_write(lnk);
	if (CMI_ERROR(status))
	{ /* This routine is a server routine; not sure why it does error processing similar to a client. vinu 06/27/01 */
		((link_info *)(lnk->usr))->neterr = TRUE;
		gvcmz_error(CMMS_U_LKESHOW, status);
	} else
		lnk->mbf = (unsigned char *)sreq; /* don't restore if lnk->mbf isn't modified. vinu 06/27/01 */
#else
	/* server calls to cmi_* should do error processing as a callback. vinu 06/27/01 */
	*lnk->mbf = srep.code;
	cmi_write(lnk);
#endif
	return CM_READ;
}
