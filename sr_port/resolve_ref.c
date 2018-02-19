/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "cmd_qlf.h"
#include "mmemory.h"
#include "resolve_lab.h"
#include "cdbg_dump.h"
#include "gtmdbglvl.h"

GBLREF boolean_t		run_time;
GBLREF command_qualifier	cmd_qlf;
GBLREF mlabel			*mlabtab;
GBLREF triple			t_orig;
GBLREF uint4			gtmDebugLevel;

error_def(ERR_ACTLSTTOOLONG);
error_def(ERR_FMLLSTMISSING);
error_def(ERR_LABELMISSING);
error_def(ERR_LABELNOTFND);
error_def(ERR_LABELUNKNOWN);

int resolve_ref(int errknt)
{	/* simplify operand references where possible and make literals dynamic when in that mode */
	int	actcnt;
	mlabel	*mlbx;
	mline	*mxl;
	oprtype *opnd;
	tbp	*tripbp;
	triple	*chktrip, *curtrip, *tripref;
#	ifndef i386
	oprtype *j, *k;
	triple	*ref, *ref1;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (errknt && !(cmd_qlf.qlf & CQ_IGNORE))
	{
		assert(!run_time);
		walktree((mvar *)mlabtab, resolve_lab, (char *)&errknt);
	} else
	{
#		ifndef i386
		if (!run_time && (cmd_qlf.qlf & CQ_DYNAMIC_LITERALS))
		{	/* OC_LIT --> OC_LITC wherever OC_LIT is actually used, i.e. not a dead end */
			dqloop(&t_orig, exorder, curtrip)
			{
				switch (curtrip->opcode)
				{	/* Do a few literal optimizations typically done later in alloc_reg. It's convenient to
					 * check for OC_LIT parameters here, before we start sliding OC_LITC opcodes in the way.
					 */
					case OC_NOOP:
					case OC_PARAMETER:
					case OC_LITC:	/* possibly already inserted in bx_boolop */
						continue;
					case OC_STO:	/* see counterpart in alloc_reg.c */
						if ((cmd_qlf.qlf & CQ_INLINE_LITERALS)
						    && (TRIP_REF == curtrip->operand[1].oprclass)
						    && (OC_LIT == curtrip->operand[1].oprval.tref->opcode))
						{
							curtrip->opcode = OC_STOLITC;
							continue;
						}
						break;
					case OC_EQU:	/* see counterpart in alloc_reg.c */
						if ((TRIP_REF == curtrip->operand[0].oprclass)
						    && (OC_LIT == curtrip->operand[0].oprval.tref->opcode)
						    && (0 == curtrip->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len))
						{
							curtrip->operand[0] = curtrip->operand[1];
							curtrip->operand[1].oprclass = NO_REF;
							curtrip->opcode = OC_EQUNUL;
							continue;
						} else if ((TRIP_REF == curtrip->operand[1].oprclass)
						    && (OC_LIT == curtrip->operand[1].oprval.tref->opcode)
						    && (0 == curtrip->operand[1].oprval.tref->operand[0].oprval.mlit->v.str.len))
						{
							curtrip->operand[1].oprclass = NO_REF;
							curtrip->opcode = OC_EQUNUL;
							continue;
						}
						break;
					case OC_FNTEXT:
						if (resolve_optimize(curtrip))	/* only deals with $TEXT(), but could do more */
						{
							curtrip = curtrip->exorder.bl;	/* Backup to rescan change to curtrip */
							continue;
						}	/* WARNING fallthrough*/
					default:
						break;
				}
				for (j = curtrip->operand, ref = curtrip; j < ARRAYTOP(ref->operand); )
				{	/* Iterate over all parameters of the current triple */
					k = j;
					while (INDR_REF == k->oprclass)
						k = k->oprval.indr;
					if (TRIP_REF == k->oprclass)
					{
						tripref = k->oprval.tref;
						if (OC_PARAMETER == tripref->opcode)
						{
							ref = tripref;
							j = ref->operand;
							continue;
						}
						if (OC_LIT == tripref->opcode)
						{	/* Insert an OC_LITC to relay the OC_LIT result to curtrip */
							ref1 = maketriple(OC_LITC);
							ref1->src = tripref->src;
							ref1->operand[0] = put_tref(tripref);
							dqins(curtrip->exorder.bl, exorder, ref1);
							*k = put_tref(ref1);
						}
					}
					j++;
				}
			}
		}
#		endif
		COMPDBG(PRINTF("\n\n\n********************* New Compilation -- Begin resolve_ref scan **********************\n"););
		dqloop(&t_orig, exorder, curtrip)
		{	/* If the optimization was not executed earlier */
			COMPDBG(PRINTF(" ************************ Triple Start **********************\n"););
			COMPDBG(cdbg_dump_triple(curtrip, 0););
#			ifndef i386
			if (!run_time && !(cmd_qlf.qlf & CQ_DYNAMIC_LITERALS))
			{
#			endif
				if ((OC_FNTEXT == curtrip->opcode) && (resolve_optimize(curtrip)))
				{ /* resolve_optimize now only deals with $TEXT(), but could do more by removing 1st term below */
					curtrip = curtrip->exorder.bl;	/* Backup to rescan after changing curtrip */
					continue;
				}
#			ifndef i386
			}
#			endif
			for (opnd = curtrip->operand; opnd < ARRAYTOP(curtrip->operand); opnd++)
			{
				if (INDR_REF == opnd->oprclass)
					*opnd = *(opnd->oprval.indr);
				switch (opnd->oprclass)
				{
				case TNXT_REF:
					opnd->oprclass = TJMP_REF;
					opnd->oprval.tref = opnd->oprval.tref->exorder.fl;
					/* caution:  fall through */
				case TJMP_REF:
					tripbp = (tbp *)mcalloc(SIZEOF(tbp));
					tripbp->bpt = curtrip;
					dqins(&opnd->oprval.tref->jmplist, que, tripbp);
					continue;
				case MNXL_REF:	/* external reference to the routine (not within the routine) */
					mxl = opnd->oprval.mlin->child;
					tripref = mxl ? mxl->externalentry : NULL;
					if (!tripref)
					{	/* ignore vacuous DO sp sp */
						curtrip->opcode = OC_NOOP;
						break;
					}
					opnd->oprclass = TJMP_REF;
					opnd->oprval.tref = tripref;
					break;
				case MLAB_REF:	/* target label should have no parms; this is DO without parens or args */
					assert(!run_time);
					mlbx = opnd->oprval.lab;
					tripref = mlbx->ml ? mlbx->ml->externalentry : NULL;
					if (tripref)
					{
						opnd->oprclass = TJMP_REF;
						opnd->oprval.tref = tripref;
					} else
					{
						errknt++;
						stx_error(ERR_LABELMISSING, 2, mlbx->mvname.len, mlbx->mvname.addr);
						TREF(source_error_found) = 0;
						tripref = newtriple(OC_RTERROR);
						tripref->operand[0] = put_ilit(OC_JMP == curtrip->opcode
							? ERR_LABELNOTFND : ERR_LABELUNKNOWN); /* special error for GOTO jmp */
						tripref->operand[1] = put_ilit(TRUE);	/* This is a subroutine/func reference */
						opnd->oprval.tref = tripref;
						opnd->oprclass = TJMP_REF;
					}
					continue;
				case MFUN_REF:
					assert(!run_time);
					assert(OC_JMP == curtrip->opcode);
					chktrip = curtrip->exorder.bl;
					assert((OC_EXCAL == chktrip->opcode) || (OC_EXFUN == chktrip->opcode));
					assert(TRIP_REF == chktrip->operand[1].oprclass);
					chktrip = chktrip->operand[1].oprval.tref;
					assert(OC_PARAMETER == chktrip->opcode);
					assert(TRIP_REF == chktrip->operand[0].oprclass);
#					ifdef __i386
					assert(OC_ILIT == chktrip->operand[0].oprval.tref->opcode);
					assert(ILIT_REF == chktrip->operand[0].oprval.tref->operand[0].oprclass);
#					else
					assert(OC_TRIPSIZE == chktrip->operand[0].oprval.tref->opcode);
					assert(TSIZ_REF == chktrip->operand[0].oprval.tref->operand[0].oprclass);
#					endif
					chktrip = chktrip->operand[1].oprval.tref;
					assert(OC_PARAMETER == chktrip->opcode);
					assert(TRIP_REF == chktrip->operand[0].oprclass);
					assert(OC_ILIT == chktrip->operand[0].oprval.tref->opcode);
					assert(ILIT_REF == chktrip->operand[0].oprval.tref->operand[0].oprclass);
#					ifndef __i386
					chktrip = chktrip->operand[1].oprval.tref;
					assert(OC_PARAMETER == chktrip->opcode);
					assert(TRIP_REF == chktrip->operand[0].oprclass);
					assert(OC_ILIT == chktrip->operand[0].oprval.tref->opcode);
					assert(ILIT_REF == chktrip->operand[0].oprval.tref->operand[0].oprclass);
#					endif
					actcnt = chktrip->operand[0].oprval.tref->operand[0].oprval.ilit;
					assert(0 <= actcnt);
					mlbx = opnd->oprval.lab;
					tripref = mlbx->ml ? mlbx->ml->externalentry : 0;
					if (tripref)
					{
						if (NO_FORMALLIST == mlbx->formalcnt)
						{
							errknt++;
							stx_error(ERR_FMLLSTMISSING, 2, mlbx->mvname.len, mlbx->mvname.addr);
							TREF(source_error_found) = 0;
							tripref = newtriple(OC_RTERROR);
							tripref->operand[0] = put_ilit(ERR_FMLLSTMISSING);
							tripref->operand[1] = put_ilit(TRUE);	/* subroutine/func reference */
							opnd->oprval.tref = tripref;
							opnd->oprclass = TJMP_REF;
						} else if (mlbx->formalcnt < actcnt)
						{
							errknt++;
							stx_error(ERR_ACTLSTTOOLONG, 2, mlbx->mvname.len, mlbx->mvname.addr);
							TREF(source_error_found) = 0;
							tripref = newtriple(OC_RTERROR);
							tripref->operand[0] = put_ilit(ERR_ACTLSTTOOLONG);
							tripref->operand[1] = put_ilit(TRUE);	/* subroutine/func reference */
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
						TREF(source_error_found) = 0;
						tripref = newtriple(OC_RTERROR);
						tripref->operand[0] = put_ilit(ERR_LABELUNKNOWN);
						tripref->operand[1] = put_ilit(TRUE);	/* subroutine/func reference */
						opnd->oprval.tref = tripref;
						opnd->oprclass = TJMP_REF;
					}
					continue;
				case TRIP_REF:
					resolve_tref(curtrip, opnd);
					continue;
				default:
					break;
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
 * necessary to make sure no OC_PASSTHRUs remain in the parameter chain to get turned into OC_NOOPs that will
 * cause assertpro in emit_code.
 */
void resolve_tref(triple *curtrip, oprtype *opnd)
{
	triple	*tripref;
	tbp	*tripbp;

	while (OC_PASSTHRU == (tripref = opnd->oprval.tref)->opcode)		/* note the assignment */
	{	/* As many OC_PASSTHRUs as are stacked, we devour */
		COMPDBG(PRINTF(" ** Passthru replacement: Operand at 0x%08lx replaced by operand at 0x%08lx\n",
			       (unsigned long)opnd, (unsigned long)&tripref->operand[0]););
		assert(TRIP_REF == tripref->operand[0].oprclass);
		*opnd = tripref->operand[0];
	}
	tripbp = (tbp *)mcalloc(SIZEOF(tbp));
	tripbp->bpt = curtrip;
	dqins(&opnd->oprval.tref->backptr, que, tripbp);
}
