/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_signal.h"		/* for VSIG_ATOMIC_T type */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "filestruct.h"
#include "send_msg.h"
#include "mutex.h"
#include "deferred_signal_handler.h"
#include "caller_id.h"
#include "is_proc_alive.h"
#include "gtmimagename.h"
#include "error.h"
#include "wcs_recover.h"

GBLREF	short 			crash_count;
GBLREF	volatile int4		crit_count;
GBLREF	uint4 			process_id;
GBLREF	node_local_ptr_t	locknl;
#ifdef DEBUG
GBLREF	boolean_t		mupip_jnl_recover;
#endif

error_def(ERR_CRITRESET);
error_def(ERR_DBCCERR);
error_def(ERR_DBFLCORRP);

boolean_t grab_crit_immediate(gd_region *reg, boolean_t ok_for_wcs_recover)
{
	unix_db_info 		*udi;
	sgmnt_addrs  		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	enum cdb_sc		status;
	mutex_spin_parms_ptr_t	mutex_spin_parms;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	if (!csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;	/* prevent interrupts */
		DEBUG_ONLY(locknl = cnl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		mutex_spin_parms = (mutex_spin_parms_ptr_t)&csd->mutex_spin_parms;
		status = gtm_mutex_lock(reg, mutex_spin_parms, crash_count, MUTEX_LOCK_WRITE_IMMEDIATE);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (status != cdb_sc_normal)
		{
			crit_count = 0;
			switch (status)
			{
				case cdb_sc_nolock:
					return(FALSE);
				case cdb_sc_critreset:
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
				case cdb_sc_dbccerr:
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
				default:
					/* An out-of-design return value. assertpro(FALSE) but spell out the failing condition
					 * details instead of just FALSE. Hence the complex structure below.
					 */
					assertpro((cdb_sc_nolock != status) && (cdb_sc_critreset != status)
						&& (cdb_sc_dbccerr != status) && FALSE);
			}
			return(FALSE);
		}
		/* There is only one case we know of when cnl->in_crit can be non-zero and that is when a process holding
		 * crit gets kill -9ed and another process ends up invoking "secshr_db_clnup" which in turn clears the
		 * crit semaphore (making it available for waiters) but does not also clear cnl->in_crit since it does not
		 * hold crit at that point. But in that case, the pid reported in cnl->in_crit should be dead. Check that.
		 */
		assert((0 == cnl->in_crit) || (FALSE == is_proc_alive(cnl->in_crit, 0)));
		cnl->in_crit = process_id;
		CRIT_TRACE(csa, crit_ops_gw);		/* see gdsbt.h for comment on placement */
		crit_count = 0;
	}
	else
		assert(FALSE);
	/* Commands/Utilties that plays with the file_corrupt flags (DSE/MUPIP SET -PARTIAL_RECOV_BYPASS/RECOVER/ROLLBACK) should
	 * NOT issue DBFLCORRP. Use skip_file_corrupt_check global variable for this purpose. Ideally we need this check only
	 * in grab_crit.c and not grab_crit_immediate.c as all the above commands/utilities only go to grab_crit and do not come
	 * here but we keep it the same for consistency.
	 */
	/* Assert that MUPIP RECOVER/ROLLBACK has TREF(skip_file_corrupt_check)=TRUE and so does not issue DBFLCORRP error */
	assert(!mupip_jnl_recover || TREF(skip_file_corrupt_check));
	if (csd->file_corrupt && !TREF(skip_file_corrupt_check))
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFLCORRP, 2, DB_LEN_STR(reg));
	if (cnl->wc_blocked)
	{
		if (ok_for_wcs_recover)
			wcs_recover(reg);
		else
		{	/* Caller says it is not okay to call "wcs_recover" so return as if crit could not be obtained right away.
			 * These callers are periodic/cyclical and interrupt-based so they will retry later if needed
			 * (e.g. wcs_wtstart and wcs_clean_dbsync).
			 */
			rel_crit(reg);
			return(FALSE);
		}
	}
	return(TRUE);
}

