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
#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "cgp.h"
#include "emit_code.h"

GBLREF int4		curr_addr, code_size;
GBLREF char		cg_phase;	/* code generation phase */
GBLREF triple		t_orig;		/* head of triples */
LITREF octabstruct	oc_tab[];	/* op-code table */

void shrink_jmps(void)
{
	int4	new_size, old_size, shrink;
	triple	*ct;	/* current triple */

	assert(cg_phase == CGP_ADDR_OPT);
	do
	{
		shrink = 0;
		dqloop(&t_orig, exorder, ct)
		{
			if (oc_tab[ct->opcode].octype & OCT_JUMP || ct->opcode == OC_LDADDR || ct->opcode == OC_FORLOOP)
			{
				old_size = ct->exorder.fl->rtaddr - ct->rtaddr;
				curr_addr = 0;
				if (ct->operand[0].oprval.tref->rtaddr - ct->rtaddr < 0)
				{	ct->rtaddr -= shrink;
					trip_gen(ct);
				}
				else
				{	trip_gen(ct);
					ct->rtaddr -= shrink;
				}
				new_size = curr_addr;		/* size of operand 0 */
				assert(old_size >= new_size);
				shrink += old_size - new_size;
			}
			else
			{
				ct->rtaddr -= shrink;
			}
		}/* dqloop */
		code_size -= shrink;
	}while (shrink != 0);
}
