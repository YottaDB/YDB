/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __WCS_CLEAN_DBSYNC_H__
#define __WCS_CLEAN_DBSYNC_H__

#include "gt_timer.h"
#include "have_crit.h"

void wcs_clean_dbsync(TID tid, int4 hd_len, sgmnt_addrs **csaptr);

#define	START_DBSYNC_TIMER(CSA, TIM_DEFER_DBSYNC)							\
{													\
	CSA->dbsync_timer = TRUE;									\
	CSA->dbsync_timer_tn = CSA->ti->curr_tn;							\
	start_timer((TID)CSA, TIM_DEFER_DBSYNC, &wcs_clean_dbsync, SIZEOF(CSA), (char *)&CSA);		\
}

#define	CANCEL_DBSYNC_TIMER(CSA)									\
{													\
	cancel_timer((TID)CSA);										\
	CSA->dbsync_timer = FALSE;									\
}
#endif
