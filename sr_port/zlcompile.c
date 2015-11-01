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

#include "gtm_string.h"

#include "stp_parms.h"
#include "stringpool.h"
#include "cmd_qlf.h"
#include "cgp.h"
#include "compiler.h"

GBLREF bool			run_time, compile_time;
GBLREF spdesc			stringpool, rts_stringpool, indr_stringpool;
GBLREF char			source_file_name[];
GBLREF unsigned short		source_name_len;
GBLREF command_qualifier	cmd_qlf, glb_cmd_qlf;
GBLREF char			cg_phase;
GBLREF int4			dollar_zcstatus;
GBLREF bool			transform;

int zlcompile (unsigned char len, unsigned char *addr)
{
	bool	obj_exp, status;
	error_def(ERR_ZLINKFILE);
	error_def(ERR_ZLNOOBJECT);

	memcpy (source_file_name, addr, len);
	source_file_name[len] = 0;
	source_name_len = len;

	assert (run_time == TRUE);
	obj_exp = (cmd_qlf.qlf & CQ_OBJECT) != 0;
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
	status = compiler_startup();
	assert (run_time == FALSE && compile_time == TRUE);
	run_time = TRUE;
	compile_time = FALSE;
	transform = TRUE;
	indr_stringpool = stringpool;
	stringpool = rts_stringpool;
	indr_stringpool.free = indr_stringpool.base;
	cmd_qlf.qlf = glb_cmd_qlf.qlf;
	if (obj_exp && cg_phase != CGP_FINI)
		rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, len, addr, ERR_ZLNOOBJECT);
	return status;
}
