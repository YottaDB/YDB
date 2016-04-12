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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "util.h"
#include "cli.h"
#include "repl_log.h"
#include "repl_instance.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF 	gtmsource_options_t	gtmsource_options;

error_def(ERR_MUPCLIERR);

int gtmsource_needrestart(void)
{
	gtmsource_local_ptr_t	gtmsource_local;
	sgmnt_addrs		*repl_csa;

	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);

	gtmsource_local = jnlpool.gtmsource_local;
	if (NULL != gtmsource_local)
	{
		assert(!STRCMP(gtmsource_options.secondary_instname, gtmsource_local->secondary_instname));
		repl_log(stderr, TRUE, TRUE,
			"Initiating NEEDRESTART operation on source server pid [%d] for secondary instance [%s]\n",
			gtmsource_local->gtmsource_pid, gtmsource_options.secondary_instname);
	} else
		repl_log(stderr, TRUE, TRUE, "Initiating NEEDRESTART operation for secondary instance [%s]\n",
			gtmsource_options.secondary_instname);
	DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;)
	assert(!repl_csa->hold_onto_crit);	/* so it is ok to invoke "grab_lock" and "rel_lock" unconditionally */
	grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
	if ((NULL != gtmsource_local) && (gtmsource_local->connect_jnl_seqno >= jnlpool.jnlpool_ctl->start_jnl_seqno))
		util_out_print("Secondary Instance [!AZ] DOES NOT NEED to be restarted", TRUE, gtmsource_local->secondary_instname);
	else
		util_out_print("Secondary Instance [!AZ] NEEDS to be restarted first", TRUE, gtmsource_options.secondary_instname);
	rel_lock(jnlpool.jnlpool_dummy_reg);
	return (NORMAL_SHUTDOWN);
}
