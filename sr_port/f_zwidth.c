/****************************************************************
 *                                                              *
 *      Copyright 2006, 2010 Fidelity Information Services, Inc	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "advancewindow.h"

GBLREF char window_token;

/* $ZWIDTH(): Single parameter - string expression */
int f_zwidth(oprtype *a, opctype op)
{
        triple *r;

        r = maketriple(op);
        if (!strexpr(&(r->operand[0])))
                return FALSE;
        ins_triple(r);
        *a = put_tref(r);
        return TRUE;
}

