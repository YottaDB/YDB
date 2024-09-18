/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include <stddef.h>

#include "cmd_qlf.h"
#include "gtmdbglvl.h"
#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "alloc_reg.h"
#include "cdbg_dump.h"
#include "fullbool.h"

GBLDEF int4			sa_temps[VALUED_REF_TYPES];
GBLDEF int4			sa_temps_offset[VALUED_REF_TYPES];

GBLREF int			mvmax;
GBLREF triple			t_orig;
GBLREF uint4			ydbDebugLevel;
GBLREF command_qualifier  	cmd_qlf;

LITDEF int4 sa_class_sizes[VALUED_REF_TYPES] =
{
	0		/* dummy for slot zero */
	,SIZEOF(mval*)	/* TVAR_REF */
	,SIZEOF(mval)	/* TVAL_REF */
	,SIZEOF(mint)	/* TINT_REF */
	,SIZEOF(mval*)	/* TVAD_REF */
	,SIZEOF(char*)	/* TCAD_REF */
};
LITREF octabstruct 		oc_tab[];

#define MAX_TEMP_COUNT		1024

STATICFNDCL void remove_backptr(triple *curtrip, oprtype *opnd, char (*tempcont)[MAX_TEMP_COUNT]);

void alloc_reg(void)
{
	triple		*x, *y, *ref;
	oprtype 	*j;
	opctype 	opc, opx;
	char		tempcont[VALUED_REF_TYPES][MAX_TEMP_COUNT], dest_type;
	int		r, c, temphigh[VALUED_REF_TYPES];
	unsigned int	oct;
	int4		size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	memset(&tempcont[0][0], 0, SIZEOF(tempcont));
	memset(&temphigh[0], -1, SIZEOF(temphigh));
	temphigh[TVAR_REF] = mvmax - 1;
	COMPDBG(PRINTF(" \n\n\n\n************************************ Begin alloc_reg scan *****************************\n"););
	dqloop(&t_orig, exorder, x)
	{
		COMPDBG(PRINTF(" ************************ Triple Start **********************\n"););
		COMPDBG(cdbg_dump_triple(x, 0););
		opc = x->opcode;
		switch (opc)
		{
			case OC_NOOP:
			case OC_PARAMETER:
				continue;
			case OC_LINESTART:
				/* If the next triple is also a LINESTART, then this is a comment line.
				 * Therefore eliminate this LINESTART
				 */
				opx = x->exorder.fl->opcode;
				if ((OC_LINESTART == opx) || (OC_LINEFETCH == opx))
				{
					x->opcode = OC_NOOP;
					COMPDBG(PRINTF("   ** Converting triple to NOOP (rsn 1) **\n"););
					continue;	/* continue, because 'normal' NOOP continues from this switch */
				}
				/* There is a special case in the case of NOLINE_ENTRY being specified. If a blank line is followed
				 * by a line with a label and that label generates fetch information, the generated triple sequence
				 * will be LINESTART (from blank line), ILIT (count from PREVIOUS fetch), LINEFETCH. We will detect
				 * that sequence here and change the LINESTART to a NOOP.
				 */
				if (!(cmd_qlf.qlf & CQ_LINE_ENTRY) && (OC_ILIT == opx) && (NULL != x->exorder.fl->exorder.fl)
				    && (OC_LINEFETCH == x->exorder.fl->exorder.fl->opcode))
				{
					x->opcode = OC_NOOP;
					COMPDBG(PRINTF("   ** Converting triple to NOOP (rsn 2) **\n"););
					continue;	/* continue, because 'normal' NOOP continues from this switch */
				}				/* WARNING else fallthrough */
			case OC_LINEFETCH:
				/* this code is a sad hack - it used to assert that there was no temp leak, but in non-short-circuit
				 * mode there can be a leak, we weren't finding it and the customers were justifiably impatient
				 * so the following code now makes the leak go away
				 */
				for (c = temphigh[TVAL_REF]; 0 <= c; c--)
					tempcont[TVAL_REF][c] = 0;	/* prevent leaking TVAL temps */
				if (OC_LINESTART == opc)
					break;			/* WARNING else fallthrough */
			case OC_FETCH:
				assert((TRIP_REF == x->operand[0].oprclass) && (OC_ILIT == x->operand[0].oprval.tref->opcode));
				if (x->operand[0].oprval.tref->operand[0].oprval.ilit == mvmax)
				{
					x->operand[0].oprval.tref->operand[0].oprval.ilit = 0;
					x->operand[1].oprclass = NO_REF;
				}
				break;
			case OC_PASSTHRU:
				COMPDBG(PRINTF(" *** OC_PASSTHRU opcode being NOOP'd\n"););
				remove_backptr(x, &x->operand[0], tempcont);
				x->opcode = OC_NOOP;
				x->operand[0].oprclass = NO_REF;
				assert((NO_REF == x->operand[1].oprclass) || (TRIP_REF == x->operand[1].oprclass));
				assert(NO_REF == x->destination.oprclass);
				continue;
			case OC_STO:
				/* If we are storing a literal e.g. s x="hi", don't call op_sto, because we do not
				 * need to check if the literal is defined.  OC_STOLIT will be an in-line copy.
				 * Bypass this if we have been requested to not do inline literals.
				 */
				if ((cmd_qlf.qlf & CQ_INLINE_LITERALS) && (TRIP_REF == x->operand[1].oprclass)
				    && (OC_LIT == x->operand[1].oprval.tref->opcode))
					opc = x->opcode = OC_STOLIT;
				break;
			case OC_GVSAVTARG:
				if (x->backptr.que.fl == &x->backptr)
				{	/* jmp_opto removed all associated OC_GVRECTARGs - must get rid of this OC_GVSAVTARG */
					opc = x->opcode = OC_NOOP;
					assert((NO_REF == x->operand[0].oprclass) && (NO_REF == x->operand[1].oprclass));
				}				/* WARNING fallthrough */
			case OC_BOOLEXPRSTART:
				/* Check if OC_BOOLEXPRSTART/OC_BOOLEXPRFINISH triple removal is pending (phase 2 of
				 * OC_EQUNUL_RETBOOL etc. optimization, see comment in "sr_port/bool_expr.c" for details).
				 * It is easily identified by operand[0] pointing to a non-empty triple. Instead of
				 * removing the triple, replace it with a OC_NOOP opcode as later OC_BOOLEXPRFINISH
				 * triples also need to be replaced for the same optimization and this triple is the
				 * only way they can all note the need for the optimization.
				 */
				if (NO_REF != x->operand[0].oprclass)
				{
					assert(TRIP_REF == x->operand[0].oprclass);
					assert((OC_EQUNUL_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_NEQUNUL_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_EQU_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_NEQU_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_CONTAIN_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_NCONTAIN_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_FOLLOW_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_NFOLLOW_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_PATTERN_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_NPATTERN_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_SORTSAFTER_RETBOOL == x->operand[0].oprval.tref->opcode)
						|| (OC_NSORTSAFTER_RETBOOL == x->operand[0].oprval.tref->opcode));
					x->opcode = OC_NOOP;	/* Replace OC_BOOLEXPRSTART triple with OC_NOOP */
					continue;
				}
				break;
			case OC_BOOLEXPRFINISH:
				/* Normally, a OC_BOOLEXPRFINISH triple points to the corresponding OC_BOOLEXPRSTART
				 * triple through its operand[0]. But in case the OC_BOOLEXPRSTART triple was optimized
				 * out (see previous "case" for details), it would have been replaced with a OC_NOOP
				 * opcode.
				 */
				assert(TRIP_REF == x->operand[0].oprclass);
				y = x->operand[0].oprval.tref;	/* y points to corresponding OC_BOOLEXPRSTART triple */
				assert((OC_BOOLEXPRSTART == y->opcode) || (OC_NOOP == y->opcode));
				if (OC_NOOP == y->opcode)
				{
					assert(TRIP_REF == y->operand[0].oprclass);
					assert((OC_EQUNUL_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_NEQUNUL_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_EQU_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_NEQU_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_CONTAIN_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_NCONTAIN_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_FOLLOW_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_NFOLLOW_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_PATTERN_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_NPATTERN_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_SORTSAFTER_RETBOOL == y->operand[0].oprval.tref->opcode)
						|| (OC_NSORTSAFTER_RETBOOL == y->operand[0].oprval.tref->opcode));
					x->opcode = OC_NOOP;	/* Replace OC_BOOLEXPRFINISH triple with OC_NOOP */
					/* Check if OC_BOOLEXPRFINISH is preceded by an OC_JMP that jumps past just
					 * this OC_BOOLEXPRFINISH triple. In that case, we can safely remove the
					 * unnecessary OC_JMP triple too. Just like OC_BOOLEXPRFINISH, we replace
					 * the OC_JMP triple with OC_NOOP.
					 */
					y = x->exorder.bl;
					if ((OC_JMP == y->opcode) && (TJMP_REF == y->operand[0].oprclass)
							&& (y->operand[0].oprval.tref == x->exorder.fl))
						y->opcode = OC_NOOP;
					continue;
				}
				break;
			default:
				break;
		}
		if (NO_REF == (dest_type = x->destination.oprclass))	/* Note assignment */
		{
			oct = oc_tab[opc].octype;
			if ((oct & OCT_VALUE) && (x->backptr.que.fl != &x->backptr) && !(oct & OCT_CGSKIP))
			{
				if (!(oct & OCT_MVADDR) && (x->backptr.que.fl->que.fl == &x->backptr)
				    && (OC_STO == (y = x->backptr.que.fl->bkptr)->opcode) && (y->operand[1].oprval.tref == x)
				    && (OC_VAR == y->operand[0].oprval.tref->opcode))
				{
					x->destination = y->operand[0];
					y->opcode = OC_NOOP;
					y->operand[0].oprclass = y->operand[1].oprclass = NO_REF;
				} else
				{
					oct &= OCT_VALUE | OCT_MVADDR;
					switch(oct)
					{
					case OCT_MVAL:
						r = TVAL_REF;
						break;
					case OCT_MINT:
					case OCT_BOOL:
						r = TINT_REF;
						break;
					case (OCT_MVADDR | OCT_MVAL):
						r = TVAD_REF;
						break;
					case OCT_CDADDR:
						r = TCAD_REF;
						break;
					default:
						assert(FALSE);
						break;
					}
					for (c = 0; tempcont[r][c] && (MAX_TEMP_COUNT > c); c++)
						;
					if (MAX_TEMP_COUNT <= c)
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TMPSTOREMAX);
					tempcont[r][c] = 1;
					x->destination.oprclass = r;
					x->destination.oprval.temp = c;
					if (c > temphigh[r])
						temphigh[r] = c;
					if (OC_CDADDR == x->opcode)
						x->opcode = OC_NOOP;
				}
			}
		} else if (TRIP_REF == dest_type)
		{
			assert(x->destination.oprval.tref->destination.oprclass);
			x->destination = x->destination.oprval.tref->destination;
		}
		for (j = x->operand, y = x; j < ARRAYTOP(y->operand); )
		{	/* Loop through all the parameters of the current opcode. For each parameter that requires an intermediate
			 * temporary, decrement (this is what remove_backptr does) the "reference count" -- opcodes yet to be
			 * processed that still need the intermediate result -- and if that number is zero, mark the temporary
			 * available. We can then reuse the temp to hold the results of subsequent opcodes. Note that remove_backptr
			 * is essentially the resolve_tref() in resolve_ref.c. resolve_tref increments the "reference count",
			 * while remove_backptr decrements it.
			 */
			if (TRIP_REF == j->oprclass)
			{
				ref = j->oprval.tref;
				if (OC_PARAMETER == ref->opcode)
				{
					y = ref;
					j = y->operand;
					continue;
				}
				remove_backptr(y, j, tempcont);
			}
			j++;
		}
	}
	for (r = 0; VALUED_REF_TYPES > r; r++)
		sa_temps[r] = temphigh[r] + 1;
	sa_temps_offset[TVAR_REF] = sa_temps[TVAR_REF] * sa_class_sizes[TVAR_REF];
	size = sa_temps[TVAL_REF] * sa_class_sizes[TVAL_REF];
	sa_temps_offset[TVAL_REF] = size;
	/* Since we need to align the temp region to the largest types, align even int temps to SIZEOF(char*) */
	size += ROUND_UP2(sa_temps[TINT_REF] *sa_class_sizes[TINT_REF], SIZEOF(char *));
	sa_temps_offset[TINT_REF] = size;
	size += sa_temps[TVAD_REF] * sa_class_sizes[TVAD_REF];
	sa_temps_offset[TVAD_REF] = size;
	size += sa_temps[TCAD_REF] * sa_class_sizes[TCAD_REF];
	sa_temps_offset[TCAD_REF] = size;
}

void remove_backptr(triple *curtrip, oprtype *opnd, char (*tempcont)[MAX_TEMP_COUNT])
{
	triple		*ref;
	tbp		*b;
	int		r;

	assert(TRIP_REF == opnd->oprclass);
	assert(OC_PASSTHRU != opnd->oprval.tref->opcode);
	ref = opnd->oprval.tref;
	r = ref->destination.oprclass;
	if (NO_REF != r)
	{
		dqloop(&ref->backptr, que, b)
		{
			if (b->bkptr == curtrip)
			{
				dqdel(b, que);
				break;
			}
		}
		if ((ref->backptr.que.fl == &ref->backptr) && (TVAR_REF != r))
			tempcont[r][ref->destination.oprval.temp] = 0;
	}
}
