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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "mupip_set.h"
#include "cli.h"
#include "iosp.h"
#include "error.h"
#include "mupint.h"
#include "util.h"
#include "mupipbckup.h"
#include "mupip_exit.h"
#include "mu_getlst.h"

GBLDEF	bool		region;
GBLREF	bool		error_mupip;
#ifdef UNIX
GBLREF	boolean_t	jnlpool_init_needed;
#endif

error_def(ERR_MUNOACTION);
error_def(ERR_MUNODBNAME);

void mupip_set(void)
{
	boolean_t	file, jnlfile;
	char		db_fn[MAX_FN_LEN + 1], jnl_fn[JNL_NAME_SIZE];
	unsigned short	db_fn_len, jnl_fn_len;
	uint4		status;
	int 		cli_stat;
	boolean_t	set_journal, set_replication;

	UNIX_ONLY(jnlpool_init_needed = TRUE);
	file = cli_present("FILE") == CLI_PRESENT;
	region = cli_present("REGION") == CLI_PRESENT;
	jnlfile = cli_present("JNLFILE") == CLI_PRESENT;
	cli_stat =  cli_present("JOURNAL"); /* just save a call to cli_present */
	set_journal = ((CLI_PRESENT == cli_stat) || (CLI_NEGATED == cli_stat));
	cli_stat =  cli_present("REPLICATION"); /* just save a call to cli_present */
	set_replication = ((CLI_PRESENT == cli_stat) || (CLI_NEGATED == cli_stat));
	if (region)
	{
		gvinit();
		mu_getlst("WHAT", SIZEOF(mu_set_rlist));
		if (error_mupip)
			mupip_exit(ERR_MUNOACTION);
		db_fn_len = 0;
	} else if (file)
	{
		db_fn_len = SIZEOF(db_fn);
		memset(db_fn, 0, db_fn_len);
		if (!cli_get_str("WHAT", db_fn, &db_fn_len))
			mupip_exit(ERR_MUNODBNAME);
	} else
	{
		assert(jnlfile);
		jnl_fn_len = SIZEOF(jnl_fn);
		memset(jnl_fn, 0, jnl_fn_len);
		if (!cli_get_str("WHAT", jnl_fn, &jnl_fn_len))
			mupip_exit(ERR_MUNOACTION);
		status = mupip_set_jnlfile(jnl_fn, SIZEOF(jnl_fn));
		mupip_exit(status);
	}
	if (cli_present("ACCESS_METHOD")   	== CLI_PRESENT  ||
#	    ifdef UNIX
	    cli_present("INST_FREEZE_ON_ERROR")	== CLI_PRESENT  ||
	    cli_present("INST_FREEZE_ON_ERROR") == CLI_NEGATED  ||
#	    endif
	    cli_present("EXTENSION_COUNT") 	== CLI_PRESENT  ||
	    cli_present("GLOBAL_BUFFERS")  	== CLI_PRESENT  ||
	    cli_present("RESERVED_BYTES")  	== CLI_PRESENT  ||
	    cli_present("FLUSH_TIME")      	== CLI_PRESENT  ||
	    cli_present("LOCK_SPACE")      	== CLI_PRESENT  ||
	    cli_present("MUTEX_SLOTS")      	== CLI_PRESENT  ||
	    cli_present("DEFER_TIME")      	== CLI_PRESENT  ||
	    cli_present("WAIT_DISK")		== CLI_PRESENT  ||
	    cli_present("PARTIAL_RECOV_BYPASS")	== CLI_PRESENT  ||
#	    ifdef UNIX
	    cli_present("KEY_SIZE")		== CLI_PRESENT  ||
	    cli_present("QDBRUNDOWN")		== CLI_PRESENT  ||
	    cli_present("QDBRUNDOWN")		== CLI_NEGATED  ||
	    cli_present("RECORD_SIZE")		== CLI_PRESENT  ||
#	    endif
	    cli_present("VERSION")		== CLI_PRESENT)
	{
		if (SS_NORMAL != (status = mupip_set_file(db_fn_len, db_fn)))
			mupip_exit(status);
	}
	if (set_journal || set_replication)
		status = mupip_set_journal(db_fn_len, db_fn);
	else
		status = SS_NORMAL;
	mupip_exit(status);
}
