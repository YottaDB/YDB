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
#include "cmmdef.h"
#include "error.h"

GBLREF struct NTD 		*ntd_root;
GBLREF connection_struct 	*curr_entry;
GBLREF int4 			gtcm_exi_condition;

CONDITION_HANDLER(gtcm_exi_handler)
{
	struct CLB	*p;
	void gtcm_exi_ch();

	gtcm_exi_condition = SIGNAL;
	ESTABLISH(gtcm_exi_ch);
	if (ntd_root)
	{	for ( p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl); p != (struct CLB *)ntd_root ;
			p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl))
		{
			curr_entry = (connection_struct*)(p->usr);
			gtcmtr_terminate(FALSE);
		}
	}
	REVERT;
	print_exit_stats();
	exit(gtcm_exi_condition);
}


void gtcm_exi_ch()
{
	exit(gtcm_exi_condition);
}
