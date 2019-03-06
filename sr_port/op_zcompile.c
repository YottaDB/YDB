/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "op.h"
#include "source_file.h"
#include "cli.h"
#include "mmemory.h"
#include "comp_esc.h"

GBLREF CLI_ENTRY		*cmd_ary, mumps_cmd_ary[];
GBLREF command_qualifier	cmd_qlf;
GBLREF mident			module_name;

void op_zcompile(mval *v, boolean_t ignore_dollar_zcompile)
{
	char			ceprep_file[MAX_FN_LEN + 1],
				list_file[MAX_FN_LEN + 1],
				obj_file[MAX_FN_LEN + 1],
				source_file_string[MAX_FN_LEN + 1];
	CLI_ENTRY		*save_cmd_ary;
	unsigned int		status;
	unsigned short		len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	if (!v->str.len)
		return;
	/* We need to parse the string as if it was a MUMPS compilation command, so we switch temporarily to the MUMPS parsing
	 * table "mumps_cmd_ary". Note that the only rts_errors possible between save and restore of the cmd_ary are in
	 * compile_source_file and those are internally handled by source_ch which will transfer control back to us (right after
	 * the the call to compile_source_file below) and hence proper restoring of cmd_ary is guaranteed even in case of errors.
	 */
	save_cmd_ary = cmd_ary;
	cmd_ary = mumps_cmd_ary;
	INIT_CMD_QLF_STRINGS(cmd_qlf, obj_file, list_file, ceprep_file, MAX_FN_LEN);
	len = module_name.len = 0;
	if (!ignore_dollar_zcompile)	/* Process $ZCOMPILE qualifiers except unless a trigger */
		zl_cmd_qlf(&(TREF(dollar_zcompile)), &cmd_qlf, source_file_string, &len, FALSE);
	else
		assert(MAX_FN_LEN == cmd_qlf.object_file.str.len);
	zl_cmd_qlf(&v->str, &cmd_qlf, source_file_string, &len, TRUE);		/* command args override $ZCOMPILE */
	ce_init();	/* initialize compiler escape processing */
	do {
		compile_source_file(len, source_file_string, FALSE);
		cmd_qlf.object_file.str.len = module_name.len = 0;
		len = MAX_FN_LEN;
		status = cli_get_str("INFILE", source_file_string, &len);
	} while (status);
	cmd_ary = save_cmd_ary;	/* restore cmd_ary */
}
