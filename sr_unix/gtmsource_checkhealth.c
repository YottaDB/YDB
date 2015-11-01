/****************************************************************
 *								*
 *	Copyright 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_inet.h"

#include <sys/time.h>
#include <errno.h>
#ifdef UNIX
#include <sys/sem.h>
#endif
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "repl_sp.h"
#include "repl_log.h"
#include "is_proc_alive.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];

int gtmsource_checkhealth(void)
{
	uint4			gtmsource_pid;
	int			status, semval;
	boolean_t		srv_alive;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;
	int4			index, num_servers;

	assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	if (NULL != jnlpool.gtmsource_local)	/* Check health of a specific source server */
		gtmsourcelocal_ptr = jnlpool.gtmsource_local;
	else
		gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[0];
	num_servers = 0;
	status = SRV_ALIVE;
	for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++)
	{
		if ('\0' == gtmsourcelocal_ptr->secondary_instname[0])
		{
			assert(NULL == jnlpool.gtmsource_local);
			continue;
		}
		gtmsource_pid = gtmsourcelocal_ptr->gtmsource_pid;
		/* If CHECKHEALTH on a specific secondary instance is requested, print the health information irrespective
		 * of whether a source server for that instance is alive or not. For CHECKHEALTH on ALL secondary instances
		 * print health information only for those instances that have an active or passive source server alive.
		 */
		if ((NULL == jnlpool.gtmsource_local) && (0 == gtmsource_pid))
			continue;
		repl_log(stdout, TRUE, TRUE, "Initiating CHECKHEALTH operation on source server pid [%d] for secondary instance"
			" name [%s]\n", gtmsource_pid, gtmsourcelocal_ptr->secondary_instname);
		srv_alive = (0 == gtmsource_pid) ? FALSE : is_proc_alive(gtmsource_pid, 0);
		if (srv_alive)
		{
			repl_log(stderr, FALSE, TRUE, FORMAT_STR1, gtmsource_pid, "Source server", "",
				((GTMSOURCE_MODE_ACTIVE == gtmsourcelocal_ptr->mode) ? "ACTIVE" : "PASSIVE"));
			status |= SRV_ALIVE;
			num_servers++;
		} else if (!srv_alive)
		{
			repl_log(stderr, FALSE, TRUE, FORMAT_STR, gtmsource_pid, "Source server", " NOT");
			status |= SRV_DEAD;
		} else
			status |= SRV_ERR;
		if (NULL != jnlpool.gtmsource_local)
			break;
	}
	if (NULL == jnlpool.gtmsource_local)
	{	/* Compare number of servers that were found alive with the current value of the COUNT semaphore.
		 * If they are not equal, report the discrepancy.
		 */
		semval = get_sem_info(SOURCE, SRC_SERV_COUNT_SEM, SEM_INFO_VAL);
		if (-1 == semval)
		{
			repl_log(stderr, FALSE, TRUE,
				"Error fetching source server count semaphore value : %s\n", REPL_SEM_ERROR);
			status |= SRV_ERR;
		} else if (semval != num_servers)
		{
			repl_log(stderr, FALSE, FALSE,
				"Error : Expected %d source server(s) to be alive but found %d actually alive\n",
				semval, num_servers);
			repl_log(stderr, FALSE, TRUE, "Error : Check if any pid reported above is NOT a source server process\n");
			status |= SRV_ERR;
		}
	}
	return (status + NORMAL_SHUTDOWN);
}
