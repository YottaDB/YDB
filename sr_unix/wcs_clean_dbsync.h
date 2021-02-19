/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef WCS_CLEAN_DBSYNC_H_INCLUDED
#define WCS_CLEAN_DBSYNC_H_INCLUDED

#include "gt_timer.h"
#include "have_crit.h"

void wcs_clean_dbsync(TID tid, int4 hd_len, sgmnt_addrs **csaptr);

#define	START_DBSYNC_TIMER(CSA, TIM_DEFER_DBSYNC)							\
{													\
	GBLREF	boolean_t	exit_handler_active;							\
													\
	if (!exit_handler_active)									\
	{												\
		CSA->dbsync_timer = TRUE;								\
		CSA->dbsync_timer_tn = CSA->ti->curr_tn;						\
		start_timer((TID)CSA, TIM_DEFER_DBSYNC, &wcs_clean_dbsync, SIZEOF(CSA), (char *)&CSA);	\
	}												\
	/* else: we have already started exit processing. It is not safe to start a timer (YDB#679) */	\
}

#define	CANCEL_DBSYNC_TIMER(CSA)									\
{													\
	assert(CSA->dbsync_timer);									\
	cancel_timer((TID)CSA);										\
	CSA->dbsync_timer = FALSE;									\
}
#endif
