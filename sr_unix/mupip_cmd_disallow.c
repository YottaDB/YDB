/****************************************************************
 *								*
 *	Copyright 2002, 2012 Fidelity Information Services, Inc.*
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

boolean_t cli_disallow_mupip_backup(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value =  (d_c_cli_present("TRANSACTION") || d_c_cli_present("SINCE"))
					&& !(d_c_cli_present("INCREMENTAL") || d_c_cli_present("BYTESTREAM"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("INCREMENTAL") || d_c_cli_present("BYTESTREAM"))
					&& (d_c_cli_present("COMPREHENSIVE") || d_c_cli_present("DATABASE"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("TRANSACTION") && d_c_cli_present("SINCE");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("BKUPDBJNL.DISABLE") && d_c_cli_present("BKUPDBJNL.OFF");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("REPLICATION.OFF");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("REPLICATION.ON") && d_c_cli_negated("NEWJNLFILES"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_freeze(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value = !d_c_cli_present("ON") && !d_c_cli_present("OFF");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("RECORD") && !d_c_cli_present("ON");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("OVERRIDE") && !d_c_cli_present("OFF");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("ON") && d_c_cli_present("OFF");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_integ(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value = d_c_cli_present("BRIEF") && d_c_cli_present("FULL");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("FILE") && d_c_cli_present("REGION");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("TN_RESET") && (d_c_cli_present("FAST")
								|| d_c_cli_present("BLOCK")
								|| d_c_cli_present("SUBSCRIPT")
								|| d_c_cli_present("REGION"));
	CLI_DIS_CHECK_N_RESET;
	/* -ONLINE and -FILE/-TN_RESET is incompatible as the former requires shared memory and the latter requires
	 * standalone access
	 */
	disallow_return_value = d_c_cli_present("ONLINE") && (d_c_cli_present("TN_RESET")
								|| d_c_cli_present("FILE"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_journal(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value =  !(d_c_cli_present("RECOVER")
					|| d_c_cli_present("VERIFY")
					|| d_c_cli_present("SHOW")
					|| d_c_cli_present("EXTRACT")
					|| d_c_cli_present("ROLLBACK"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("RECOVER") && d_c_cli_present("ROLLBACK");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  !(d_c_cli_present("FORWARD") || d_c_cli_present("BACKWARD"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("FORWARD") && d_c_cli_present("BACKWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("SINCE") && d_c_cli_present("FORWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("LOOKBACK_LIMIT") && d_c_cli_present("FORWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("REDIRECT") && !d_c_cli_present("RECOVER");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("CHECKTN") && d_c_cli_present("BACKWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("RESYNC") && d_c_cli_present("FETCHRESYNC");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("RESYNC") || d_c_cli_present("FETCHRESYNC"))
					&& !d_c_cli_present("ROLLBACK");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("RSYNC_STRM") && !d_c_cli_present("RESYNC");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("LOSTTRANS") && !(d_c_cli_present("RECOVER")
									|| d_c_cli_present("ROLLBACK")
									|| d_c_cli_present("EXTRACT"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("BROKENTRANS") && !(d_c_cli_present("RECOVER")
									|| d_c_cli_present("ROLLBACK")
									|| d_c_cli_present("EXTRACT"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("FORWARD") && d_c_cli_present("ROLLBACK");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("FULL") && (d_c_cli_present("RECOVER") || d_c_cli_present("ROLLBACK"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("DETAIL") && !d_c_cli_present("EXTRACT");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("AFTER") && !d_c_cli_present("FORWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("AFTER") && (d_c_cli_present("RECOVER") || d_c_cli_present("ROLLBACK"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("SINCE") && !d_c_cli_present("BACKWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("LOOKBACK_LIMIT") && !d_c_cli_present("BACKWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("LOOKBACK_LIMIT") && !(d_c_cli_present("VERIFY")
									|| d_c_cli_present("RECOVER")
									|| d_c_cli_present("EXTRACT")
									|| d_c_cli_present("SHOW"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("APPLY_AFTER_IMAGE") && !(d_c_cli_present("ROLLBACK")
										|| d_c_cli_present("RECOVER"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("REDIRECT") && !d_c_cli_present("RECOVER");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("REDIRECT") && !d_c_cli_present("FORWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("BACKWARD") && d_c_cli_negated("CHAIN");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("CHECKTN") && d_c_cli_present("BACKWARD");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  d_c_cli_present("ROLLBACK") && (d_c_cli_present("AFTER")
									|| d_c_cli_present("BEFORE")
									|| d_c_cli_present("SINCE")
									|| d_c_cli_present("LOOKBACK_LIMIT"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("GLOBAL")
					|| d_c_cli_present("USER")
					|| d_c_cli_present("ID")
					|| d_c_cli_present("TRANSACTION")) && (d_c_cli_present("RECOVER")
										|| d_c_cli_present("ROLLBACK")
										|| d_c_cli_present("VERIFY"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("ONLINE") && !d_c_cli_present("ROLLBACK"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_reorg(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value = (d_c_cli_present("SELECT")
				|| d_c_cli_present("EXCLUDE")
				|| d_c_cli_present("FILL_FACTOR")
				|| d_c_cli_present("INDEX_FILL_FACTOR")
				|| d_c_cli_present("RESUME")
				|| d_c_cli_present("USER_DEFINED_REORG")
				|| d_c_cli_present("TRUNCATE")) && (d_c_cli_present("UPGRADE")
									|| d_c_cli_present("DOWNGRADE"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("UPGRADE") && d_c_cli_present("DOWNGRADE");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("UPGRADE") || d_c_cli_present("DOWNGRADE")) && !d_c_cli_present("REGION");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("SAFEJNL")
				|| d_c_cli_negated("SAFEJNL")
				|| d_c_cli_present("STARTBLK")
				|| d_c_cli_present("STOPBLK")) && !(d_c_cli_present("UPGRADE")
									|| d_c_cli_present("DOWNGRADE"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_replicate(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3, p4, p5, p6;

	*cli_err_str_ptr = 0;
	p1 = d_c_cli_present("EDITINSTANCE");
	p2 = d_c_cli_present("INSTANCE_CREATE");
	p3 = d_c_cli_present("RECEIVER");
	p4 = d_c_cli_present("SOURCE");
	p5 = d_c_cli_present("UPDATEPROC");
	p6 = d_c_cli_present("UPDHELPER");

	/* any MUPIP REPLIC command should contain at LEAST one of the above qualifiers */
	disallow_return_value = !(p1 || p2 || p3 || p4 || p5 || p6);
	CLI_DIS_CHECK;	/* Note CLI_DIS_CHECK_N_RESET is not used as we want to reuse the computed error string (cli_err_str_ptr)
			 * for the next check as well in case it fails. Note that this can be done only if both checks use
			 * exactly the same set of qualifiers (which is TRUE in this case). */

	/* any MUPIP REPLIC command should contain at MOST one of the above qualifiers */
	disallow_return_value = cli_check_any2(VARLSTCNT(6) p1, p2, p3, p4, p5, p6);
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_replic_editinst(void)
{
	int		disallow_return_value = 0;

	*cli_err_str_ptr = 0;

	/* any MUPIP REPLIC -EDITINSTANCE command should contain one of CHANGE or SHOW or NAME */
	disallow_return_value = !(d_c_cli_present("CHANGE") || d_c_cli_present("SHOW") || d_c_cli_present("NAME"));
	CLI_DIS_CHECK_N_RESET;
	/* CHANGE and SHOW are mutually exclusive */
	disallow_return_value = (d_c_cli_present("CHANGE") && d_c_cli_present("SHOW"));
	CLI_DIS_CHECK_N_RESET;
	/* CHANGE and NAME are mutually exclusive */
	disallow_return_value = (d_c_cli_present("CHANGE") && d_c_cli_present("NAME"));
	CLI_DIS_CHECK_N_RESET;
	/* SHOW and NAME are mutually exclusive */
	disallow_return_value = (d_c_cli_present("SHOW") && d_c_cli_present("NAME"));
	CLI_DIS_CHECK_N_RESET;
	/* OFFSET, SIZE and VALUE is compatible only with CHANGE */
	disallow_return_value = (!d_c_cli_present("CHANGE")
				&& (d_c_cli_present("OFFSET") || d_c_cli_present("SIZE") || d_c_cli_present("VALUE")));
	CLI_DIS_CHECK_N_RESET;
	/* OFFSET and SIZE have to be present when CHANGE is specified */
	disallow_return_value = (d_c_cli_present("CHANGE")
				&& (!d_c_cli_present("OFFSET") || !d_c_cli_present("SIZE")));
	CLI_DIS_CHECK_N_RESET;
	/* DETAIL is compatible only with SHOW */
	disallow_return_value = (!d_c_cli_present("SHOW") && d_c_cli_present("DETAIL"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_replic_receive(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3, p4, p5, p6;

	*cli_err_str_ptr = 0;

	p1 = d_c_cli_present("START");
	p2 = d_c_cli_present("SHUTDOWN");
	p3 = d_c_cli_present("CHECKHEALTH");
	p4 = d_c_cli_present("STATSLOG");
	p5 = d_c_cli_present("SHOWBACKLOG");
	p6 = d_c_cli_present("CHANGELOG");

	/* any MUPIP REPLIC -RECEIVE command should contain at LEAST one of the above qualifiers */
	disallow_return_value = !(p1 || p2 || p3 || p4 || p5 || p6);
	CLI_DIS_CHECK;	/* Note CLI_DIS_CHECK_N_RESET is not used as we want to reuse the computed error string (cli_err_str_ptr)
			 * for the next check as well in case it fails. Note that this can be done only if both checks use
			 * exactly the same set of qualifiers (which is TRUE in this case). */

	/* any MUPIP REPLIC -RECEIVE command should contain at MOST one of the above qualifiers */
	disallow_return_value = cli_check_any2(VARLSTCNT(6) p1, p2, p3, p4, p5, p6);
	CLI_DIS_CHECK_N_RESET;

	disallow_return_value = (d_c_cli_present("START")
				&& !(d_c_cli_present("LISTENPORT") || d_c_cli_present("UPDATEONLY") || d_c_cli_present("HELPERS")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("START") && d_c_cli_present("LISTENPORT") && !d_c_cli_present("LOG"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (!d_c_cli_present("START") && (d_c_cli_present("LISTENPORT")
								|| d_c_cli_present("UPDATERESYNC")
								|| d_c_cli_present("NORESYNC")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("UPDATERESYNC") && d_c_cli_present("NORESYNC"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (!d_c_cli_present("UPDATERESYNC") && (d_c_cli_present("REUSE")
									|| d_c_cli_present("RESUME")
									|| d_c_cli_present("INITIALIZE")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("INITIALIZE") && d_c_cli_present("RESUME"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("REUSE") && d_c_cli_present("RESUME"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (!(d_c_cli_present("START") || d_c_cli_present("SHUTDOWN")) && d_c_cli_present("UPDATEONLY"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (!(d_c_cli_present("START") || d_c_cli_present("SHUTDOWN") || d_c_cli_present("CHECKHEALTH"))
				&& d_c_cli_present("HELPERS"));
	disallow_return_value = (d_c_cli_present("LISTENPORT") && d_c_cli_present("UPDATEONLY"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("UPDATEONLY") && d_c_cli_present("HELPERS"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("CHANGELOG") && !(d_c_cli_present("LOG") || d_c_cli_present("LOG_INTERVAL")));
	CLI_DIS_CHECK_N_RESET;
	/* BUFFSIZE, CMPSIZE or FILTER are supported only with START qualifier */
	disallow_return_value = (!d_c_cli_present("START") && (d_c_cli_present("BUFFSIZE")
								|| d_c_cli_present("CMPLVL")
								|| d_c_cli_present("FILTER")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("AUTOROLLBACK") && !d_c_cli_present("LISTENPORT") && !d_c_cli_present("START"));
	CLI_DIS_CHECK_N_RESET;
	/* LOG are not allowed with STATS qualifier */
	disallow_return_value = (d_c_cli_present("STATSLOG") && d_c_cli_present("LOG"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_replic_source(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13;

	*cli_err_str_ptr = 0;

	p1 = d_c_cli_present("START");
	p2 = d_c_cli_present("SHUTDOWN");
	p3 = d_c_cli_present("ACTIVATE");
	p4 = d_c_cli_present("DEACTIVATE");
	p5 = d_c_cli_present("CHECKHEALTH");
	p6 = d_c_cli_present("STATSLOG");
	p7 = d_c_cli_present("SHOWBACKLOG");
	p8 = d_c_cli_present("CHANGELOG");
	p9 = d_c_cli_present("STOPSOURCEFILTER");
	p10 = d_c_cli_present("LOSTTNCOMPLETE");
	p11 = d_c_cli_present("NEEDRESTART");
	p12 = d_c_cli_present("JNLPOOL");
	p13 = d_c_cli_present("FREEZE");

	/* every source server command must have at least one of the above control qualifiers */
	disallow_return_value = !(p1 || p2 || p3 || p4 || p5 || p6 || p7 || p8 || p9 || p10 || p11 || p12 || p13);
	CLI_DIS_CHECK;	/* Note CLI_DIS_CHECK_N_RESET is not used as we want to reuse the computed error string (cli_err_str_ptr)
			 * for the next check as well in case it fails. Note that this can be done only if both checks use
			 * exactly the same set of qualifiers (which is TRUE in this case). */

	/* every source server command cannot have any more than one of the above control qualifiers */
	disallow_return_value = cli_check_any2(VARLSTCNT(11) p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);
	CLI_DIS_CHECK_N_RESET;

	/* BUFFSIZE, CMPLVL, FILTER, PASSIVE are supported only with START qualifier */
	disallow_return_value = (!d_c_cli_present("START")
					&& (d_c_cli_present("BUFFSIZE")
						|| d_c_cli_present("CMPLVL")
						|| d_c_cli_present("FILTER")
						|| d_c_cli_present("PASSIVE")));
	CLI_DIS_CHECK_N_RESET;
	/* CONNECTPARAMS, SECONDARY are supported only with START or ACTIVATE qualifiers */
	disallow_return_value = (!d_c_cli_present("START") && !d_c_cli_present("ACTIVATE")
					&& (d_c_cli_present("CONNECTPARAMS") || d_c_cli_present("SECONDARY")));
	CLI_DIS_CHECK_N_RESET;
	/* LOG are not allowed with STATS qualifier */
	disallow_return_value = (d_c_cli_present("STATSLOG") && d_c_cli_present("LOG"));
	CLI_DIS_CHECK_N_RESET;
	/* LOG are supported only with START, CHANGELOG, ACTIVATE qualifiers */
	disallow_return_value = (!d_c_cli_present("START")
					&& !d_c_cli_present("CHANGELOG")
					&& !d_c_cli_present("ACTIVATE")
					&& d_c_cli_present("LOG"));
	CLI_DIS_CHECK_N_RESET;
	/* LOG_INTERVAL are supported only with START, CHANGELOG, ACTIVATE, STATSLOG qualifiers */
	disallow_return_value = (!d_c_cli_present("START")
					&& !d_c_cli_present("CHANGELOG")
					&& !d_c_cli_present("ACTIVATE")
					&& !d_c_cli_present("STATSLOG")
					&& d_c_cli_present("LOG_INTERVAL"));
	CLI_DIS_CHECK_N_RESET;
	/* TIMEOUT is supported only with SHUTDOWN qualifier */
	disallow_return_value = (!d_c_cli_present("SHUTDOWN") && d_c_cli_present("TIMEOUT"));
	CLI_DIS_CHECK_N_RESET;
	/* One and only one of PASSIVE or SECONDARY must be specified with START */
	disallow_return_value = (d_c_cli_present("START") && d_c_cli_present("PASSIVE") && d_c_cli_present("SECONDARY"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("START") && !d_c_cli_present("PASSIVE") && !d_c_cli_present("SECONDARY"));
	CLI_DIS_CHECK_N_RESET;
	/* LOG is a mandatory qualifier with START */
	disallow_return_value = (d_c_cli_present("START") && !d_c_cli_present("LOG"));
	CLI_DIS_CHECK_N_RESET;
	/* SECONDARY is a mandatory qualifier with ACTIVATE */
	disallow_return_value = (d_c_cli_present("ACTIVATE") && !d_c_cli_present("SECONDARY"));
	CLI_DIS_CHECK_N_RESET;
	/* One of LOG or LOG_INTERVAL needs to be specified with CHANGELOG */
	disallow_return_value = (d_c_cli_present("CHANGELOG") && !d_c_cli_present("LOG") && !d_c_cli_present("LOG_INTERVAL"));
	CLI_DIS_CHECK_N_RESET;
	/* ROOTPRIMARY (or UPDOK) and PROPAGATEPRIMARY (or UPDNOTOK) are mutually exclusive */
	disallow_return_value = ((d_c_cli_present("ROOTPRIMARY") || d_c_cli_present("UPDOK"))
					&& (d_c_cli_present("PROPAGATEPRIMARY") || d_c_cli_present("UPDNOTOK")));
	CLI_DIS_CHECK_N_RESET;
	/* ROOTPRIMARY and PROPAGATEPRIMARY are allowed only along with START, ACTIVATE or DEACTIVATE qualifiers */
	disallow_return_value = ((d_c_cli_present("ROOTPRIMARY") || d_c_cli_present("PROPAGATEPRIMARY")
						|| d_c_cli_present("UPDOK") || d_c_cli_present("UPDNOTOK"))
					&& !(d_c_cli_present("START")
						|| d_c_cli_present("ACTIVATE")
						|| d_c_cli_present("DEACTIVATE")));
	CLI_DIS_CHECK_N_RESET;
	/* INSTSECONDARY is allowed with every control qualifier except LOSTTNCOMPLETE and JNLPOOL */
	disallow_return_value = (d_c_cli_present("INSTSECONDARY")
					&& (d_c_cli_present("LOSTTNCOMPLETE") || d_c_cli_present("JNLPOOL")));
	CLI_DIS_CHECK_N_RESET;
	/* DETAIL is compatible only with JNLPOOL */
	disallow_return_value = (!d_c_cli_present("JNLPOOL") && d_c_cli_present("DETAIL"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_replic_updhelper(void)
{
	int	disallow_return_value = 0;

	disallow_return_value = !(d_c_cli_present("READER") || d_c_cli_present("WRITER"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_rundown(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value = d_c_cli_present("FILE") && d_c_cli_present("REGION");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_set(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3;

	*cli_err_str_ptr = 0;

	p1 = d_c_cli_present("FILE");
	p2 = d_c_cli_present("REGION");
	p3 = d_c_cli_present("JNLFILE");

	/* any MUPIP SET command should contain at LEAST one of the above qualifiers */
	disallow_return_value = !(p1 || p2 || p3);
	CLI_DIS_CHECK;	/* Note CLI_DIS_CHECK_N_RESET is not used as we want to reuse the computed error string (cli_err_str_ptr)
			 * for the next check as well in case it fails. Note that this can be done only if both checks use
			 * exactly the same set of qualifiers (which is TRUE in this case). */
	/* any MUPIP SET command should contain at MOST one of the above qualifiers */
	disallow_return_value =  cli_check_any2(VARLSTCNT(3) p1, p2, p3);
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
		d_c_cli_present("JOURNAL.DISABLE") || d_c_cli_negated("JOURNAL")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =  (d_c_cli_present("PREVJNLFILE") && !(d_c_cli_present("JNLFILE")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("VERSION") &&
					(d_c_cli_present("ACCESS_METHOD")
					|| d_c_cli_present("GLOBAL_BUFFERS")
					|| d_c_cli_present("RESERVED_BYTES")
					|| d_c_cli_present("FLUSH_TIME")
					|| d_c_cli_present("LOCK_SPACE")
					|| d_c_cli_present("DEFER_TIME")
					|| d_c_cli_present("WAIT_DISK")
					|| d_c_cli_present("PARTIAL_RECOV_BYPASS")));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = (d_c_cli_present("INST_FREEZE_ON_ERROR") && p3);
	CLI_DIS_CHECK_N_RESET;
	p1 = d_c_cli_present("KEY_SIZE");
	p2 = d_c_cli_present("RECORD_SIZE");
	p3 = d_c_cli_present("RESERVED_BYTES");
	disallow_return_value =  cli_check_any2(VARLSTCNT(3) p1, p2, p3);
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_mupip_trigger(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;

	/* any MUPIP TRIGGER command has to have either SELECT or TRIGGERFILE */
	disallow_return_value = !(d_c_cli_present("SELECT") || d_c_cli_present("TRIGGERFILE"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

/*
 * Disallows multiple heuristics, both region and filename, and invalid matching of heuristic parameter with the heuristic.
 */
boolean_t cli_disallow_mupip_size(void)
{
	boolean_t	disallow_return_value = FALSE;
	int4		heur_cnt = 0;

	heur_cnt += (d_c_cli_present("HEURISTIC.ARSAMPLE") ? 1 : 0);
	heur_cnt += (d_c_cli_present("HEURISTIC.IMPSAMPLE") ? 1 : 0);
	heur_cnt += (d_c_cli_present("HEURISTIC.SCAN") ? 1 : 0);
	disallow_return_value = heur_cnt > 1;
	CLI_DIS_CHECK_N_RESET;

	disallow_return_value = ! (
			/* SAMPLES is param for AR and IMP. So:	SAMPLES => (AR || IMP) */
			(d_c_cli_present("HEURISTIC.ARSAMPLE") || d_c_cli_present("HEURISTIC.IMPSAMPLE") ||
			 					  !d_c_cli_present("HEURISTIC.SAMPLES"))
			&&
			/* LEVEL is param for SCAN	So:	LEVEL  => SCAN */
			(d_c_cli_present("HEURISTIC.SCAN") || !d_c_cli_present("HEURISTIC.LEVEL"))
				  );
	CLI_DIS_CHECK_N_RESET;

	disallow_return_value = ! (
			/* SEED is param for AR and IMP. So:	SAMPLES => (AR || IMP) */
			(d_c_cli_present("HEURISTIC.ARSAMPLE") || d_c_cli_present("HEURISTIC.IMPSAMPLE") ||
			!d_c_cli_present("HEURISTIC.SEED"))
				);
	CLI_DIS_CHECK_N_RESET;

	return disallow_return_value;
}
