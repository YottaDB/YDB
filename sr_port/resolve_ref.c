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

#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "cmd_qlf.h"
#include "mmemory.h"
#include "resolve_lab.h"
#include "cdbg_dump.h"
#include "gtmdbglvl.h"

GBLREF int4		source_error_found;
GBLREF boolean_t	run_time;
GBLREF triple		t_orig;
GBLREF mlabel		*mlabtab;
GBLREF command_qualifier cmd_qlf;
GBLREF uint4		gtmDebugLevel;

int resolve_ref(int errknt)
{
	triple	*curtrip, *tripref, *chktrip;
	tbp	*tripbp;
	mline	*mxl;
	mlabel	*mlbx;
	oprtype *opnd;
	int	actcnt;

	error_def(ERR_LABELMISSING);
	error_def(ERR_LABELUNKNOWN);
	error_def(ERR_FMLLSTPRESENT);
	error_def(ERR_FMLLSTMISSING);
	error_def(ERR_ACTLSTTOOLONG);

	if (errknt && !(cmd_qlf.qlf & CQ_IGNORE))
	{
		assert(!run_time);
		walktree((mvar *)mlabtab, resolve_lab, (char *)&errknt);
	} else
	{
		COMPDBG(printf(" ************************************* Begin resolve_ref scan ******************************\n"););
		dqloop(&t_orig, exorder, curtrip)
		{
			COMPDBG(printf(" ************************ Triple Start **********************\n"););
			COMPDBG(cdbg_dump_triple(curtrip, 0););
			for (opnd = curtrip->operand ; opnd < &(curtrip->operand[2]) ; opnd++)
			{
				if (opnd->oprclass == INDR_REF)
					*opnd = *(opnd->oprval.indr);
				switch (opnd->oprclass)
				{
				case TNXT_REF:
					opnd->oprclass = TJMP_REF;
					opnd->oprval.tref = opnd->oprval.tref->exorder.fl;
					/* caution:  fall through */
				case TJMP_REF:
					tripbp = (tbp *) mcalloc(sizeof(tbp));
					tripbp->bpt = curtrip;
					dqins(&opnd->oprval.tref->jmplist, que, tripbp);
					continue;
				case MNXL_REF:
					mxl = opnd->oprval.mlin->child;
					if (mxl)
						tripref = mxl->externalentry;
					else
						tripref = 0;
					if (!tripref)
					{	/* ignore vacuuous DO sp sp */
						curtrip->opcode = OC_NOOP;
						break;
					}
					opnd->oprclass = TJMP_REF;
					opnd->oprval.tref = tripref;
					break;
				case MLAB_REF:
					assert(!run_time);
					mlbx = opnd->oprval.lab;
					tripref = mlbx->ml ? mlbx->ml->externalentry : 0;
					if (tripref)
					{
						if (mlbx->formalcnt == NO_FORMALLIST)
						{
							opnd->oprclass = TJMP_REF;
							opnd->oprval.tref = tripref;
						} else
						{
							errknt++;
							stx_error(ERR_FMLLSTPRESENT, 2, mlbx->mvname.len, mlbx->mvname.addr);
							source_error_found = 0;
							tripref = newtriple(OC_RTERROR);
							tripref->operand[0] = put_ilit(ERR_FMLLSTPRESENT);
							/* This is a subroutine/func reference */
							tripref->operand[1] = put_ilit(TRUE);
							opnd->oprval.tref = tripref;
							opnd->oprclass = TJMP_REF;
						}
					} else
					{
						errknt++;
						stx_error(ERR_LABELMISSING, 2, mlbx->mvname.len, mlbx->mvname.addr);
						source_error_found = 0;
						tripref = newtriple(OC_RTERROR);
						tripref->operand[0] = put_ilit(ERR_LABELUNKNOWN);
						/* This is a subroutine/func reference */
						tripref->operand[1] = put_ilit(TRUE);
						opnd->oprval.tref = tripref;
						opnd->oprclass = TJMP_REF;
					}
					continue;
				case MFUN_REF:
					assert (!run_time);
					assert (curtrip->opcode == OC_JMP);
					chktrip = curtrip->exorder.bl;
					assert (chktrip->opcode == OC_EXCAL || chktrip->opcode == OC_EXFUN);
					assert (chktrip->operand[1].oprclass == TRIP_REF);
					chktrip = chktrip->operand[1].oprval.tref;
					assert (chktrip->opcode == OC_PARAMETER);
					assert (chktrip->operand[0].oprclass == TRIP_REF);
					assert (chktrip->operand[0].oprval.tref->opcode == OC_TRIPSIZE);
					assert (chktrip->operand[0].oprval.tref->operand[0].oprclass == TSIZ_REF);
					chktrip = chktrip->operand[1].oprval.tref;
					assert (chktrip->opcode == OC_PARAMETER);
					assert (chktrip->operand[0].oprclass == TRIP_REF);
					assert (chktrip->operand[0].oprval.tref->opcode == OC_ILIT);
					assert (chktrip->operand[0].oprval.tref->operand[0].oprclass == ILIT_REF);
					chktrip = chktrip->operand[1].oprval.tref;
					assert (chktrip->opcode == OC_PARAMETER);
					assert (chktrip->operand[0].oprclass == TRIP_REF);
					assert (chktrip->operand[0].oprval.tref->opcode == OC_ILIT);
					assert (chktrip->operand[0].oprval.tref->operand[0].oprclass == ILIT_REF);
					actcnt = chktrip->operand[0].oprval.tref->operand[0].oprval.ilit;
					assert (0 <= actcnt);
					mlbx = opnd->oprval.lab;
					tripref = mlbx->ml ? mlbx->ml->externalentry : 0;
					if (tripref)
					{
						if (mlbx->formalcnt == NO_FORMALLIST)
						{
							errknt++;
							stx_error(ERR_FMLLSTMISSING, 2, mlbx->mvname.len, mlbx->mvname.addr);
							source_error_found = 0;
							tripref = newtriple(OC_RTERROR);
							tripref->operand[0] = put_ilit(ERR_FMLLSTMISSING);
							/* This is a subroutine/func reference */
							tripref->operand[1] = put_ilit(TRUE);
							opnd->oprval.tref = tripref;
							opnd->oprclass = TJMP_REF;
						} else if (mlbx->formalcnt < actcnt)
						{
							errknt++;
							stx_error(ERR_ACTLSTTOOLONG, 2, mlbx->mvname.len, mlbx->mvname.addr);
							source_error_found = 0;
							tripref = newtriple(OC_RTERROR);
							tripref->operand[0] = put_ilit(ERR_ACTLSTTOOLONG);
							/* This is a subroutine/func reference */
							tripref->operand[1] = put_ilit(TRUE);
							opnd->oprval.tref = tripref;
							opnd->oprclass = TJMP_REF;
						} else
						{
							opnd->oprclass = TJMP_REF;
							opnd->oprval.tref = tripref;
						}
					} else
					{
						errknt++;
						stx_error(ERR_LABELMISSING, 2, mlbx->mvname.len, mlbx->mvname.addr);
						source_error_found = 0;
						tripref = newtriple(OC_RTERROR);
						tripref->operand[0] = put_ilit(ERR_LABELUNKNOWN);
						/* This is a subroutine/func reference */
						tripref->operand[1] = put_ilit(TRUE);
						opnd->oprval.tref = tripref;
						opnd->oprclass = TJMP_REF;
					}
					continue;
				case TRIP_REF:
					resolve_tref(curtrip, opnd);
					continue;
				}
			}
			opnd = &curtrip->destination;
			if (opnd->oprclass == TRIP_REF)
				resolve_tref(curtrip, opnd);
		}
	}
	return errknt;
}


/* If for example there are nested $SELECT routines feeding a value to a SET $PIECE/$EXTRACT, this nested checking is
   necessary to make sure no OC_PASSTHRUs remain in the parameter chain to get turned into OC_NOOPs that will
   cause GTMASSERTs in emit_code.
*/
void resolve_tref(triple *curtrip, oprtype *opnd)
{
	triple	*tripref;
	tbp	*tripbp;

	if (OC_PASSTHRU == (tripref = opnd->oprval.tref)->opcode)
	{
		assert(tripref->operand[0].oprclass == TRIP_REF);
		do
		{	/* As many OC_PASSTHRUs as are stacked, we will devour */
			*opnd = tripref->operand[0];
		} while (OC_PASSTHRU == (tripref = opnd->oprval.tref)->opcode);
	}

	COMPDBG(printf(" ** Passthru replacement: Operand at 0x%08lx replaced by operand at 0x%08lx\n",
		       opnd, &tripref->operand[0]););

	tripbp = (tbp *) mcalloc(sizeof(tbp));
	tripbp->bpt = curtrip;
	dqins(&opnd->oprval.tref->backptr, que, tripbp);
}
