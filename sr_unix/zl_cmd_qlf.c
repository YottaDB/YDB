/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

GBLREF	char	cli_err_str[];

void zl_cmd_qlf (mstr *quals, command_qualifier *qualif)
{
	char		cbuf[MAX_LINE];
	error_def	(ERR_COMPILEQUALS);
	int		parse_ret;

	if (quals->len + sizeof(COMMAND) > MAX_LINE)
		rts_error(VARLSTCNT(4) ERR_COMPILEQUALS, 2, quals->len, quals->addr);

	memcpy(cbuf, COMMAND, sizeof(COMMAND) - 1);
	memcpy(cbuf + sizeof(COMMAND) -1, quals->addr, quals->len);
	cbuf[sizeof(COMMAND) - 1 + quals->len] = 0;
	cli_str_setup((SIZEOF(COMMAND) + quals->len), cbuf);
	parse_ret = parse_cmd();
	if (parse_ret)
		rts_error(VARLSTCNT(4) parse_ret, 2, LEN_AND_STR(cli_err_str));

	qualif->object_file.mvtype = qualif->list_file.mvtype = qualif->ceprep_file.mvtype = 0;
	get_cmd_qlf (qualif);
}
