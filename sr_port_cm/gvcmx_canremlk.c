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

#include "mdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "stringpool.h"
#include "gvcmx.h"
#include "gvcmz.h"

GBLREF struct	NTD *ntd_root;
GBLREF spdesc	stringpool;

void gvcmx_canremlk(void)
{
	uint4		status, count, buffer;
	unsigned char	*ptr;
	struct CLB	*p;
	error_def(ERR_BADSRVRNETMSG);

	if (!ntd_root)
		return;
	buffer = 0;
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl); p != (struct CLB *)ntd_root ; p = (struct CLB *)RELQUE2PTR(p->cqe.fl))
	{
		if (((link_info*)(p->usr))->lck_info & REQUEST_PENDING)
			buffer += p->mbl;
	}
	ENSURE_STP_FREE_SPACE(buffer);
	ptr = stringpool.free;
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl); p != (struct CLB *)ntd_root; p = (struct CLB *)RELQUE2PTR(p->cqe.fl))
	{
		if (((link_info*)(p->usr))->lck_info & REQUEST_PENDING)
		{
			p->mbf = ptr;
			ptr += p->mbl;
		}
	}
	gvcmz_int_lkcancel();
}
