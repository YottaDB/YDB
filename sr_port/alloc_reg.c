/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

GBLDEF int4			sa_temps[VALUED_REF_TYPES];
GBLDEF int4			sa_temps_offset[VALUED_REF_TYPES];

GBLREF int			mvmax;
GBLREF triple			t_orig;
GBLREF uint4			gtmDebugLevel;
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

error_def(ERR_TMPSTOREMAX);

STATICFNDCL void remove_backptr(triple *curtrip, oprtype *opnd, char (*tempcont)[MAX_TEMP_COUNT]);

void alloc_reg(void)
{
	triple		*x, *y, *ref;
	tbp		*b;
	oprtype 	*j;
	opctype 	opc, opx;
	char		tempcont[VALUED_REF_TYPES][MAX_TEMP_COUNT], dest_type;
	int		r, c, temphigh[VALUED_REF_TYPES];
	unsigned int	oct;
	int4		size;

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
					opc = x->opcode = OC_NOOP;
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
					opc = x->opcode = OC_NOOP;
					COMPDBG(PRINTF("   ** Converting triple to NOOP (rsn 2) **\n"););
					continue;	/* continue, because 'normal' NOOP continues from this switch */
				}
#				ifndef DEBUG
				break;
#				endif
			case OC_LINEFETCH:
#				ifdef DEBUG
				for (c = temphigh[TVAL_REF]; 0 <= c; c--)
					assert(0 == tempcont[TVAL_REF][c]);	/* check against leaking TVAL temps */
				if (OC_LINESTART == opc)
					break;
#				endif
			case OC_FETCH:
				assert((TRIP_REF == x->operand[0].oprclass) && (OC_ILIT == x->operand[0].oprval.tref->opcode));
				if (x->operand[0].oprval.tref->operand[0].oprval.ilit == mvmax)
				{
					x->operand[0].oprval.tref->operand[0].oprval.ilit = 0;
					x->operand[1].oprclass = NO_REF;
				}
				break;
			case OC_STO:
				/* If we are storing a literal e.g. s x="hi", don't call op_sto, because we do not
				 * need to check if the literal is defined.  OC_STOLIT will be an in-line copy.
				 * Bypass this if we have been requested to not do inline literals.
				 */
				if ((cmd_qlf.qlf & CQ_INLINE_LITERALS) && (TRIP_REF == x->operand[1].oprclass)
				    && (OC_LIT == x->operand[1].oprval.tref->opcode))
					opc = x->opcode = OC_STOLIT;
				break;
			case OC_EQU:
				/* Check to see if the operation is a x="" or a ""=x, if so (and this is a very common case)
				 * use special opcode OC_EQUNUL, which takes one argument and just checks length for zero
				 */
				if ((TRIP_REF == x->operand[0].oprclass) && (OC_LIT == x->operand[0].oprval.tref->opcode)
				    && (0 == x->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len))
				{
					x->operand[0] = x->operand[1];
					x->operand[1].oprclass = NO_REF;
					opc = x->opcode = OC_EQUNUL;
				} else if ((TRIP_REF == x->operand[1].oprclass) && (OC_LIT == x->operand[1].oprval.tref->opcode)
					   && (0 == x->operand[1].oprval.tref->operand[0].oprval.mlit->v.str.len))
				{
					x->operand[1].oprclass = NO_REF;
					opc = x->opcode = OC_EQUNUL;
				}
				break;
		}
		if (OC_PASSTHRU == x->opcode)
		{
			COMPDBG(PRINTF(" *** OC_PASSTHRU opcode being NOOP'd\n"););
			remove_backptr(x, &x->operand[0], tempcont);
			x->opcode = OC_NOOP;
			continue;
		}
		if (NO_REF == (dest_type = x->destination.oprclass))	/* Note assignment */
		{
			oct = oc_tab[opc].octype;
			if ((oct & OCT_VALUE) && (x->backptr.que.fl != &x->backptr) && !(oct & OCT_CGSKIP))
			{
				if (!(oct & OCT_MVADDR) && (x->backptr.que.fl->que.fl == &x->backptr)
				    && (OC_STO == (y = x->backptr.que.fl->bpt)->opcode) && (y->operand[1].oprval.tref == x)
				    && (OC_VAR == y->operand[0].oprval.tref->opcode))
				{
					x->destination = y->operand[0];
					y->opcode = OC_NOOP;
					y->operand[0].oprclass = y->operand[1].oprclass = NO_REF;
				} else
				{
					oct &= OCT_VALUE | OCT_MVADDR;
					assert((OCT_MVAL == oct) || (OCT_MINT == oct) || ((OCT_MVADDR | OCT_MVAL) == oct)
						|| (OCT_CDADDR == oct));
					r = (OCT_MVAL == oct) ? TVAL_REF : (((OCT_MVADDR | OCT_MVAL) == oct)
						? TVAD_REF : ((OCT_MINT == oct) ? TINT_REF : TCAD_REF));
					for (c = 0; tempcont[r][c] && (MAX_TEMP_COUNT > c); c++)
						;
					if (MAX_TEMP_COUNT <= c)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TMPSTOREMAX);
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
	ref = opnd->oprval.tref;
	while (OC_PASSTHRU == opnd->oprval.tref->opcode)
	{
		ref = ref->operand[0].oprval.tref;
		opnd = &ref->operand[0];
		assert(TRIP_REF == opnd->oprclass);
	}
	r = ref->destination.oprclass;
	if (NO_REF != r)
	{
		dqloop(&ref->backptr, que, b)
		{
			if (b->bpt == curtrip)
			{
				dqdel(b, que);
				break;
			}
		}
		if ((ref->backptr.que.fl == &ref->backptr) && (TVAR_REF != r))
			tempcont[r][ref->destination.oprval.temp] = 0;
	}
}
