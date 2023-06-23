/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "cgp.h"
#include "compiler.h"
#include "mmemory.h"

GBLREF boolean_t		run_time;
GBLREF unsigned char		source_file_name[];
GBLREF unsigned short		source_name_len;
GBLREF command_qualifier	cmd_qlf;
GBLREF char			cg_phase;

error_def(ERR_ZLINKFILE);
error_def(ERR_ZLNOOBJECT);

int zlcompile (unsigned char len, unsigned char *addr)
{
	boolean_t	obj_exp, status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	memcpy(source_file_name, addr, len);
	source_file_name[len] = 0;
	source_name_len = len;
	assert(run_time);
	obj_exp = (0 != (cmd_qlf.qlf & CQ_OBJECT));
	status = compiler_startup();
	if (obj_exp && cg_phase != CGP_FINI)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, len, addr, ERR_ZLNOOBJECT);
	return status;
}
