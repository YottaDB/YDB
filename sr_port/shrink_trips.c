/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "cgp.h"
#include "gtmdbglvl.h"
#include "cdbg_dump.h"
#include <emit_code.h>
#include <rtnhdr.h>
#include "obj_file.h"

#if defined(USHBIN_SUPPORTED) || defined(VMS)

GBLREF int4		curr_addr, code_size;
GBLREF char		cg_phase;	/* code generation phase */
GBLREF triple		t_orig;		/* head of triples */
GBLREF uint4		gtmDebugLevel;
LITREF octabstruct	oc_tab[];	/* op-code table */

/* Certain triples need to have their expansions shrunk down to be optimal. Current
 * triples that are eligible for shrinking are:
 *  <triples of type OCT_JUMP>
 *  OC_LDADDR
 *  OC_FORLOOP
 *  literal argument references (either one) for given triple
 * Many of the jump triples start out life as long jumps because of the assumped jump offset
 * in the first pass before the offsets are known. This is where we shrink them.
 *
 * For the literal referencess, if the offset into the literal table exceeds that which can be
 * addressed by the immediate operand of a load address instruction, a long form is generated.
 * But this long form is initially assumed. This is where we will shrink it if we can which
 * should be the normal case.
 */

void shrink_trips(void)
{
	int		new_size, old_size, shrink;
	boolean_t	first_pass;
	triple		*ct;	/* current triple */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef DEBUG
	/* If debug and compiler debugging enabled, run through triples again to show where we are just before we modify them. */
	if (gtmDebugLevel & GDL_DebugCompiler)
	{
		PRINTF(" \n\n\n\n************************************ Begin pre-shrink_trips dump *****************************\n");
		dqloop(&t_orig, exorder, ct)
		{
			PRINTF(" ************************ Triple Start **********************\n");
			cdbg_dump_triple(ct, 0);
		}
		PRINTF(" \n\n\n\n************************************ Begin shrink_trips scan *****************************\n");
	}
#	endif
	first_pass = TRUE;
	assert(CGP_ADDR_OPT == cg_phase);	/* Follows address optimization phase */
	do
	{
		shrink = 0;
		dqloop(&t_orig, exorder, ct)
		{
			if ((oc_tab[ct->opcode].octype & OCT_JUMP) || (OC_LDADDR == ct->opcode) || (OC_FORLOOP == ct->opcode))
			{
				old_size = ct->exorder.fl->rtaddr - ct->rtaddr;
				curr_addr = 0;
				if (ct->operand[0].oprval.tref->rtaddr - ct->rtaddr < 0)
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
				COMPDBG(if (0 != (old_size - new_size)) cdbg_dump_shrunk_triple(ct, old_size, new_size););
				shrink += old_size - new_size;
			} else if (first_pass && !(oc_tab[ct->opcode].octype & OCT_CGSKIP) &&
				   (litref_triple_oprcheck(&ct->operand[0]) || litref_triple_oprcheck(&ct->operand[1])))
			{
				old_size = ct->exorder.fl->rtaddr - ct->rtaddr;
				curr_addr = 0;
				trip_gen(ct);
				ct->rtaddr -= shrink;
				new_size = curr_addr;		/* size of operand 0 */
				assert(old_size >= new_size);
				COMPDBG(if (0 != (old_size - new_size)) cdbg_dump_shrunk_triple(ct, old_size, new_size););
				shrink += old_size - new_size;
			} else
				ct->rtaddr -= shrink;
			if (0 == shrink && OC_TRIPSIZE == ct->opcode)
			{	/* This triple is a reference to another triple whose codegen size we want to compute for an
				 * argument to another triple. We will only do this on what appears to (thus far) be
				 * the last pass through the triples).
				 */
				curr_addr = 0;
				trip_gen(ct->operand[0].oprval.tsize->ct);
				ct->operand[0].oprval.tsize->size = curr_addr;
			}
		}	/* dqloop */
		code_size -= shrink;
		first_pass = FALSE;
	} while (0 != shrink);		/* Do until no more shrinking occurs */

	/* Now that the code has been strunk, we may have to adjust the pad length of the code segment. Compute
	 * this by now subtracting out the size of the pad length from the code size and recomputing the pad length
	 * and readjusting the code size. (see similar computation in code_gen().
	 */
	code_size -= TREF(codegen_padlen);
	TREF(codegen_padlen) = PADLEN(code_size, SECTION_ALIGN_BOUNDARY);	/* Length to pad to align next section */
	code_size += TREF(codegen_padlen);
#	ifdef DEBUG
	/* If debug and compiler debuggingenabled, run through the triples again to show what we did to them */
	if (gtmDebugLevel & GDL_DebugCompiler)
	{
		dqloop(&t_orig, exorder, ct)
		{
			PRINTF(" ************************ Triple Start **********************\n");
			cdbg_dump_triple(ct, 0);
		}
	}
#	endif
}

/* Check if this operand or any it may be chained to are literals in the literal section (not immediate operands) */
boolean_t litref_triple_oprcheck(oprtype *operand)
{
	if (TRIP_REF == operand->oprclass)
	{	/* We are referring to another triple */
		if (OC_LIT == operand->oprval.tref->opcode)
			/* It is a literal section reference.. done */
			return TRUE;
		if (OC_PARAMETER == operand->oprval.tref->opcode)
		{	/* There are two more parameters to check. Call recursively */
			if (litref_triple_oprcheck(&operand->oprval.tref->operand[0]))
				return TRUE;
			return litref_triple_oprcheck(&operand->oprval.tref->operand[1]);
		}
	}
	return FALSE;	/* Uninteresting triple .. not our type */
}
#endif /* USHBIN_SUPPORTED or VMS */
