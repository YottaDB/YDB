/****************************************************************
 *								*
 * Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "io.h"
#include "iosp.h"
#include "collseq.h"
#include "error.h"
#include "ydb_trans_log_name.h"

int find_local_colltype(void)
{
	int	lct, status;
	char	transbuf[MAX_TRANS_NAME_LEN];
	mstr	transnam;

	status = ydb_trans_log_name(YDBENVINDX_LOCAL_COLLATE, &transnam, transbuf, SIZEOF(transbuf),
										IGNORE_ERRORS_TRUE, NULL);
	if (SS_NORMAL != status)
		return 0;
	lct = asc2i((uchar_ptr_t)transnam.addr, transnam.len);
	return lct >= MIN_COLLTYPE && lct <= MAX_COLLTYPE ? lct : 0;
}

collseq *ready_collseq(int act)
{
	collseq		temp_csp, *csp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Validate the alternative collating type (act) */
	if (!(act >= 1 && act <= MAX_COLLTYPE))
		return (collseq*)NULL;
	/* Search for record of the collating type already being mapped in. */
	for (csp = TREF(collseq_list); csp != NULL && act != csp->act; csp = csp->flink)
		;
	if (NULL == csp)
	{
		/* If not found, create a structure and attempt to map in the collating support package.*/
		temp_csp.act = act;
		temp_csp.flink = TREF(collseq_list);
		if (!map_collseq(act, &temp_csp))
			return NULL;
		csp = (collseq *) malloc(SIZEOF(collseq));
		*csp = temp_csp;
		TREF(collseq_list) = csp;
	}
	return csp;
}
