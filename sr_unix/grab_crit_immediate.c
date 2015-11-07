/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>	/* for VSIG_ATOMIC_T type */

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
#include "wcs_recover.h"
#include "caller_id.h"
#include "is_proc_alive.h"
#include "gtmimagename.h"
#include "error.h"

GBLREF	short 			crash_count;
GBLREF	volatile int4		crit_count;
GBLREF	uint4 			process_id;
GBLREF	node_local_ptr_t	locknl;
GBLREF	boolean_t		mupip_jnl_recover;

error_def(ERR_CRITRESET);
error_def(ERR_DBCCERR);
error_def(ERR_DBFLCORRP);

boolean_t grab_crit_immediate(gd_region *reg)
{
	unix_db_info 		*udi;
	sgmnt_addrs  		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	enum cdb_sc		status;
	mutex_spin_parms_ptr_t	mutex_spin_parms;

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
		status = mutex_lockwim(reg, mutex_spin_parms, crash_count);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (status != cdb_sc_normal)
		{
			crit_count = 0;
			switch (status)
			{
				case cdb_sc_nolock:
					return(FALSE);
				case cdb_sc_critreset:
					rts_error(VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
				case cdb_sc_dbccerr:
					rts_error(VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
				default:
					GTMASSERT;
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
		CRIT_TRACE(crit_ops_gw);		/* see gdsbt.h for comment on placement */
		crit_count = 0;
	}
	else
		assert(FALSE);
	if (csd->file_corrupt && !mupip_jnl_recover)
	{
		if (!IS_DSE_IMAGE)
			rts_error(VARLSTCNT(4) ERR_DBFLCORRP, 2, DB_LEN_STR(reg));
		else
			gtm_putmsg(VARLSTCNT(4) MAKE_MSG_WARNING(ERR_DBFLCORRP), 2, DB_LEN_STR(reg));
	}
	/* Ideally we do not want to do wcs_recover if we are in interrupt code (as opposed to mainline code).
	 * This is easily accomplished in VMS with a library function lib$ast_in_prog but in Unix there is no way
	 * to tell mainline code from interrupt code without the caller providing that information. Hence we
	 * currently do the cache recovery even in case of interrupt code even though it is a heavyweight operation.
	 * If it is found to cause issues, this logic has to be re-examined.
	 */
	if (cnl->wc_blocked)
		wcs_recover(reg);
	return(TRUE);
}

