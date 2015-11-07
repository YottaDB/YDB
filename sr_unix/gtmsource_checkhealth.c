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
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "repl_sp.h"
#include "repl_log.h"
#include "is_proc_alive.h"
#include "gtmmsg.h"
#include "sgtm_putmsg.h"
#include "util.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	gd_addr			*gd_header;

error_def(ERR_NOTALLDBOPN);
error_def(ERR_REPLJNLCLOSED);
error_def(ERR_SRCSRVNOTEXIST);

int gtmsource_checkhealth(void)
{
	uint4			gtmsource_pid;
	int			status, semval, save_errno;
	boolean_t		srv_alive, all_files_open;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;
	int4			index, num_servers;
	seq_num			reg_seqno, jnlseqno;
	gd_region		*reg, *region_top;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	char			errtxt[OUT_BUFF_SIZE];
	char			*modestr;

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
			if (GTMSOURCE_MODE_ACTIVE == gtmsourcelocal_ptr->mode)
				modestr = "ACTIVE";
			else if (GTMSOURCE_MODE_ACTIVE_REQUESTED == gtmsourcelocal_ptr->mode)
				modestr = "ACTIVE REQUESTED";
			else if (GTMSOURCE_MODE_PASSIVE == gtmsourcelocal_ptr->mode)
				modestr = "PASSIVE";
			else if (GTMSOURCE_MODE_PASSIVE_REQUESTED == gtmsourcelocal_ptr->mode)
				modestr = "PASSIVE REQUESTED";
			else
			{
				assert(gtmsourcelocal_ptr->mode != gtmsourcelocal_ptr->mode);
				modestr = "UNKNOWN";
			}
			repl_log(stderr, FALSE, TRUE, FORMAT_STR1, gtmsource_pid, "Source server", "", modestr);
			status |= SRV_ALIVE;
			num_servers++;
		} else
		{
			repl_log(stderr, FALSE, TRUE, FORMAT_STR, gtmsource_pid, "Source server", " NOT");
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SRCSRVNOTEXIST, 2,
					LEN_AND_STR(gtmsourcelocal_ptr->secondary_instname));
			status |= SRV_DEAD;
		}
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
			save_errno = errno;
			repl_log(stderr, FALSE, TRUE,
				"Error fetching source server count semaphore value : %s\n", STRERROR(save_errno));
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
	/* Check that there are no regions with replication state = WAS_ON (i.e. repl_was_open). If so report that.
	 * But to determine that, we need to attach to all the database regions.
	 */
	gvinit();
	/* We use the same code dse uses to open all regions but we must make sure they are all open before proceeding. */
	all_files_open = region_init(FALSE);
	if (!all_files_open)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTALLDBOPN);
		status |= SRV_ERR;
	} else
	{
		for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
		{
			csa = &FILE_INFO(reg)->s_addrs;
			csd = csa->hdr;
			if (REPL_WAS_ENABLED(csd))
			{
				assert(!JNL_ENABLED(csd) || REPL_ENABLED(csd));	/* || is for turning replication on concurrently */
				reg_seqno = csd->reg_seqno;
				jnlseqno = (NULL != jnlpool.jnlpool_ctl) ? jnlpool.jnlpool_ctl->jnl_seqno : MAX_SEQNO;
				sgtm_putmsg(errtxt, VARLSTCNT(8) ERR_REPLJNLCLOSED, 6, DB_LEN_STR(reg),
					&reg_seqno, &reg_seqno, &jnlseqno, &jnlseqno);
				repl_log(stderr, FALSE, TRUE, errtxt);
				status |= SRV_ERR;
			}
		}
	}
	if (jnlpool.jnlpool_ctl->freeze)
	{
		repl_log(stderr, FALSE, FALSE, "Warning: Instance Freeze is ON\n");
		repl_log(stderr, FALSE, TRUE, "   Freeze Comment: %s\n", jnlpool.jnlpool_ctl->freeze_comment);
		status |= SRV_ERR;
	}
	return (status + NORMAL_SHUTDOWN);
}
