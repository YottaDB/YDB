/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

#include "cmd_qlf.h"
#include "gtmdbglvl.h"
#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "alloc_reg.h"
#include "cdbg_dump.h"

#define MAX_TEMP_COUNT 128

GBLREF int			mvmax;
LITREF octabstruct 		oc_tab[];
GBLREF triple			t_orig;
GBLREF uint4			gtmDebugLevel;
GBLREF command_qualifier  	cmd_qlf;

GBLDEF int4 sa_temps[VALUED_REF_TYPES];
GBLDEF int4 sa_temps_offset[VALUED_REF_TYPES];
LITDEF int4 sa_class_sizes[VALUED_REF_TYPES] =
{
	0		/* dummy for slot zero */
	,sizeof(mval*)	/* TVAR_REF */
	,sizeof(mval)	/* TVAL_REF */
	,sizeof(mint)	/* TINT_REF */
	,sizeof(mval*)	/* TVAD_REF */
	,sizeof(char*)	/* TCAD_REF */
};

void alloc_reg(void)
{
	triple	*x, *y, *ref;
	tbp	*b;
	oprtype *j;
	opctype opc, opx;
	char	tempcont[VALUED_REF_TYPES][MAX_TEMP_COUNT], dest_type;
	int	r, c, temphigh[VALUED_REF_TYPES];
	unsigned int oct;
	int4	size;
	error_def(ERR_TMPSTOREMAX);

	memset(&tempcont[0][0], 0, sizeof(tempcont));
	memset(&temphigh[0], -1, sizeof(temphigh));
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
			/* If the next triple is also a LINESTART, then this is a comment line.  Therefore
				eliminate this LINESTART */
			opx = x->exorder.fl->opcode;
			if (opx == OC_LINESTART || opx == OC_LINEFETCH || opx == OC_ISFORMAL)
			{
				opc = x->opcode = OC_NOOP;
				COMPDBG(PRINTF("   ** Converting triple to NOOP (rsn 1) **\n"););
				continue;	/* continue, because 'normal' NOOP continues from this switch */
			}
			/* There is a special case in the case of NOLINE_ENTRY being specified. If a blank line is followed
			   by a line with a label and that label generates fetch information, the generated triple sequence
			   will be LINESTART (from blank line), ILIT (count from PREVIOUS fetch), LINEFETCH. We will detect
			   that sequence here and change the LINESTART to a NOOP.
			*/
			if (!(cmd_qlf.qlf & CQ_LINE_ENTRY) && OC_ILIT == opx && x->exorder.fl->exorder.fl \
			    && OC_LINEFETCH == x->exorder.fl->exorder.fl->opcode)
			{
				opc = x->opcode = OC_NOOP;
				COMPDBG(PRINTF("   ** Converting triple to NOOP (rsn 2) **\n"););
				continue;	/* continue, because 'normal' NOOP continues from this switch */
			}
			break;
		case OC_LINEFETCH:
		case OC_FETCH:
			assert(x->operand[0].oprclass == TRIP_REF &&
				x->operand[0].oprval.tref->opcode == OC_ILIT);
			if (x->operand[0].oprval.tref->operand[0].oprval.ilit == mvmax)
			{
				x->operand[0].oprval.tref->operand[0].oprval.ilit = 0;
				x->operand[1].oprclass = 0;
			}
			break;
		case OC_STO:
			/* If we are storing a literal e.g. s x="hi", don't call op_sto, because we do not
			   need to check if the literal is defined.  OC_STOLIT will be an in-line copy.
			   Bypass this if we have been requested to not do inline literals.
			*/
			if ((cmd_qlf.qlf & CQ_INLINE_LITERALS) && x->operand[1].oprclass == TRIP_REF &&
			    x->operand[1].oprval.tref->opcode == OC_LIT)
				opc = x->opcode = OC_STOLIT;
			break;
		case OC_EQU:
			/* Check to see if the operation is a x="" or a ""=x, if so (and this is a very common case)
				use special opcode OC_EQUNUL, which takes one argument and just checks length for zero */
			if (x->operand[0].oprclass == TRIP_REF && x->operand[0].oprval.tref->opcode == OC_LIT &&
				x->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len == 0)
			{
				x->operand[0] = x->operand[1];
				x->operand[1].oprclass = 0;
				opc = x->opcode = OC_EQUNUL;
			} else if (x->operand[1].oprclass == TRIP_REF && x->operand[1].oprval.tref->opcode == OC_LIT &&
					x->operand[1].oprval.tref->operand[0].oprval.mlit->v.str.len == 0)
			{
				x->operand[1].oprclass = 0;
				opc = x->opcode = OC_EQUNUL;
			}
			break;
		}
		for (j = x->operand, y = x; j < &(y->operand[2]) ;)
		{
			if (j->oprclass == TRIP_REF)
			{
				ref = j->oprval.tref;
				if (ref->opcode == OC_PARAMETER)
				{
					y = ref;
					j = y->operand;
					continue;
				}
				if (r = ref->destination.oprclass)
				{
					dqloop(&ref->backptr, que, b)
					{
						if (b->bpt == y)
						{
							dqdel(b, que);
							break;
						}
					}
					if (ref->backptr.que.fl == &ref->backptr && r != TVAR_REF)
						tempcont[r][j->oprval.tref->destination.oprval.temp] = 0;
				}
			}
			j++;
		}
		if (x->opcode == OC_PASSTHRU)
		{
			COMPDBG(PRINTF(" *** OC_PASSTHRU opcode being NOOP'd\n"););
			x->opcode = OC_NOOP;
			continue;
		}
		if (!(dest_type = x->destination.oprclass))
		{
			oct = oc_tab[opc].octype;
			if (oct & OCT_VALUE && x->backptr.que.fl != &x->backptr && !(oct & OCT_CGSKIP))
			{
				if (!(oct & OCT_MVADDR) && x->backptr.que.fl->que.fl == &x->backptr
				    && (y = x->backptr.que.fl->bpt)->opcode == OC_STO && y->operand[1].oprval.tref == x
				    && y->operand[0].oprval.tref->opcode == OC_VAR)
				{
					x->destination = y->operand[0];
					y->opcode = OC_NOOP;
					y->operand[0].oprclass = y->operand[1].oprclass = 0;
				} else
				{
					oct &= OCT_VALUE | OCT_MVADDR;
					assert (oct == OCT_MVAL || oct == OCT_MINT || oct == (OCT_MVADDR | OCT_MVAL)
					    || oct == OCT_CDADDR);
					r = (oct == OCT_MVAL) ? TVAL_REF : ((oct == (OCT_MVADDR | OCT_MVAL)) ? TVAD_REF
					    : ((oct == OCT_MINT) ? TINT_REF : TCAD_REF));
					for (c = 0; tempcont[r][c] && c < MAX_TEMP_COUNT ;c++)
						;
					if (c >= MAX_TEMP_COUNT)
						rts_error(VARLSTCNT(1) ERR_TMPSTOREMAX);
					tempcont[r][c] = 1;
					x->destination.oprclass = r;
					x->destination.oprval.temp = c;
					if (c > temphigh[r])
						temphigh[r] = c;
					if (x->opcode == OC_CDADDR)
						x->opcode = OC_NOOP;
				}
			}
		} else if (dest_type == TRIP_REF)
		{
			assert(x->destination.oprval.tref->destination.oprclass);
			x->destination = x->destination.oprval.tref->destination;
		}
	}
	for (r = 0; r < VALUED_REF_TYPES ;r++)
		sa_temps[r] = temphigh[r] + 1;

	sa_temps_offset[TVAR_REF] = sa_temps[TVAR_REF] * sa_class_sizes[TVAR_REF];

	size = sa_temps[TVAL_REF] * sa_class_sizes[TVAL_REF];
	sa_temps_offset[TVAL_REF] = size;
	/* Since we need to align the temp region to the largest types, align even int temps to sizeof(char*) */
	size += ROUND_UP2(sa_temps[TINT_REF] * sa_class_sizes[TINT_REF], sizeof(char *));
	sa_temps_offset[TINT_REF] = size;
	size += sa_temps[TVAD_REF] * sa_class_sizes[TVAD_REF];
	sa_temps_offset[TVAD_REF] = size;
	size += sa_temps[TCAD_REF] * sa_class_sizes[TCAD_REF];
	sa_temps_offset[TCAD_REF] = size;
}
