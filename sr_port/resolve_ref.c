/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
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
#include "stringpool.h"

GBLREF boolean_t		run_time;
GBLREF command_qualifier	cmd_qlf;
GBLREF mlabel			*mlabtab;
GBLREF triple			t_orig;
GBLREF uint4			gtmDebugLevel;
GBLREF spdesc			stringpool;
GBLREF src_line_struct          src_head;
GBLREF mident			routine_name;

error_def(ERR_ACTLSTTOOLONG);
error_def(ERR_FMLLSTMISSING);
error_def(ERR_LABELMISSING);
error_def(ERR_LABELNOTFND);
error_def(ERR_LABELUNKNOWN);

STATICFNDCL bool do_optimize(triple *curtrip);

int resolve_ref(int errknt)
{
	int	actcnt;
	mline	*mxl;
	mlabel	*mlbx;
	oprtype *j, *k, *opnd;
	tbp	*tripbp;
	triple	*chktrip, *curtrip, *ref, *tripref, *y;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (errknt && !(cmd_qlf.qlf & CQ_IGNORE))
	{
		assert(!run_time);
		walktree((mvar *)mlabtab, resolve_lab, (char *)&errknt);
	} else
	{
		if (!run_time && (cmd_qlf.qlf & CQ_DYNAMIC_LITERALS))
		{	/* OC_LIT --> OC_LITC wherever OC_LIT is actually used, i.e. not a dead end */
			dqloop(&t_orig, exorder, curtrip)
			{
				if (do_optimize(curtrip))
				{	/* Attempt optimization without checking if's; there are too many conditions to check here,
					   and we will be adding more in the near future
					*/
					/* Backup the pointer so that we rescan after changing the triple */
					curtrip = curtrip->exorder.bl;
					continue;
				}
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
				}
				for (j = curtrip->operand, y = curtrip; j < ARRAYTOP(y->operand); )
				{	/* Iterate over all parameters of the current triple */
					k = j;
					while (INDR_REF == k->oprclass)
						k = k->oprval.indr;
					if (TRIP_REF == k->oprclass)
					{
						tripref = k->oprval.tref;
						if (OC_PARAMETER == tripref->opcode)
						{
							y = tripref;
							j = y->operand;
							continue;
						}
						if (OC_LIT == tripref->opcode)
						{	/* Insert an OC_LITC to relay the OC_LIT result to curtrip */
							ref = maketriple(OC_LITC);
							ref->src = tripref->src;
							ref->operand[0] = put_tref(tripref);
							dqins(curtrip->exorder.bl, exorder, ref);
							*k = put_tref(ref);
						}
					}
					j++;
				}
			}
		}
		COMPDBG(PRINTF("\n\n\n********************* New Compilation -- Begin resolve_ref scan **********************\n"););
		dqloop(&t_orig, exorder, curtrip)
		{	/* If the optimization was not executed earlier */
			if (!run_time && !(cmd_qlf.qlf & CQ_DYNAMIC_LITERALS))
			{
				if (do_optimize(curtrip))
				{	/* Attempt optimization without checking if's; there are too many conditions to check here,
					   and we will be adding more in the near future
					*/
					curtrip = curtrip->exorder.bl;
					continue;
				}
			}
			COMPDBG(PRINTF(" ************************ Triple Start **********************\n"););
			COMPDBG(cdbg_dump_triple(curtrip, 0););
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
					if (mxl)
						tripref = mxl->externalentry;
					else
						tripref = 0;
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
					tripref = mlbx->ml ? mlbx->ml->externalentry : 0;
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
						/* This is a subroutine/func reference */
						tripref->operand[1] = put_ilit(TRUE);
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
					assert(OC_TRIPSIZE == chktrip->operand[0].oprval.tref->opcode);
					assert(TSIZ_REF == chktrip->operand[0].oprval.tref->operand[0].oprclass);
					chktrip = chktrip->operand[1].oprval.tref;
					assert(OC_PARAMETER == chktrip->opcode);
					assert(TRIP_REF == chktrip->operand[0].oprclass);
					assert(OC_ILIT == chktrip->operand[0].oprval.tref->opcode);
					assert(ILIT_REF == chktrip->operand[0].oprval.tref->operand[0].oprclass);
					chktrip = chktrip->operand[1].oprval.tref;
					assert(OC_PARAMETER == chktrip->opcode);
					assert(TRIP_REF == chktrip->operand[0].oprclass);
					assert(OC_ILIT == chktrip->operand[0].oprval.tref->opcode);
					assert(ILIT_REF == chktrip->operand[0].oprval.tref->operand[0].oprclass);
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
							/* This is a subroutine/func reference */
							tripref->operand[1] = put_ilit(TRUE);
							opnd->oprval.tref = tripref;
							opnd->oprclass = TJMP_REF;
						} else if (mlbx->formalcnt < actcnt)
						{
							errknt++;
							stx_error(ERR_ACTLSTTOOLONG, 2, mlbx->mvname.len, mlbx->mvname.addr);
							TREF(source_error_found) = 0;
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
						TREF(source_error_found) = 0;
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

	if (OC_PASSTHRU == (tripref = opnd->oprval.tref)->opcode)		/* note the assignment */
	{
		assert(TRIP_REF == tripref->operand[0].oprclass);
		do
		{	/* As many OC_PASSTHRUs as are stacked, we will devour */
			*opnd = tripref->operand[0];
		} while (OC_PASSTHRU == (tripref = opnd->oprval.tref)->opcode);	/* note the assignment */
	}
	COMPDBG(PRINTF(" ** Passthru replacement: Operand at 0x%08lx replaced by operand at 0x%08lx\n",
		       (long unsigned int) opnd, (long unsigned int)&tripref->operand[0]););
	tripbp = (tbp *)mcalloc(SIZEOF(tbp));
	tripbp->bpt = curtrip;
	dqins(&opnd->oprval.tref->backptr, que, tripbp);
}

STATICFNDCL bool do_optimize(triple *curtrip)
{
	int	i;
	mstr	*source_line;
	mval	tmp_mval;
	tbp	*b;
	triple	*ref, *y, *triple_temp;
	triple	*line_offset, *label, *routine;
	src_line_struct	*cur_line;
	boolean_t negative, optimized = FALSE;
	/* If we are resolving indirect's or something of the sort, die sadly */
	assert(NULL != src_head.que.fl);
	switch (curtrip->opcode)
	{
	case OC_FNTEXT:
		/* If this is a OC_FNTEXT for the current routine, we can
			optimize it by simply inserting the text string from
			src_line_struct.que
		*/
		assert(OC_LITC != curtrip->operand[0].oprval.tref->opcode);
		routine = curtrip->operand[1].oprval.tref->operand[1].oprval.tref;
		line_offset = curtrip->operand[1].oprval.tref->operand[0].oprval.tref;
		label = curtrip->operand[0].oprval.tref;
		/* TODO: there should be a routine to verify literals for a given function */
		if (MLIT_REF != routine->operand[0].oprclass)
			break;
		if (!WANT_CURRENT_RTN(&routine->operand[0].oprval.mlit->v))
			break;
		if (MLIT_REF != label->operand[0].oprclass)
			break;
		if (ILIT_REF != line_offset->operand[0].oprclass)
			break;
		/* If we're here, we have a $TEXT with all literals for the current routine */
		source_line = (mstr *)mcalloc(SIZEOF(mstr));
		/* Special case; label == "" && +0 means file name */
		if (0 == label->operand[0].oprval.mlit->v.str.len
			&& 0 == line_offset->operand[0].oprval.ilit)
		{	/* Get filename, replace thing */
			/* Find last /; this is the start of the filename */
			source_line->len = routine_name.len;
			source_line->addr = malloc(source_line->len);
			memcpy(source_line->addr, routine_name.addr, source_line->len);
		} else
		{	/* Search through strings for label; if label == "" skip */
			cur_line = src_head.que.fl;
			negative = (0 > line_offset->operand[0].oprval.ilit);
			if (0 != label->operand[0].oprval.mlit->v.str.len && cur_line != cur_line->que.fl)
			{
				for (i = 0; cur_line != &src_head; cur_line = cur_line->que.fl)
				{
					if (label->operand[0].oprval.mlit->v.str.len > cur_line->str.len)
						continue;
					if (label->operand[0].oprval.mlit->v.str.len != cur_line->str.len)
					{
						switch (cur_line->str.addr[label->operand[0].oprval.mlit->v.str.len])
						{
							case ' ':
							case ';':
							case '(':
							case ':':
								break;
							default:
								/* If we get here, it means we have a superstring of the label; 
								 * i.e. searching for "a" found "abc" */
								continue;
						}
					}
					if (!strncmp(label->operand[0].oprval.mlit->v.str.addr, cur_line->str.addr,
						label->operand[0].oprval.mlit->v.str.len))
						break;
				}
				if (&src_head == cur_line)
					break;
					/* Error; let the runtime program deal with it for now */
			} else
			{	/* We need a special case to handle +0; if no label, it means start at top of file
					and we begin counting on 1,
					otherwise, it means the line that the label is on
				*/
				i = 1;
			}
			/* We could mod the offset by the size of the file, but hopefully no one is dumb enough to say +100000 */
			/* Counting the number of lines in the file will be O(n), not worth it */
			for (; i < (negative ? -1 : 1) * line_offset->operand[0].oprval.ilit && cur_line != &src_head; i++)
			{
				cur_line = (negative ? cur_line->que.bl : cur_line->que.fl);
			}
			/* If we went through all nodes and i is less than the line we are looking for, use an empty source line */
			if (&src_head == cur_line)
			{	/* Special case; we were counting backward, hit the end of the file, but we are done counting */
				/* This means we should output the name of the routine */
				if (i == (negative ? -1 : 1) * line_offset->operand[0].oprval.ilit
					&& negative)
				{
					source_line->len = routine_name.len;
					source_line->addr = malloc(source_line->len);
					memcpy(source_line->addr, routine_name.addr, source_line->len);
				} else
				{
					source_line->len = 0;
					source_line->addr = 0;
				}
			} else
			{
				source_line->len = cur_line->str.len;
				source_line->addr = malloc(source_line->len);
				memcpy(source_line->addr, cur_line->str.addr, cur_line->str.len);
			}
		}
		/* Insert literal into triple tree */
		tmp_mval.mvtype = MV_STR;
		/* Minus one so we don't copy newline character */
		tmp_mval.str.len = (source_line->len == 0 ? 0 :
			source_line->len - (source_line->addr[source_line->len-1] == '\n' ? 1 : 0));
		ENSURE_STP_FREE_SPACE(tmp_mval.str.len);
		tmp_mval.str.addr = (char *)stringpool.free;
		memcpy(tmp_mval.str.addr, source_line->addr, tmp_mval.str.len);
		/* Replace tab characters with spaces */
		for (i = 0; i < tmp_mval.str.len && tmp_mval.str.addr[i] != ';'; i++)
		{
			if ('\t' == tmp_mval.str.addr[i])
				tmp_mval.str.addr[i] = ' ';
		}
		stringpool.free += tmp_mval.str.len;
		if (source_line->addr != 0)
			free(source_line->addr);
		/* Update all things that referenced this value */
		curtrip->opcode = OC_LIT;
		put_lit_s(&tmp_mval, curtrip);
		label->opcode = OC_NOOP;
		line_offset = OC_NOOP;
		routine->opcode = OC_NOOP;
		optimized = TRUE;
		break;
		/* If no cases no optimizations to perform.... yet */
	}
	return optimized;
}
