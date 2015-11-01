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
#include "mdq.h"
#include "opcode.h"
#include "cmd_qlf.h"
#include "mmemory.h"
#include "resolve_lab.h"

GBLREF int4 source_error_found;
GBLREF bool run_time;
GBLREF triple t_orig;
GBLREF mlabel *mlabtab;
GBLREF command_qualifier cmd_qlf;

int resolve_ref(int errknt)
{
	triple *x,*y,*z;
	tbp *p;
	mline *mxl;
	mlabel *mlbx;
	oprtype *n;
	int4	in_error;
	int	actcnt;
	error_def(ERR_LABELMISSING);
	error_def(ERR_LABELUNKNOWN);
	error_def	(ERR_FMLLSTPRESENT);
	error_def	(ERR_FMLLSTMISSING);
	error_def	(ERR_ACTLSTTOOLONG);

	if (errknt && !(cmd_qlf.qlf & CQ_IGNORE))
	{
		assert(!run_time);
		walktree((mvar *)mlabtab,resolve_lab,(char *)&errknt);
	}
	else
	{
		dqloop(&t_orig,exorder,x)
		{
			for (n = x->operand ; n < &(x->operand[2]) ; n++)
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
					p = (tbp *) mcalloc(sizeof(tbp));
					p->bpt = x;
					dqins(&n->oprval.tref->jmplist,que,p);
					continue;
				case MNXL_REF:
					mxl = n->oprval.mlin->child;
					if (mxl)
						y = mxl->externalentry;
					else
						y = 0;
					if (!y)
					{
						/* ignore vacuuous DO sp sp */
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
						if (mlbx->formalcnt == NO_FORMALLIST)
						{
							n->oprclass = TJMP_REF;
							n->oprval.tref = y;
						}
						else
						{
							errknt++;
							stx_error(ERR_FMLLSTPRESENT, 2, mid_len(&mlbx->mvname)
								,&mlbx->mvname);
							source_error_found = 0;
							y = maketriple(OC_RTERROR);
							y->operand[0] = put_ilit(ERR_FMLLSTPRESENT);
							ins_triple(y);
							n->oprval.tref = y;
							n->oprclass = TJMP_REF;
						}
					}
					else
					{
						errknt++;
						stx_error(ERR_LABELMISSING, 2, mid_len(&mlbx->mvname)
							,&mlbx->mvname);
						source_error_found = 0;
						y = maketriple(OC_RTERROR);
						y->operand[0] = put_ilit(ERR_LABELUNKNOWN);
						ins_triple(y);
						n->oprval.tref = y;
						n->oprclass = TJMP_REF;
					}
					continue;
				case MFUN_REF:
					assert (!run_time);
					assert (x->opcode == OC_JMP);
					z = x->exorder.bl;
					assert (z->opcode == OC_EXCAL || z->opcode == OC_EXFUN);
					assert (z->operand[1].oprclass == TRIP_REF);
					z = z->operand[1].oprval.tref;
					assert (z->opcode == OC_PARAMETER);
					assert (z->operand[0].oprclass == TRIP_REF);
					assert (z->operand[0].oprval.tref->opcode == OC_ILIT);
					assert (z->operand[0].oprval.tref->operand[0].oprclass == ILIT_REF);
					z = z->operand[1].oprval.tref;
					assert (z->opcode == OC_PARAMETER);
					assert (z->operand[0].oprclass == TRIP_REF);
					assert (z->operand[0].oprval.tref->opcode == OC_ILIT);
					assert (z->operand[0].oprval.tref->operand[0].oprclass == ILIT_REF);
					actcnt = z->operand[0].oprval.tref->operand[0].oprval.ilit;
					assert (0 <= actcnt);
					mlbx = n->oprval.lab;
					y = mlbx->ml ? mlbx->ml->externalentry : 0;
					if (y)
					{
						if (mlbx->formalcnt == NO_FORMALLIST)
						{
							errknt++;
							stx_error(ERR_FMLLSTMISSING, 2, mid_len(&mlbx->mvname)
								,&mlbx->mvname);
							source_error_found = 0;
							y = maketriple(OC_RTERROR);
							y->operand[0] = put_ilit(ERR_FMLLSTMISSING);
							ins_triple(y);
							n->oprval.tref = y;
							n->oprclass = TJMP_REF;
						}
						else
						if (mlbx->formalcnt < actcnt)
						{
							errknt++;
							stx_error(ERR_ACTLSTTOOLONG, 2, mid_len(&mlbx->mvname)
								,&mlbx->mvname);
							source_error_found = 0;
							y = maketriple(OC_RTERROR);
							y->operand[0] = put_ilit(ERR_ACTLSTTOOLONG);
							ins_triple(y);
							n->oprval.tref = y;
							n->oprclass = TJMP_REF;
						}
						else
						{
							n->oprclass = TJMP_REF;
							n->oprval.tref = y;
						}
					}
					else
					{
						errknt++;
						stx_error(ERR_LABELMISSING, 2, mid_len(&mlbx->mvname)
							,&mlbx->mvname);
						source_error_found = 0;
						y = maketriple(OC_RTERROR);
						y->operand[0] = put_ilit(ERR_LABELUNKNOWN);
						ins_triple(y);
						n->oprval.tref = y;
						n->oprclass = TJMP_REF;
					}
					continue;
				case TRIP_REF:
					if ((y = n->oprval.tref)->opcode == OC_PASSTHRU)
					{
						assert(y->operand[0].oprclass == TRIP_REF);
						*n = y->operand[0];
					}
					p = (tbp *) mcalloc(sizeof(tbp));
					p->bpt = x;
					dqins(&n->oprval.tref->backptr,que,p);
					continue;
				}
			}
			n = &x->destination;
			if (n->oprclass == TRIP_REF)
			{
				if ((y = n->oprval.tref)->opcode == OC_PASSTHRU)
				{
					assert(y->operand[0].oprclass == TRIP_REF);
					*n = y->operand[0];
				}
				p = (tbp *) mcalloc(sizeof(tbp));
				p->bpt = x;
				dqins(&n->oprval.tref->backptr,que,p);
			}
		}
	}
	return errknt;
}
