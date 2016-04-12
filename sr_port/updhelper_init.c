/****************************************************************
 *								*
 * Copyright (c) 2005-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"
#include "gtm_time.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_inet.h"

#include <sys/mman.h>
#include <errno.h>

#include "cdb_sc.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for muprec.h and tp.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "tp.h"
#include "iosp.h"
#include "gtmrecv.h"
#include "cli.h"
#include "error.h"
#include "repl_dbg.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_shutdcode.h"
#include "repl_sp.h"
#include "jnl_write.h"
#include "gtmio.h"

#include "util.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "targ_alloc.h"
#include "dpgbldir.h"
#include "read_db_files_from_gld.h"
#include "updproc.h"
#include "upd_open_files.h"
#include "tp_change_reg.h"
#include "wcs_flu.h"
#include "repl_log.h"
#include "tp_restart.h"
#include "gtmmsg.h"	/* for gtm_putmsg() prototype */
#include "mu_gv_stack_init.h"
#include "jnl_typedef.h"
#include "memcoherency.h"
#include "mupip_exit.h"
#include "getjobnum.h"

GBLREF	recvpool_addrs		recvpool;
GBLREF	upd_helper_entry_ptr_t	helper_entry;
GBLREF	uint4			process_id;
GBLREF	uint4			is_updhelper;
GBLREF	boolean_t		jnlpool_init_needed;

error_def(ERR_NOTALLDBOPN);
error_def(ERR_RECVPOOLSETUP);
error_def(ERR_REPLWARN);
error_def(ERR_TEXT);

void updhelper_init(recvpool_user who)
{
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	upd_helper_entry_ptr_t	helper, helper_top;

	is_updhelper = who;
	getjobnum();
	recvpool_init(UPD_HELPER_READER, FALSE);
	upd_log_init(who);
	upd_helper_ctl = recvpool.upd_helper_ctl;
	for (helper = upd_helper_ctl->helper_list, helper_top = helper + MAX_UPD_HELPERS; helper < helper_top; helper++)
	{
		if (helper->helper_pid_prev == process_id) /* found my entry */
		{
			helper->helper_type = who;
			helper->helper_pid = process_id; /* become owner of slot, tell receiver startup in now complete */
			break;
		}
	}
	OPERATOR_LOG_MSG;
	if (helper == helper_top)
	{ /* did not find my entry possibly due to startup directly from command line as opposed to the desired via-rcvr server */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				LEN_AND_LIT("Invalid startup, start helper via receiver server"));
	}
	helper_entry = helper;
	gvinit();
	jnlpool_init_needed = TRUE;
	if (!region_init(FALSE))
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTALLDBOPN);
	return;
}
