/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "mupipset.h"
#include "cli.h"
#include "iosp.h"
#include "error.h"
#include "mupint.h"
#include "util.h"
#include "mupipbckup.h"
#include "mupip_exit.h"
#include "mu_getlst.h"

GBLDEF	bool	region;
GBLREF	bool	error_mupip;

bool check_qual(bool file, bool reg, bool jnlfile);

void mupip_set(void)
{
	bool		file;
	char		db_fn[MAX_LINE], jnl_fn[MAX_LINE];
	unsigned short	db_fn_len, jnl_fn_len;
	uint4		status, journal, replication, jnlfile;

	error_def(ERR_MUNOACTION);
	error_def(ERR_MUNODBNAME);
	error_def(ERR_MUPCLIERR);

	file = cli_present("FILE") == CLI_PRESENT;
	region = cli_present("REGION") == CLI_PRESENT;
	jnlfile = cli_present("JNLFILE") == CLI_PRESENT;
	replication = cli_present("REPLICATION");

	if (check_qual(file, region, jnlfile))
	{
		util_out_print("Any one of REGION, FILE or JNLFILE qualifer should (and only) be specified", TRUE);
		mupip_exit(ERR_MUNODBNAME);
	}

	if (region)
	{
		gvinit();
		mu_getlst("WHAT", sizeof(mu_set_rlist));
		if (error_mupip)
			mupip_exit(ERR_MUNOACTION);
		db_fn_len = 0;
	} else if (file)
	{
		db_fn_len = sizeof(db_fn);
		memset(db_fn, 0, db_fn_len);
		if (!cli_get_str("WHAT", db_fn, &db_fn_len))
			mupip_exit(ERR_MUNODBNAME);
	} else
	{
		assert(jnlfile);
		jnl_fn_len = sizeof(jnl_fn);
		memset(jnl_fn, 0, jnl_fn_len);
		if (!cli_get_str("WHAT", jnl_fn, &jnl_fn_len))
			mupip_exit(ERR_MUNOACTION);
		status = mupip_set_jnl_file(jnl_fn);
		mupip_exit(status);
	}

	if (cli_present("ACCESS_METHOD")   	== CLI_PRESENT  ||
	    cli_present("EXTENSION_COUNT") 	== CLI_PRESENT  ||
	    cli_present("GLOBAL_BUFFERS")  	== CLI_PRESENT  ||
	    cli_present("RESERVED_BYTES")  	== CLI_PRESENT  ||
	    cli_present("FLUSH_TIME")      	== CLI_PRESENT  ||
	    cli_present("LOCK_SPACE")      	== CLI_PRESENT  ||
	    cli_present("DEFER_TIME")      	== CLI_PRESENT  ||
	    cli_present("WAIT_DISK")		== CLI_PRESENT  ||
	    cli_present("PARTIAL_RECOV_BYPASS")	== CLI_PRESENT)
		mupip_set_file(db_fn_len, db_fn);

	journal = cli_present("JOURNAL");

	if (CLI_PRESENT == journal  ||  CLI_NEGATED == journal  ||  CLI_PRESENT == replication  ||  CLI_NEGATED == replication)
		status = mupip_set_journal(journal == CLI_PRESENT, replication == CLI_PRESENT, db_fn_len, db_fn);
	else
		status = SS_NORMAL;

	mupip_exit(status);

}

bool check_qual(bool file, bool reg, bool jnlfile)
{
	if (!file && !reg && !jnlfile)
	{
		return TRUE;
	} else if (file)
	{
		if(reg || jnlfile)
			return TRUE;
	} else if (reg)
	{
		if (file || jnlfile)
			return TRUE;
	} else
	{
		if (file || reg)
			return TRUE;
	}
	return FALSE;
}
