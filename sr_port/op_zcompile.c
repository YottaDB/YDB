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
#include "stp_parms.h"
#include "stringpool.h"
#include "cmd_qlf.h"
#include "op.h"
#include "source_file.h"
#include "cli.h"

#define FILE_NAME_SIZE 255

GBLDEF int4			dollar_zcstatus;

GBLREF bool			run_time, compile_time;
GBLREF spdesc			stringpool, rts_stringpool, indr_stringpool;
GBLREF command_qualifier	cmd_qlf, glb_cmd_qlf;
GBLREF bool			transform;

void op_zcompile(mval *v)
{
	unsigned		status;
	command_qualifier	save_qlf;
	unsigned short		len;
	char			source_file_string[FILE_NAME_SIZE + 1],
				obj_file[FILE_NAME_SIZE + 1],
				list_file[FILE_NAME_SIZE + 1],
				ceprep_file[FILE_NAME_SIZE + 1];

	MV_FORCE_STR(v);
	if (!v->str.len)
		return;

	save_qlf = glb_cmd_qlf;

	glb_cmd_qlf.object_file.str.addr = obj_file;
	glb_cmd_qlf.object_file.str.len = FILE_NAME_SIZE;
	glb_cmd_qlf.list_file.str.addr = list_file;
	glb_cmd_qlf.list_file.str.len = FILE_NAME_SIZE;
	glb_cmd_qlf.ceprep_file.str.addr = ceprep_file;
	glb_cmd_qlf.ceprep_file.str.len = FILE_NAME_SIZE;

	zl_cmd_qlf(&v->str, &glb_cmd_qlf);
	cmd_qlf = glb_cmd_qlf;

	assert (run_time == TRUE);
	assert(rts_stringpool.base == stringpool.base);
	rts_stringpool = stringpool;
	if (!indr_stringpool.base)
	{
		stp_init (STP_INITSIZE);
		indr_stringpool = stringpool;
	} else
		stringpool = indr_stringpool;

	run_time = FALSE;
	compile_time = TRUE;
	transform = FALSE;
	dollar_zcstatus = 1;
	len = FILE_NAME_SIZE;
	for (status = cli_get_str("INFILE",source_file_string, &len);
		status;
		status = cli_get_str("INFILE",source_file_string, &len))
	{
		compile_source_file(len, source_file_string);
		len = FILE_NAME_SIZE;
	}

	assert (run_time == FALSE && compile_time == TRUE);
	run_time = TRUE;
	compile_time = FALSE;
	transform = TRUE;
	indr_stringpool = stringpool;
	stringpool = rts_stringpool;
	indr_stringpool.free = indr_stringpool.base;
	glb_cmd_qlf = save_qlf;
}
