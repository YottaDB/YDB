/****************************************************************
 *								*
 *	Copyright 2002, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "cli.h"
#include "cli_parse.h"
#include "cli_disallow.h"
#include "mupip_cmd_disallow.h"

GBLREF	char	*cli_err_str_ptr;

boolean_t cli_disallow_mupip_journal(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;

	disallow_return_value =  !(d_c_cli_present("RECOVER") || d_c_cli_present("VERIFY") ||
		d_c_cli_present("SHOW") || d_c_cli_present("EXTRACT") || d_c_cli_present("ROLLBACK"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("RECOVER") && d_c_cli_present("ROLLBACK"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  !(d_c_cli_present("FORWARD") || d_c_cli_present("BACKWARD"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("FORWARD") && d_c_cli_present("BACKWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("SINCE") && d_c_cli_present("FORWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("LOOKBACK_LIMIT") && d_c_cli_present("FORWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("CHECKTN") && d_c_cli_present("BACKWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("RESYNC") && d_c_cli_present("FETCHRESYNC"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("RESYNC") || d_c_cli_present("FETCHRESYNC")) &&
		!(d_c_cli_present("ROLLBACK"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("LOSTTRANS") && !(d_c_cli_present("RECOVER") ||
		d_c_cli_present("ROLLBACK") || d_c_cli_present("EXTRACT"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("BROKENTRANS") && !(d_c_cli_present("RECOVER") ||
		d_c_cli_present("ROLLBACK") || d_c_cli_present("EXTRACT"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("FORWARD") && d_c_cli_present("ROLLBACK");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("FULL") && (d_c_cli_present("RECOVER") || d_c_cli_present("ROLLBACK"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("DETAIL") && !d_c_cli_present("EXTRACT"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("AFTER") && !d_c_cli_present("FORWARD"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("AFTER") && (d_c_cli_present("RECOVER") || d_c_cli_present("ROLLBACK")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("SINCE") && !d_c_cli_present("BACKWARD"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("LOOKBACK_LIMIT") && !d_c_cli_present("BACKWARD"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("LOOKBACK_LIMIT") && !(d_c_cli_present("VERIFY") ||
		d_c_cli_present("RECOVER") || d_c_cli_present("EXTRACT") || d_c_cli_present("SHOW")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("APPLY_AFTER_IMAGE") && !(d_c_cli_present("ROLLBACK") ||
		d_c_cli_present("RECOVER")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("REDIRECT") && !d_c_cli_present("RECOVER"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("REDIRECT") && !d_c_cli_present("FORWARD"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("BACKWARD") && d_c_cli_negated("CHAIN"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("CHECKTN") && d_c_cli_present("BACKWARD"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  ((d_c_cli_present("AFTER") || d_c_cli_present("BEFORE") ||
		d_c_cli_present("SINCE") || d_c_cli_present("LOOKBACK_LIMIT")) && d_c_cli_present("ROLLBACK"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  ((d_c_cli_present("GLOBAL") || d_c_cli_present("USER") ||
		d_c_cli_present("ID") || d_c_cli_present("TRANSACTION")) && !(d_c_cli_present("EXTRACT") ||
		d_c_cli_present("SHOW")));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}
/*=========================================================================*/
boolean_t cli_disallow_mupip_set(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;

	disallow_return_value =  cli_check_any2(VARLSTCNT(3)   d_c_cli_present("FILE"), d_c_cli_present("REGION"),
		d_c_cli_present("JNLFILE"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (!(d_c_cli_present("FILE") || d_c_cli_present("REGION") || d_c_cli_present("JNLFILE")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("JOURNAL.ON") && d_c_cli_present("JOURNAL.OFF"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("JOURNAL.DISABLE") &&
		(d_c_cli_present("JOURNAL.ON") 	|| d_c_cli_present("JOURNAL.OFF")           ||
		  d_c_cli_present("JOURNAL.ENABLE") 	||
		  d_c_cli_present("JOURNAL.BEFORE_IMAGES") || d_c_cli_negated("JOURNAL.BEFORE_IMAGES") ||
		  d_c_cli_present("JOURNAL.FILENAME") 	|| d_c_cli_present("JOURNAL.ALLOCATION")    ||
		  d_c_cli_present("JOURNAL.EXTENSION")	|| d_c_cli_present("JOURNAL.BUFFER_SIZE")   ||
		  d_c_cli_present("JOURNAL.ALIGNSIZE") 	|| d_c_cli_present("JOURNAL.EPOCH_INTERVAL") ||
		  d_c_cli_present("JOURNAL.AUTOSWITCHLIMIT")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (!(!d_c_cli_present("JOURNAL") || d_c_cli_present("DISABLE") || d_c_cli_present("OFF") ||
		d_c_cli_present("JOURNAL.BEFORE_IMAGES") || d_c_cli_negated("JOURNAL.BEFORE_IMAGES")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("REPLICATION.ON") && d_c_cli_present("REPLICATION.OFF"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("REPLICATION.ON") && (d_c_cli_present("JOURNAL.OFF") ||
		d_c_cli_present("JOURNAL.DISABLE") || d_c_cli_negated("JOURNAL") || d_c_cli_negated("JOURNAL.BEFORE_IMAGES")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("PREVJNLFILE") && !(d_c_cli_present("JNLFILE")));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}
/*=========================================================================*/
boolean_t cli_disallow_mupip_backup(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;

	disallow_return_value =  ((d_c_cli_present("TRANSACTION") || d_c_cli_present("SINCE")) &&
		!(d_c_cli_present("INCREMENTAL") || d_c_cli_present("BYTESTREAM")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (((d_c_cli_present("INCREMENTAL") || d_c_cli_present("BYTESTREAM")) &&
		(d_c_cli_present("COMPREHENSIVE") || d_c_cli_present("DATABASE"))) ||
		(d_c_cli_present("TRANSACTION") && d_c_cli_present("SINCE")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  ((d_c_cli_present("BKUPDBJNL.DISABLE")) && (d_c_cli_present("BKUPDBJNL.OFF")));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}
