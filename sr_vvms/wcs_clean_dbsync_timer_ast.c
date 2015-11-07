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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "efn.h"		/* for efn_ignore */

GBLREF	int4		defer_dbsync[2];
GBLREF	short		astq_dyn_avail;

/* wcs_clean_dbsync_ast() is not directly being called when the active queue becomes empty. This is because
 * we want to avoid syncing the database, in the case where it is actively being updated though frequently
 * getting emptied (by something other than a wcs_flu). Only in the case where there is prolonged update
 * inactivity after emptying the active queue do we sync the db. "prolonged" is defined by TIM_DEFER_DBSYNC.
 */
void	wcs_clean_dbsync_timer_ast(sgmnt_addrs *csa)
{
	int		status;
	void		wcs_clean_dbsync_ast();

	assert(lib$ast_in_prog());	/* If dclast fails and setast is used, this assert trips, but in that
					 * case, we anyway want to know why we needed setast. */
	assert(0 < astq_dyn_avail);
	if (0 >= astq_dyn_avail)
		csa->dbsync_timer = FALSE;
	/* Note that csa->dbsync_timer can be FALSE while entering this routine in case we had issued the dsk_write (sys$qio)
	 * of the last dirty cache-record and then went to gds_rundown() which resets the dbsync_timer to FALSE unconditionally.
	 * In this case, we need to return.
	 */
	if (FALSE == csa->dbsync_timer)
	{
		astq_dyn_avail++;
		return;
	}
	status = sys$setimr(efn_ignore, &defer_dbsync[0], wcs_clean_dbsync_ast, csa, 0);
	if (0 == (status & 1))
	{
		assert(FALSE);
		csa->dbsync_timer = FALSE;
		astq_dyn_avail++;	/* in this case too, we skip syncing the database */
	}
	return;
}
