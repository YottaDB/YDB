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
#include "indir_enum.h"
#include "nametabtyp.h"
#include "toktyp.h"
#include "funsvn.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "namelook.h"
#include "cmd.h"


GBLDEF bool		temp_subs;
GBLREF triple		*curtchain;
GBLREF char		director_token, window_token;
GBLREF mident		window_ident;
LITREF unsigned char	svn_index[], fun_index[];
LITREF nametabent	svn_names[], fun_names[];
LITREF svn_data_type	svn_data[];
LITREF fun_data_type	fun_data[];

int m_set(void)
{
	error_def(ERR_INVSVN);
	error_def(ERR_VAREXPECTED);
	error_def(ERR_RPARENMISSING);
	error_def(ERR_EQUAL);
	error_def(ERR_COMMA);
	error_def(ERR_SVNOSET);
	int		index;
	int		first_val, last_val;
	boolean_t	first_is_lit, last_is_lit, got_lparen, delim1char, is_extract;
	opctype		put_oc;
	oprtype		v, delimval, *sb1, *result, resptr;
	triple		*delimiter, *first, *put, *get, *last, *obp, *s, *sub, *s0, *s1, tmpchain;
	triple		*jmptrp1, *jmptrp2;

	temp_subs = delim1char = is_extract = FALSE;
	dqinit(&tmpchain, exorder);
	result = (oprtype *) mcalloc(sizeof(oprtype));
	resptr = put_indr(result);
	jmptrp1 = jmptrp2 = 0;
	sub = (triple *)0;

	if (got_lparen = (window_token == TK_LPAREN))
	{
		advancewindow();
		temp_subs = TRUE;
	}
	for (;;)
	{
		switch (window_token)
		{
		case TK_IDENT:
			if (!lvn(&v, OC_PUTINDX, 0))
				return FALSE;
			if (v.oprval.tref->opcode == OC_PUTINDX)
			{
				dqdel(v.oprval.tref,exorder);
				dqins(tmpchain.exorder.bl,exorder,v.oprval.tref);
				sub = v.oprval.tref;
				put_oc = OC_PUTINDX;
			}
			put = maketriple(OC_STO);
			put->operand[0] = v;
			put->operand[1] = resptr;
			dqins(tmpchain.exorder.bl,exorder,put);
			break;
		case TK_CIRCUMFLEX:
			s1 = curtchain->exorder.bl;
			if (!gvn())
				return FALSE;
			for (sub = curtchain->exorder.bl; sub != s1 ; sub = sub->exorder.bl)
			{
				put_oc = sub->opcode;
				if (put_oc == OC_GVNAME || put_oc == OC_GVNAKED || put_oc == OC_GVEXTNAM)
					break;
			}
			assert(put_oc == OC_GVNAME || put_oc == OC_GVNAKED || put_oc == OC_GVEXTNAM);
			dqdel(sub,exorder);
			dqins(tmpchain.exorder.bl,exorder,sub);
			put = maketriple(OC_GVPUT);
			put->operand[0] = resptr;
			dqins(tmpchain.exorder.bl,exorder,put);
			break;
		case TK_ATSIGN:
			if (!indirection(&v))
				return FALSE;
			if (!got_lparen && window_token != TK_EQUAL)
			{
				put = newtriple(OC_COMMARG);
				put->operand[0] = v;
				put->operand[1] = put_ilit(indir_set);
				ins_triple(put);
				return TRUE;
			}
			put = maketriple(OC_INDSET);
			put->operand[0] = v;
			put->operand[1] = resptr;
			dqins(tmpchain.exorder.bl,exorder,put);
			break;
		case TK_DOLLAR:
			advancewindow();
			if (window_token != TK_IDENT)
			{
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			if (director_token != TK_LPAREN)
			{	/* Look for extrinsic vars */
				if ((index = namelook(svn_index, svn_names, window_ident.c)) < 0)
				{
					stx_error(ERR_INVSVN);
					return FALSE;
				}
				advancewindow();
				if (!svn_data[index].can_set)
				{
					stx_error(ERR_SVNOSET);
					return FALSE;
				}
				put = maketriple(OC_SVPUT);
				put->operand[0] = put_ilit(svn_data[index].opcode);
				put->operand[1] = resptr;
				dqins(tmpchain.exorder.bl,exorder,put);
				break;
			}
			/* Only 2 function names allowed on left side: $Piece and $Extract */
			index = namelook(fun_index, fun_names, window_ident.c);
			is_extract = (OC_FNEXTRACT == fun_data[index].opcode);
			if (index < 0 || (!is_extract && fun_data[index].opcode != OC_FNPIECE) || got_lparen)
			{
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			advancewindow();
			advancewindow();
			if (!is_extract)
			{
				s = maketriple(OC_SETPIECE);
				delimiter = newtriple(OC_PARAMETER);
				s->operand[1] = put_tref(delimiter);
				first = newtriple(OC_PARAMETER);
				delimiter->operand[1] = put_tref(first);
			} else
			{
				s = maketriple(OC_SETEXTRACT);
				first = newtriple(OC_PARAMETER);
				s->operand[1] = put_tref(first);
			}
			switch (window_token)
			{
			case TK_IDENT:
				if (!lvn(&v, OC_PUTINDX, 0))
				{
					stx_error(ERR_VAREXPECTED);
					return FALSE;
				}
				if (v.oprval.tref->opcode == OC_PUTINDX)
				{
					dqdel(v.oprval.tref,exorder);
					dqins(tmpchain.exorder.bl,exorder,v.oprval.tref);
					sub = v.oprval.tref;
					put_oc = OC_PUTINDX;
				}
				get = maketriple(OC_FNGET);
				get->operand[0] = v;
				put = maketriple(OC_STO);
				put->operand[0] = v;
				put->operand[1] = put_tref(s);
				break;
			case TK_ATSIGN:
				if (!indirection(&v))
				{
					stx_error(ERR_VAREXPECTED);
					return FALSE;
				}
				get = maketriple(OC_INDGET);
				get->operand[0] = v;
				get->operand[1] = put_str(0,0);
				put = maketriple(OC_INDSET);
				put->operand[0] = v;
				put->operand[1] = put_tref(s);
				break;
			case TK_CIRCUMFLEX:
				s1 = curtchain->exorder.bl;
				if (!gvn())
					return FALSE;
				for (sub = curtchain->exorder.bl; sub != s1 ; sub = sub->exorder.bl)
				{
					put_oc = sub->opcode;
					if (put_oc == OC_GVNAME || put_oc == OC_GVNAKED || put_oc == OC_GVEXTNAM)
						break;
				}
				assert(put_oc == OC_GVNAME || put_oc == OC_GVNAKED || put_oc == OC_GVEXTNAM);
				dqdel(sub,exorder);
				dqins(tmpchain.exorder.bl,exorder,sub);
				get = maketriple(OC_FNGVGET);
				get->operand[0] = put_str(0,0);
				put = maketriple(OC_GVPUT);
				put->operand[0] = put_tref(s);
				break;
			default:
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			s->operand[0] = put_tref(get);
			dqins(tmpchain.exorder.bl,exorder,get);

			if (!is_extract)
			{	/* Process delimiter string ($piece only) */
				if (window_token != TK_COMMA)
				{
					stx_error(ERR_COMMA);
					return FALSE;
				}
				advancewindow();
				if (!strexpr(&delimval))
					return FALSE;
				assert(delimval.oprclass == TRIP_REF);
			}

			/* Process first integer value */
			if (window_token != TK_COMMA)
				first->operand[0] = put_ilit(1);
			else
			{
				advancewindow();
				if (!intexpr(&(first->operand[0])))
					return FALSE;
			}
			assert(first->operand[0].oprclass == TRIP_REF);
			if (first_is_lit = (first->operand[0].oprval.tref->opcode == OC_ILIT))
			{
				assert(first->operand[0].oprval.tref->operand[0].oprclass  == ILIT_REF);
				first_val = first->operand[0].oprval.tref->operand[0].oprval.ilit;
			}
			if (window_token != TK_COMMA)
			{
				/* Only if 1 char literal delimiter and no "last" value can we generate
				   shortcut code to op_setp1 entry instead of op_setpiece */
				if (!is_extract && window_token != TK_COMMA && delimval.oprval.tref->opcode == OC_LIT &&
				    delimval.oprval.tref->operand[0].oprval.mlit->v.str.len == 1)
				{
					s->opcode = OC_SETP1;
					delimiter->operand[0] =
						put_ilit((uint4) *delimval.oprval.tref->operand[0].oprval.mlit->v.str.addr);
					delim1char = TRUE;
				} else
				{
					if (!is_extract)
						delimiter->operand[0] = delimval;
					last = newtriple(OC_PARAMETER);
					first->operand[1] = put_tref(last);
					last->operand[0] = first->operand[0];
				}
				if (first_is_lit)
				{
					if (first_val < 1)
						jmptrp1 = newtriple(OC_JMP);
				} else
				{
					jmptrp1 = newtriple(OC_COBOOL);
					if (delim1char)
						jmptrp1->operand[0] = first->operand[0];
					else
						jmptrp1->operand[0] = last->operand[0];
					jmptrp1 = newtriple(OC_JMPLEQ);
				}
			} else
			{
				if (!is_extract)
					delimiter->operand[0] = delimval;
				last = newtriple(OC_PARAMETER);
				first->operand[1] = put_tref(last);
				advancewindow();
				if (!intexpr(&(last->operand[0])))
					return FALSE;

				/* NOTE: This is a "by-hand" compile time constant evaluation */
				assert(last->operand[0].oprclass == TRIP_REF);
				if (last_is_lit = (last->operand[0].oprval.tref->opcode == OC_ILIT))
				{
					assert(last->operand[0].oprval.tref->operand[0].oprclass  == ILIT_REF);
					last_val = last->operand[0].oprval.tref->operand[0].oprval.ilit;
					if (last_val < 1 || (first_is_lit && first_val > last_val))
						jmptrp1 = newtriple(OC_JMP);
				} else
				{
					jmptrp1 = newtriple(OC_COBOOL);
					jmptrp1->operand[0] = last->operand[0];
					jmptrp1 = newtriple(OC_JMPLEQ);
				}
				if (!last_is_lit || !first_is_lit)
				{
					jmptrp2 = newtriple(OC_VXCMPL);
					jmptrp2->operand[0] = first->operand[0];
					jmptrp2->operand[1] = last->operand[0];
					jmptrp2 = newtriple(OC_JMPGTR);
				}
			}
			if (window_token != TK_RPAREN)
			{
				stx_error(ERR_RPARENMISSING);
				return FALSE;
			}
			advancewindow();
			dqins(tmpchain.exorder.bl,exorder,s);
			dqins(tmpchain.exorder.bl,exorder,put);
			/* Put result operand on the chain. End of chain depends on whether or not
			   we are calling the shortcut or the full set-piece code */
			if (delim1char)
				first->operand[1] = resptr;
			else
				last->operand[1] = resptr;
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		if (!got_lparen)
			break;
		if (window_token == TK_COMMA)
			advancewindow();
		else
		{
			if (window_token == TK_RPAREN)
			{
				advancewindow();
				break;
			} else
			{
				stx_error(ERR_RPARENMISSING);
				return FALSE;
			}
		}
	}
	if (window_token != TK_EQUAL)
	{
		stx_error(ERR_EQUAL);
		return FALSE;
	}
	advancewindow();
	if (!expr(result))
		return FALSE;
	obp = curtchain->exorder.bl;
	dqadd(obp, &tmpchain, exorder);   /*this is a violation of info hiding*/

	if (sub)
	{
		sb1 = &sub->operand[1];
		if (put_oc == OC_GVNAME || put_oc == OC_PUTINDX)
		{
			sub = sb1->oprval.tref;		/* global name */
			assert(sub->opcode == OC_PARAMETER);
			sb1 = &sub->operand[1];
		}
		else if (put_oc == OC_GVEXTNAM)
		{
			sub = sb1->oprval.tref;		/* first env */
			assert(sub->opcode == OC_PARAMETER);
			sb1 = &sub->operand[0];
			assert(sb1->oprclass == TRIP_REF);
			s0 = sb1->oprval.tref;
			if ((temp_subs && s0->opcode == OC_GETINDX) || s0->opcode == OC_VAR)
			{
				s1 = maketriple(OC_STOTEMP);
				s1->operand[0] = *sb1;
				*sb1 = put_tref(s1);
				dqins(s0->exorder.bl, exorder, s1);
			}
			sb1 = &sub->operand[1];
			sub = sb1->oprval.tref;		/* second env */
			assert(sub->opcode == OC_PARAMETER);
			sb1 = &sub->operand[0];
			assert(sb1->oprclass == TRIP_REF);
			s0 = sb1->oprval.tref;
			if ((temp_subs && s0->opcode == OC_GETINDX) || s0->opcode == OC_VAR)
			{
				s1 = maketriple(OC_STOTEMP);
				s1->operand[0] = *sb1;
				*sb1 = put_tref(s1);
				dqins(s0->exorder.bl, exorder, s1);
			}
			sb1 = &sub->operand[1];
			sub = sb1->oprval.tref;		/* global name */
			assert(sub->opcode == OC_PARAMETER);
			sb1 = &sub->operand[1];
		}
		while(sb1->oprclass)
		{
			assert(sb1->oprclass == TRIP_REF);
			sub = sb1->oprval.tref;
			assert(sub->opcode == OC_PARAMETER);
			sb1 = &sub->operand[0];
			assert(sb1->oprclass == TRIP_REF);
			s0 = sb1->oprval.tref;
			if (temp_subs && (s0->opcode == OC_GETINDX || s0->opcode == OC_VAR))
			{
				s1 = maketriple(OC_STOTEMP);
				s1->operand[0] = *sb1;
				*sb1 = put_tref(s1);
				s0 = s0->exorder.fl;
				dqins(s0->exorder.bl, exorder, s1);
			}
			else if (s0->opcode == OC_VAR)
			{
				s1 = maketriple(OC_NAMECHK);
				s1->operand[0] = *sb1;
				s0 = s0->exorder.fl;
				dqins(s0->exorder.bl, exorder, s1);
			}
			sb1 = &sub->operand[1];
		}
	}
	if (jmptrp1)
		tnxtarg(&jmptrp1->operand[0]);
	if (jmptrp2)
		tnxtarg(&jmptrp2->operand[0]);
	return TRUE;
}
