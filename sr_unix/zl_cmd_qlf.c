/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cmd_qlf.h"
#include "cli.h"
#include "cli_parse.h"

#define	COMMAND			"MUMPS "

GBLREF	char		cli_err_str[];
GBLREF	CLI_ENTRY	*cmd_ary;
GBLREF	CLI_ENTRY	mumps_cmd_ary[];

void zl_cmd_qlf (mstr *quals, command_qualifier *qualif)
{
	char		cbuf[MAX_LINE];
	int		parse_ret;
	CLI_ENTRY	*save_cmd_ary;

	error_def	(ERR_COMPILEQUALS);

	if (quals->len + SIZEOF(COMMAND) > MAX_LINE)
		rts_error(VARLSTCNT(4) ERR_COMPILEQUALS, 2, quals->len, quals->addr);

	MEMCPY_LIT(cbuf, COMMAND);
	memcpy(cbuf + SIZEOF(COMMAND) -1, quals->addr, quals->len);
	cbuf[SIZEOF(COMMAND) - 1 + quals->len] = 0;
	/* The caller of this function could be GT.M, DSE, MUPIP, GTCM GNP server, GTCM OMI server etc. Most of them have their
	 * own command parsing tables and some dont even have one. Nevertheless, we need to parse the string as if it was a
	 * MUMPS compilation command. So we switch temporarily to the MUMPS parsing table "mumps_cmd_ary". Note that the only
	 * rts_errors possible between save and restore of the cmd_ary are in compile_source_file and those are internally
	 * handled by source_ch which will transfer control back to us (right after the the call to compile_source_file below)
	 * and hence proper restoring of cmd_ary is guaranteed even in case of errors.
	 */
	save_cmd_ary = cmd_ary;
	cmd_ary = &mumps_cmd_ary[0];
	cli_str_setup((SIZEOF(COMMAND) + quals->len), cbuf);
	parse_ret = parse_cmd();
	if (parse_ret)
		rts_error(VARLSTCNT(4) parse_ret, 2, LEN_AND_STR(cli_err_str));

	qualif->object_file.mvtype = qualif->list_file.mvtype = qualif->ceprep_file.mvtype = 0;
	get_cmd_qlf (qualif);
	cmd_ary = save_cmd_ary;	/* restore cmd_ary */
}
