/****************************************************************
 *								*
 *	Copyright 2006, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/time.h>
#include <errno.h>
#include "gtm_inet.h"	/* Required for gtmsource.h */
#include "gtm_string.h"
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
GBLREF	gd_addr			*gd_header;

int gtmsource_checkhealth(void)
{
	uint4			gtmsource_pid;
	int			status, semval;
	boolean_t		srv_alive, all_files_open;
	seq_num			reg_seqno, jnlseqno;
	gd_region		*reg, *region_top;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	char			errtxt[OUT_BUFF_SIZE];

	error_def(ERR_NOTALLDBOPN);
	error_def(ERR_REPLJNLCLOSED);

	REPL_DPRINT1("Checking health of Source Server\n");

	/* Grab the jnlpool option write lock */
	if (0 > grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM))
	{
		repl_log(stderr, FALSE, TRUE,
			 "Error grabbing jnlpool option write lock : %s. Could not check health of Source Server\n",
			 REPL_SEM_ERROR);
		return (SRV_ERR + NORMAL_SHUTDOWN);
	}

	gtmsource_pid = jnlpool.gtmsource_local->gtmsource_pid;
	if (0 < gtmsource_pid)
	{
		if (srv_alive = is_proc_alive(gtmsource_pid, 0))
			semval = get_sem_info(SOURCE, SRC_SERV_COUNT_SEM, SEM_INFO_VAL);
		if (srv_alive && 1 == semval)
		{
			repl_log(stderr, FALSE, TRUE, FORMAT_STR, gtmsource_pid, "Source server", "");
			status = SRV_ALIVE;
		} else if (!srv_alive || 0 == semval)
		{
			repl_log(stderr, FALSE, TRUE, FORMAT_STR, gtmsource_pid, "Source server", " NOT");
			if (srv_alive)
				repl_log(stderr, FALSE, TRUE, "Another process exists with same pid");
			status = SRV_DEAD;
		} else
		{
			repl_log(stderr, FALSE, TRUE,
				"Error in source server semval : %s. Could not check health of Source Server\n", REPL_SEM_ERROR);
			status = SRV_ERR;
		}
	} else
	{
		if (NO_SHUTDOWN == jnlpool.gtmsource_local->shutdown)
			repl_log(stderr, FALSE, TRUE, "Source Server crashed during journal pool initialization\n");
		else
			repl_log(stderr, FALSE, TRUE, "Source server crashed during shutdown\n");
		status = SRV_DEAD;
	}
	rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
	/* Check that there are no regions with replication state = WAS_ON (i.e. repl_was_open). If so report that.
	 * But to determine that, we need to attach to all the database regions.
	 */
	gvinit();
	/* We use the same code dse uses to open all regions but we must make sure they are all open before proceeding. */
	all_files_open = region_init(FALSE);
	if (!all_files_open)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_NOTALLDBOPN);
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
	return (status + NORMAL_SHUTDOWN);
}
