/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 74ea4a3c... GT.M V6.3-006
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "cgp.h"
#include "cmd_qlf.h"
#include "list_file.h"
#include "source_file.h"
#include "obj_file.h"
#include "reinit_externs.h"
#include "compiler.h"
#include "util.h"
#include "hashtab_str.h"
#include "stp_parms.h"
#include "stringpool.h"

GBLREF boolean_t		mstr_native_align, save_mstr_native_align;
GBLREF char			cg_phase;
GBLREF command_qualifier	cmd_qlf;
GBLREF spdesc			indr_stringpool, rts_stringpool, stringpool;

error_def(ERR_ASSERT);
error_def(ERR_FORCEDHALT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_STACKOFLOW);
error_def(ERR_OUTOFSPACE);

CONDITION_HANDLER(compiler_ch)
{
	START_CH(TRUE);
	if (DUMPABLE)
		NEXTCH;
	if (TREF(xecute_literal_parse))
	{	/* This implies we had an error while inside "m_xecute" in the middle of a compile.
		 * Reset whatever global variables we had set temporarily there before doing an UNWIND
		 * (that transfers control back to a parent caller function).
		 */
		run_time = TREF(xecute_literal_parse) = FALSE;
	}
<<<<<<< HEAD
	if (cmd_qlf.qlf & CQ_WARNINGS)
=======
	if (CQ_WARNINGS & cmd_qlf.qlf)
>>>>>>> 74ea4a3c... GT.M V6.3-006
		PRN_ERROR;
	COMPILE_HASHTAB_CLEANUP;
	reinit_externs();
	mstr_native_align = save_mstr_native_align;
<<<<<<< HEAD
	if (cg_phase == CGP_MACHINE)
		drop_object_file();
	if (cg_phase > CGP_NOSTATE)
=======
	if (CGP_MACHINE == cg_phase)
		drop_object_file();
	if (CGP_NOSTATE < cg_phase)
>>>>>>> 74ea4a3c... GT.M V6.3-006
	{
		if (CGP_RESOLVE > cg_phase)
			close_source_file();
<<<<<<< HEAD
		if (cg_phase < CGP_FINI  &&  (cmd_qlf.qlf & CQ_LIST  ||  cmd_qlf.qlf & CQ_CROSS_REFERENCE))
			close_list_file();
=======
		if ((CGP_FINI > cg_phase)  &&  ((CQ_LIST & cmd_qlf.qlf) || (CQ_CROSS_REFERENCE & cmd_qlf.qlf)))
			close_list_file();
	}
	if (TREF(compile_time))
	{
		run_time = TRUE;
		TREF(compile_time) = FALSE;
		TREF(transform) = TRUE;
	}
	if (indr_stringpool.base == stringpool.base)
	{
		indr_stringpool = stringpool;
		stringpool = rts_stringpool;
>>>>>>> 74ea4a3c... GT.M V6.3-006
	}
	UNWIND(NULL, NULL);
}
