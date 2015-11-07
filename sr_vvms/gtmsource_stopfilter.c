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
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "util.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;

int gtmsource_stopfilter(void)
{
	/* Grab the jnlpool jnlpool option write lock */
	if (0 > grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM))
	{
		util_out_print("Error grabbing jnlpool option write lock. Could not stop filter", TRUE);
		return (ABNORMAL_SHUTDOWN);
	}

	if ('\0' == jnlpool.gtmsource_local->filter_cmd[0])
	{
		util_out_print("No filter currently active", TRUE);
		rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);
		return (ABNORMAL_SHUTDOWN);
	}

	jnlpool.gtmsource_local->filter_cmd[0] = '\0';

	util_out_print("Stop filter initiated", TRUE);

	rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);

	return (NORMAL_SHUTDOWN);
}
