/****************************************************************
 *                                                              *
 * Copyright (c) 2001-2023 Fidelity National Information        *
 * Services, Inc. and/or its subsidiaries. All rights reserved. *
 *                                                              *
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.                                         *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

/* sparm */
#include "mdef.h"
#include "compiler.h"
#include "toktyp.h"

/* Check syntax, only one string argument */
int f_zycompile(oprtype *a, opctype op)
{
		triple  *r;

        DCL_THREADGBL_ACCESS;
        SETUP_THREADGBL_ACCESS;

        r = maketriple(op);
        if (EXPR_FAIL == expr(&r->operand[0], MUMPS_STR))

                return FALSE;
        
	ins_triple(r);
        *a = put_tref(r);
        return TRUE;
}
