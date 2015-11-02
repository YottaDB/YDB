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
 * ---------------------------------------------------------------------------------------------------------
 * gtcmtr_lke_clearreq : displays the cleared lock tree received from a gtcm_server running on a remote node
 * used in	       : lke_clear.c
 * ---------------------------------------------------------------------------------------------------------
 */

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include <stddef.h>

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

#define FLUSH		1
#define LOCK_CLEAR_CONF	"Clear lock? "

GBLREF	short			crash_count;

bool gtcmtr_lke_clearreq(struct CLB *lnk, char rnum, bool all, bool interactive, int4 pid, mstr	*node)
{
	clear_confirm	conf;
	clear_reply	crep;
	clear_request	creq;
	show_reply	srep;
	bool		locks = FALSE, removed;
	uint4		status;
	char		res;
	int4		fao[2];

	error_def(ERR_LCKSGONE);

	creq.code = CMMS_U_LKEDELETE;
	creq.rnum = rnum;
	creq.all = all;
	creq.interactive = interactive;
	creq.pid = pid;
	creq.nodelength = node->len;
	memcpy(creq.node, node->addr, node->len);
	lnk->cbl = SIZEOF(creq);
	lnk->mbf = (unsigned char *)&creq;
	lnk->ast = NULL;
	status = cmi_write(lnk);
	if (CMI_ERROR(status))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		gvcmz_error(CMMS_U_LKEDELETE, status);
		return FALSE;
	}
	if (interactive)
	{
		removed = FALSE;
		for (;;)
		{
			lnk->mbl = SIZEOF(srep);
			lnk->mbf = (unsigned char *)&srep;
			status = cmi_read(lnk);
			if (CMI_ERROR(status))
			{
				((link_info *)(lnk->usr))->neterr = TRUE;
				gvcmz_error(CMMS_V_LKESHOW, status);
				return FALSE;
			}
			if (srep.code != CMMS_V_LKESHOW)
			{
				if (removed)
					gtm_putmsg(VARLSTCNT(1) ERR_LCKSGONE);
				return locks;
			}
			util_out_print(srep.line, FLUSH);
			locks = TRUE;
			util_out_print(LOCK_CLEAR_CONF, FALSE);
			res = '\0';
			res = getchar();
			conf.clear = ('y' == res || 'Y' == res);
			removed |= conf.clear;
			lnk->cbl = SIZEOF(conf);
			lnk->mbf = (unsigned char *)&conf;
			status = cmi_write(lnk);
			if (CMI_ERROR(status))
			{
				((link_info *)(lnk->usr))->neterr = TRUE;
				gvcmz_error(CMMS_U_LKEDELETE, status);
				return FALSE;
			}
		}
	} else
	{
		lnk->mbl = SIZEOF(crep);
		lnk->mbf = (unsigned char *)&crep;
		for (;;)
		{
			status = cmi_read(lnk);
			if (CMI_ERROR(status))
			{
				((link_info *)(lnk->usr))->neterr = TRUE;
				gvcmz_error(CMMS_V_LKESHOW, status);
				return FALSE;
			}
			if (crep.code != CMMS_V_LKESHOW)
				return locks;
			gtm_putmsg(VARLSTCNT(3) crep.status, crep.locknamelength, crep.lockname);
			locks = TRUE;
		}
	}
}
