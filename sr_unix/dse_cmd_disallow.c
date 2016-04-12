/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
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
#include "dse_cmd_disallow.h"

GBLREF	char	*cli_err_str_ptr;

boolean_t cli_disallow_dse_add(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value = d_c_cli_present("STAR") && (d_c_cli_present("DATA")
								|| d_c_cli_present("KEY")
								|| d_c_cli_present("RECORD")
								|| d_c_cli_present("OFFSET"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("DATA") && d_c_cli_present("POINTER");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("RECORD") && d_c_cli_present("OFFSET");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_all(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value = d_c_cli_present("WCINIT") && d_c_cli_present("BUFFER_FLUSH");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("RENEW") && (d_c_cli_present("FREEZE")
								|| d_c_cli_present("SEIZE")
								|| d_c_cli_present("RELEASE")
								|| d_c_cli_present("CRITINIT")
								|| d_c_cli_present("BUFFER_FLUSH")
								|| d_c_cli_present("REFERENCE")
								|| d_c_cli_present("WCINIT")
								|| d_c_cli_present("OVERRIDE"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("SEIZE") && d_c_cli_present("RELEASE");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("CRITINIT") && (d_c_cli_present("SEIZE") || d_c_cli_present("RELEASE"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("DUMP") && (d_c_cli_present("BUFFER_FLUSH")
								|| d_c_cli_present("CRITINIT")
								|| d_c_cli_present("FREEZE")
								|| d_c_cli_present("OVERRIDE")
								|| d_c_cli_present("REFERENCE")
								|| d_c_cli_present("RELEASE")
								|| d_c_cli_present("RENEW")
								|| d_c_cli_present("SEIZE")
								|| d_c_cli_present("WCINIT"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("ALL") && !d_c_cli_present("DUMP");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_cache(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3, p4;

	*cli_err_str_ptr = 0;

	p1 = d_c_cli_present("CHANGE");
	p2 = d_c_cli_present("RECOVER");
	p3 = d_c_cli_present("SHOW");
	p4 = d_c_cli_present("VERIFY");

	/* any DSE CACHE command should contain at LEAST one of the above qualifiers */
	disallow_return_value = !(p1 || p2 || p3 || p4);
	CLI_DIS_CHECK;	/* Note CLI_DIS_CHECK_N_RESET is not used as we want to reuse the computed error string (cli_err_str_ptr)
			 * for the next check as well in case it fails. Note that this can be done only if both checks use
			 * exactly the same set of qualifiers (which is TRUE in this case). */

	/* any DSE CACHE command should contain at MOST one of the above qualifiers */
	disallow_return_value = cli_check_any2(VARLSTCNT(4) p1, p2, p3, p4);
	CLI_DIS_CHECK_N_RESET;

	disallow_return_value = d_c_cli_present("ALL") && d_c_cli_present("CHANGE");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = !(d_c_cli_present("CHANGE") || d_c_cli_present("SHOW")) && (d_c_cli_present("OFFSET")
												|| d_c_cli_present("SIZE")
												|| d_c_cli_present("VALUE"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("SHOW") && d_c_cli_present("VALUE");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("CHANGE") && !d_c_cli_present("OFFSET");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("OFFSET") && !d_c_cli_present("SIZE");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("SIZE") && !d_c_cli_present("OFFSET");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("VALUE") && !d_c_cli_present("OFFSET");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_negated("CRIT") && (d_c_cli_present("CHANGE")
								|| d_c_cli_present("RECOVER")
								|| d_c_cli_present("VERIFY"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_change(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value = d_c_cli_present("FILEHEADER") && (d_c_cli_present("BLOCK")
								|| d_c_cli_present("LEVEL")
								|| d_c_cli_present("BSIZ")
								|| d_c_cli_present("RECORD")
								|| d_c_cli_present("OFFSET")
								|| d_c_cli_present("CMPC")
								|| d_c_cli_present("RSIZ")
								|| d_c_cli_present("TN"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value =        (   d_c_cli_present("LEVEL")
					|| d_c_cli_present("BSIZ")
					|| d_c_cli_present("TN"))
							&&     (   d_c_cli_present("RECORD")
								|| d_c_cli_present("OFFSET")
								|| d_c_cli_present("CMPC")
								|| d_c_cli_present("RSIZ"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("RECORD") && d_c_cli_present("OFFSET");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_chng_fhead(void)
{
	int		disallow_return_value = 0;

	disallow_return_value = d_c_cli_present("STRM_NUM") && !d_c_cli_present("STRM_REG_SEQNO");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = !d_c_cli_present("STRM_NUM") && d_c_cli_present("STRM_REG_SEQNO");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_crit(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3, p4, p5;

	*cli_err_str_ptr = 0;

	p1 = d_c_cli_present("INIT");
	p2 = d_c_cli_present("OWNER");
	p3 = d_c_cli_present("SEIZE");
	p4 = d_c_cli_present("RELEASE");
	p5 = d_c_cli_present("REMOVE");
	disallow_return_value = cli_check_any2(VARLSTCNT(5) p1, p2, p3, p4, p5);
	CLI_DIS_CHECK_N_RESET;

	disallow_return_value = d_c_cli_present("CRASH") && (d_c_cli_present("SEIZE")
								|| d_c_cli_present("RELEASE")
								|| d_c_cli_present("OWNER")
								|| d_c_cli_present("RESET"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("RESET") && (d_c_cli_present("SEIZE")
								|| d_c_cli_present("RELEASE")
								|| d_c_cli_present("OWNER"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("CYCLE") && (d_c_cli_present("INIT")
								|| d_c_cli_present("REMOVE")
								|| d_c_cli_present("SEIZE")
								|| d_c_cli_present("RELEASE")
								|| d_c_cli_present("RESET"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("ALL") && (d_c_cli_present("CRASH")
								|| d_c_cli_present("CYCLE")
								|| d_c_cli_present("INIT")
								|| d_c_cli_present("OWNER")
								|| d_c_cli_present("RELEASE")
								|| d_c_cli_present("REMOVE")
								|| d_c_cli_present("RESET")
								|| d_c_cli_present("SEIZE"));
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_dump(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3;

	*cli_err_str_ptr = 0;

	p1 = d_c_cli_present("RECORD");
	p2 = d_c_cli_present("OFFSET");
	p3 = d_c_cli_present("FILEHEADER");
	disallow_return_value = cli_check_any2(VARLSTCNT(3) p1, p2, p3);
	CLI_DIS_CHECK_N_RESET;

	disallow_return_value = d_c_cli_present("FILEHEADER") && (d_c_cli_present("BLOCK")
								|| d_c_cli_present("HEADER")
								|| d_c_cli_present("COUNT")
								|| d_c_cli_present("GLO")
								|| d_c_cli_present("ZWR"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("GLO") && d_c_cli_present("ZWR");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("GLO") && d_c_cli_present("HEADER");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("ZWR") && d_c_cli_present("HEADER");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("COUNT") && !d_c_cli_present("HEADER")
					&& !(d_c_cli_present("RECORD") || d_c_cli_present("OFFSET")) && !d_c_cli_present("BLOCK");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_find(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3, p4;

	*cli_err_str_ptr = 0;

	p1 = d_c_cli_present("BLOCK");
	p2 = d_c_cli_present("FREEBLOCK");
	p3 = d_c_cli_present("KEY");
	p4 = d_c_cli_present("REGION");
	disallow_return_value = cli_check_any2(VARLSTCNT(4) p1, p2, p3, p4);
	CLI_DIS_CHECK_N_RESET;

	disallow_return_value = (	   d_c_cli_present("EXHAUSTIVE")
					|| d_c_cli_present("SIBLINGS"))
							&& (	   d_c_cli_present("FREEBLOCK")
								|| d_c_cli_present("KEY")
								|| d_c_cli_present("REGION"));
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("HINT") && !d_c_cli_present("FREEBLOCK");
	CLI_DIS_CHECK_N_RESET;
	disallow_return_value = d_c_cli_present("FREEBLOCK") && !d_c_cli_present("HINT");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_maps(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3, p4;

	*cli_err_str_ptr = 0;

	p1 = d_c_cli_present("FREE");
	p2 = d_c_cli_present("BUSY");
	p3 = d_c_cli_present("MASTER");
	p4 = d_c_cli_present("RESTORE_ALL");
	disallow_return_value = cli_check_any2(VARLSTCNT(4) p1, p2, p3, p4);
	CLI_DIS_CHECK_N_RESET;

	disallow_return_value = d_c_cli_present("BLOCK") && d_c_cli_present("RESTORE_ALL");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_remove(void)
{
	int		disallow_return_value = 0;
	boolean_t	p1, p2, p3;

	*cli_err_str_ptr = 0;

	p1 = d_c_cli_present("RECORD");
	p2 = d_c_cli_present("OFFSET");
	p3 = d_c_cli_present("VERSION");
	disallow_return_value = cli_check_any2(VARLSTCNT(3) p1, p2, p3);
	CLI_DIS_CHECK_N_RESET;

	disallow_return_value = d_c_cli_present("VERSION") && d_c_cli_present("COUNT");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_save(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value = d_c_cli_present("LIST") && d_c_cli_present("COMMENT");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

boolean_t cli_disallow_dse_shift(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = 0;
	disallow_return_value = d_c_cli_present("FORWARD") && d_c_cli_present("BACKWARD");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}
