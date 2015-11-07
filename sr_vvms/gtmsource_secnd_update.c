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

#if !defined(__MVS__) && !defined(VMS)
#include <sys/param.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include "gtm_string.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */
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
#include "error.h"
#include "gtm_stdio.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "util.h"

GBLREF jnlpool_addrs 	jnlpool;
GBLREF boolean_t	update_disable;

int gtmsource_secnd_update(boolean_t print_message)
{
	if (grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM) < 0)
	{
		util_out_print("Error grabbing jnlpool option write lock. Could not initiate change log", TRUE);
		return(ABNORMAL_SHUTDOWN);
	}
	grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
	jnlpool.jnlpool_ctl->upd_disabled = update_disable;
	rel_lock(jnlpool.jnlpool_dummy_reg);
	rel_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
	if (print_message)
		util_out_print("Updates are now !AZ", TRUE, update_disable ? "disabled" : "enabled");
	return(NORMAL_SHUTDOWN);
}
