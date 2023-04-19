/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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
#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "mmemory.h"
#include <emit_code.h>
#include "fullbool.h"
#include "stringpool.h"

LITREF octabstruct	oc_tab[];

GBLREF boolean_t		run_time;
GBLREF command_qualifier	cmd_qlf;


void bx_boolop(triple *t, boolean_t jmp_type_one, boolean_t jmp_to_next, boolean_t sense, oprtype *addr)
/* process the operations into a chain of mostly conditional jumps
 * *t points to the Boolean operation
 * jmp_type_one gives the sense of the jump associated with the first operand
 * jmp_to_next gives whether we need a second jump to complete the operation
 * sense gives the sense of the requested operation
 * *addr points the operand for the jump and is eventually used by logic back in the invocation stack to fill in a target location
 */
{
	oprtype		*p;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(OCT_BOOL & oc_tab[t->opcode].octype);
	assert(!(OCT_SE & oc_tab[t->opcode].octype));
	assert(((1 & sense) == sense) && ((1 & jmp_to_next) == jmp_to_next) && ((1 & jmp_type_one) == jmp_type_one));
	assert((TRIP_REF == t->operand[0].oprclass) && (TRIP_REF == t->operand[1].oprclass));
	assert(OCT_BOOL & oc_tab[t->opcode].octype);
	assert((TRIP_REF == t->operand[0].oprclass) && (TRIP_REF == t->operand[1].oprclass));
	if (jmp_to_next)
	{
		p = (oprtype *)mcalloc(SIZEOF(oprtype));
		*p = put_tjmp(t);
	} else
		p = addr;
	/* nice simple short circuit */
	assert(NULL == TREF(boolchain_ptr));
	bx_tail(t->operand[0].oprval.tref, jmp_type_one, p);
	bx_tail(t->operand[1].oprval.tref, sense, addr);
	t->opcode = OC_NOOP;				/* must not delete as this can be a jmp target */
	t->operand[0].oprclass = t->operand[1].oprclass = NO_REF;
	return;
}
