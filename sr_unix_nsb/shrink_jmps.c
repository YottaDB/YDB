/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include <emit_code.h>
#include <rtnhdr.h>
#include "obj_file.h"

GBLREF int4		curr_addr, code_size;
GBLREF char		cg_phase;	/* code generation phase */
GBLREF triple		t_orig;		/* head of triples */
LITREF octabstruct	oc_tab[];	/* op-code table */

void shrink_jmps(void)
{
	int	new_size, old_size, shrink;
	triple	*ct;	/* current triple */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(CGP_ADDR_OPT == cg_phase);
	do
	{
		shrink = 0;
		dqloop(&t_orig, exorder, ct)
		{
			if ((oc_tab[ct->opcode].octype & OCT_JUMP) || (OC_LDADDR == ct->opcode) || (OC_FORLOOP == ct->opcode))
			{
				old_size = ct->exorder.fl->rtaddr - ct->rtaddr;
				curr_addr = 0;
				if (0 > ct->operand[0].oprval.tref->rtaddr - ct->rtaddr)
				{
					ct->rtaddr -= shrink;
					trip_gen(ct);
				} else
				{
					trip_gen(ct);
					ct->rtaddr -= shrink;
				}
				new_size = curr_addr;		/* size of operand 0 */
				assert(old_size >= new_size);
				shrink += old_size - new_size;
			} else
				ct->rtaddr -= shrink;
		}/* dqloop */
		code_size -= shrink;
	} while (shrink != 0);

	/* Now that the code has been strunk, we may have to adjust the pad length of the code segment. Compute
	   this by now subtracting out the size of the pad length from the code size and recomputing the pad length
	   and readjusting the code size. (see similar computation in code_gen().
	*/
	code_size -= TREF(codegen_padlen);
	TREF(codegen_padlen) = PADLEN(code_size, SECTION_ALIGN_BOUNDARY);	/* Length to pad to align next section */
	code_size += TREF(codegen_padlen);
}
