/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_instance.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "util.h"
#include "cli.h"
#include "repl_log.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];

int gtmsource_losttncomplete(void)
{
	int			idx;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;
	sgmnt_addrs		*repl_csa;
	uint4			exit_status;

	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	assert(NULL == jnlpool.gtmsource_local);
	repl_log(stderr, TRUE, TRUE, "Initiating LOSTTNCOMPLETE operation on instance [%s]\n",
		jnlpool.repl_inst_filehdr->inst_info.this_instname);
	/* If this is a root primary instance, propagate this information to secondaries as well so they reset zqgblmod_seqno to 0.
	 * If propagating primary, no need to send this to tertiaries as the receiver on the tertiary cannot have started with
	 * non-zero "zqgblmod_seqno" to begin with (PRIMARYNOTROOT error would have been issued).
	 */
	if (!jnlpool.jnlpool_ctl->upd_disabled)
	{
		DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;)
		assert(!repl_csa->hold_onto_crit);	/* so it is ok to invoke "grab_lock" and "rel_lock" unconditionally */
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
		jnlpool.jnlpool_ctl->send_losttn_complete = TRUE;
		gtmsourcelocal_ptr = jnlpool.gtmsource_local_array;
		for (idx = 0; idx < NUM_GTMSRC_LCL; idx++, gtmsourcelocal_ptr++)
		{
			if (('\0' == gtmsourcelocal_ptr->secondary_instname[0])
					&& (0 == gtmsourcelocal_ptr->read_jnl_seqno)
					&& (0 == gtmsourcelocal_ptr->connect_jnl_seqno))
				continue;
			gtmsourcelocal_ptr->send_losttn_complete = TRUE;
		}
		rel_lock(jnlpool.jnlpool_dummy_reg);
	}
	/* Reset zqgblmod_seqno and zqgblmod_tn to 0 in this instance as well */
	exit_status = repl_inst_reset_zqgblmod_seqno_and_tn();
	if (SS_NORMAL != exit_status)
	{	/* Only reason we know of the above function returning -1 is in case of an online rollback. But, that's not
		 * possible because we hold the access control semaphore at this point which online rollback needs at startup.
		 * Also in case of gds_rundown failure the function returns EXIT_ERR which results in an assert failure here.
		 */
		assert(FALSE);
	}
	return (exit_status + NORMAL_SHUTDOWN);
}
