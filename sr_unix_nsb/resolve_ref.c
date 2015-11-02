/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "mdq.h"
#include "opcode.h"
#include "cmd_qlf.h"
#include "mmemory.h"
#include "resolve_lab.h"
#include "cdbg_dump.h"
#include "gtmdbglvl.h"

GBLREF boolean_t		run_time;
GBLREF triple			t_orig;
GBLREF mlabel			*mlabtab;
GBLREF command_qualifier	cmd_qlf;
GBLREF uint4			gtmDebugLevel;

error_def(ERR_LABELMISSING);
error_def(ERR_LABELUNKNOWN);
error_def(ERR_FMLLSTMISSING);
error_def(ERR_ACTLSTTOOLONG);

int resolve_ref(int errknt)
{
	triple	*x,*y,*z;
	tbp	*p;
	mline	*mxl;
	mlabel	*mlbx;
	oprtype *n;
	int4	in_error;
	int	actcnt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (errknt && !(cmd_qlf.qlf & CQ_IGNORE))
	{
		assert(!run_time);
		walktree((mvar *)mlabtab, resolve_lab, (char *)&errknt);
	} else
	{
		COMPDBG(PRINTF(" ************************************* Begin resolve_ref scan ******************************\n"););
		dqloop(&t_orig, exorder, x)
		{
			COMPDBG(PRINTF(" ************************ Triple Start **********************\n"););
			COMPDBG(cdbg_dump_triple(x, 0);)
			for (n = x->operand; n < ARRAYTOP(x->operand); n++)
			{
				if (n->oprclass == INDR_REF)
					*n = *(n->oprval.indr);
				switch (n->oprclass)
				{
				case TNXT_REF:
					n->oprclass = TJMP_REF;
					n->oprval.tref = n->oprval.tref->exorder.fl;
					/* caution:  fall through */
				case TJMP_REF:
					p = (tbp *) mcalloc(SIZEOF(tbp));
					p->bpt = x;
					dqins(&n->oprval.tref->jmplist, que, p);
					continue;
				case MNXL_REF:
					mxl = n->oprval.mlin->child;
					y = mxl ? mxl->externalentry : 0;
					if (!y)
					{	/* ignore vacuuous DO sp sp */
						x->opcode = OC_NOOP;
						break;
					}
					n->oprclass = TJMP_REF;
					n->oprval.tref = y;
					break;
				case MLAB_REF:
					assert(!run_time);
					mlbx = n->oprval.lab;
					y = mlbx->ml ? mlbx->ml->externalentry : 0;
					if (y)
					{
						n->oprclass = TJMP_REF;
						n->oprval.tref = y;
					} else
					{
						errknt++;
						stx_error(ERR_LABELMISSING, 2, mlbx->mvname.len, mlbx->mvname.addr);
						TREF(source_error_found) = 0;
						y = newtriple(OC_RTERROR);
						y->operand[0] = put_ilit(ERR_LABELUNKNOWN);
						y->operand[1] = put_ilit(TRUE);	/* This is a subroutine/func reference */
						n->oprval.tref = y;
						n->oprclass = TJMP_REF;
					}
					continue;
				case MFUN_REF:
					assert(!run_time);
					assert(OC_JMP == x->opcode);
					z = x->exorder.bl;
					assert((OC_EXCAL == z->opcode) || (OC_EXFUN == z->opcode));
					assert(TRIP_REF == z->operand[1].oprclass);
					z = z->operand[1].oprval.tref;
					assert(OC_PARAMETER == z->opcode);
					assert(TRIP_REF == z->operand[0].oprclass);
					assert(OC_ILIT == z->operand[0].oprval.tref->opcode);
					assert(ILIT_REF == z->operand[0].oprval.tref->operand[0].oprclass);
					z = z->operand[1].oprval.tref;
					assert(OC_PARAMETER == z->opcode);
					assert(TRIP_REF == z->operand[0].oprclass);
					assert(OC_ILIT == z->operand[0].oprval.tref->opcode);
					assert(ILIT_REF == z->operand[0].oprval.tref->operand[0].oprclass);
					actcnt = z->operand[0].oprval.tref->operand[0].oprval.ilit;
					assert(0 <= actcnt);
					mlbx = n->oprval.lab;
					y = mlbx->ml ? mlbx->ml->externalentry : 0;
					if (y)
					{
						if (mlbx->formalcnt == NO_FORMALLIST)
						{
							errknt++;
							stx_error(ERR_FMLLSTMISSING, 2, mlbx->mvname.len, mlbx->mvname.addr);
							TREF(source_error_found) = 0;
							y = newtriple(OC_RTERROR);
							y->operand[0] = put_ilit(ERR_FMLLSTMISSING);
							y->operand[1] = put_ilit(TRUE);	/* This is a subroutine/func reference */
							n->oprval.tref = y;
							n->oprclass = TJMP_REF;
						} else if (mlbx->formalcnt < actcnt)
						{
							errknt++;
							stx_error(ERR_ACTLSTTOOLONG, 2, mlbx->mvname.len, mlbx->mvname.addr);
							TREF(source_error_found) = 0;
							y = newtriple(OC_RTERROR);
							y->operand[0] = put_ilit(ERR_ACTLSTTOOLONG);
							y->operand[1] = put_ilit(TRUE);	/* This is a subroutine/func reference */
							n->oprval.tref = y;
							n->oprclass = TJMP_REF;
						} else
						{
							n->oprclass = TJMP_REF;
							n->oprval.tref = y;
						}
					} else
					{
						errknt++;
						stx_error(ERR_LABELMISSING, 2, mlbx->mvname.len, mlbx->mvname.addr);
						TREF(source_error_found) = 0;
						y = newtriple(OC_RTERROR);
						y->operand[0] = put_ilit(ERR_LABELUNKNOWN);
						y->operand[1] = put_ilit(TRUE);	/* This is a subroutine/func reference */
						n->oprval.tref = y;
						n->oprclass = TJMP_REF;
					}
					continue;
				case TRIP_REF:
					resolve_tref(x, n);
					continue;
				}
			}
			n = &x->destination;
			if (n->oprclass == TRIP_REF)
				resolve_tref(x, n);
		}
	}
	return errknt;
}


/* If for example there are nested $SELECT routines feeding a value to a SET $PIECE/$EXTRACT, this nested checking is
 * necessary to make sure no OC_PASSTHRUs remain in the parameter chain to get turned into OC_NOOPs that will
 * cause GTMASSERTs in emit_code.
 */
void resolve_tref(triple *curtrip, oprtype *opnd)
{
	triple	*tripref;
	tbp	*tripbp;

	if (OC_PASSTHRU == (tripref = opnd->oprval.tref)->opcode)		/* note the assignment */
	{
		assert(tripref->operand[0].oprclass == TRIP_REF);
		do
			/* As many OC_PASSTHRUs as are stacked, we will devour */
			*opnd = tripref->operand[0];
		while (OC_PASSTHRU == (tripref = opnd->oprval.tref)->opcode);	/* note the assignment */
	}
	COMPDBG(PRINTF(" ** Passthru replacement: Operand at 0x%08lx replaced by operand at 0x%08lx\n",
		       opnd, &tripref->operand[0]););
	tripbp = (tbp *) mcalloc(SIZEOF(tbp));
	tripbp->bpt = curtrip;
	dqins(&opnd->oprval.tref->backptr, que, tripbp);
}
