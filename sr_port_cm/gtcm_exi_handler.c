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

#include <unistd.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "locklits.h"
#include "error.h"
#include "print_exit_stats.h"
#include "gtcmtr_terminate.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "gtcm_exi_handler.h"

#ifdef VMS
#  define EXICONDITION gtcm_exi_condition
#else
#  define EXICONDITION exi_condition
#endif

GBLREF	struct NTD		*ntd_root;
GBLREF	connection_struct	*curr_entry;
GBLREF	int4			EXICONDITION;
GBLREF	uint4			process_id;

#define MAX_GTCM_EXI_LOOPCNT	4096

void gtcm_exi_handler()
{
	int 		lcnt;
	struct CLB	*p, *pn;
	error_def(ERR_UNKNOWNFOREX);
	error_def(ERR_GTCMEXITLOOP);

	ESTABLISH(gtcm_exi_ch);
	if (ntd_root)
	{
		for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl), lcnt = 0;
			(p != (struct CLB *)ntd_root) && (MAX_GTCM_EXI_LOOPCNT > lcnt); p = pn, lcnt++)
		{
			/* Get the forward link, in case a close removes the current entry */
			pn = (struct CLB *)RELQUE2PTR(p->cqe.fl);
			curr_entry = (connection_struct*)(p->usr);
			gtcmtr_terminate(FALSE);
		}
		if (MAX_GTCM_EXI_LOOPCNT <= lcnt)
			send_msg(VARLSTCNT(1) ERR_GTCMEXITLOOP);
	}
	print_exit_stats();
	VMS_ONLY(
		if (0 == EXICONDITION)
		        EXICONDITION = ERR_UNKNOWNFOREX;
		);
	PROCDIE(EXICONDITION);
}
