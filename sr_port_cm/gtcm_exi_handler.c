/****************************************************************
 *								*
 * Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "locklits.h"
#include "error.h"
#include "print_exit_stats.h"
#include "gtcmtr_protos.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "gtcm_exi_handler.h"

#define EXICONDITION exi_condition

GBLREF	struct NTD		*ntd_root;
GBLREF	connection_struct	*curr_entry;
GBLREF	int4			EXICONDITION;
GBLREF	uint4			process_id;
GBLREF	boolean_t		exit_handler_active;
GBLREF	boolean_t		exit_handler_complete;


error_def(ERR_UNKNOWNFOREX);
error_def(ERR_GTCMEXITLOOP);

void gtcm_exi_handler()
{
	struct CLB	*p, *pn;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (exit_handler_active)
		return;
	exit_handler_active = TRUE;
	ASSERT_IS_LIBGNPSERVER;
	ESTABLISH(gtcm_exi_ch);
	if (ntd_root)
	{	/* Need a way to detect cycles in the loop below (C9C02-001908) */
		for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl); (p != (struct CLB *)ntd_root); p = pn)
		{	/* Get the forward link, in case a close removes the current entry */
			pn = (struct CLB *)RELQUE2PTR(p->cqe.fl);
			curr_entry = (connection_struct*)(p->usr);
			gtcmtr_terminate(FALSE);
		}
	}
	exit_handler_complete = TRUE;
	print_exit_stats();
	PROCDIE(EXICONDITION);
}
