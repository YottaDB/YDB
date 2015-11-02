/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "nametabtyp.h"
#include "indir_enum.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "toktyp.h"
#include "svnames.h"
#include "funsvn.h"
#include "advancewindow.h"
#include "cmd.h"
#include "compile_pattern.h"
#include "mvalconv.h"
#include "namelook.h"

GBLREF short int 	source_column;
GBLREF uint4 		pat_everything[];
GBLREF mstr_len_t	sizeof_pat_everything;

error_def(ERR_COMMA);
error_def(ERR_INVSVN);
error_def(ERR_RPARENMISSING);
error_def(ERR_SVNEXPECTED);
error_def(ERR_VAREXPECTED);
error_def(ERR_ZWRSPONE);

LITREF unsigned char    svn_index[];
LITREF nametabent       svn_names[];
LITREF svn_data_type    svn_data[];

/******	CAUTION !!! ******
 *	All occurrences of put_lit should be replaced by put_ilit.  In order to maintain object
 *	code compatibility, however, this replacement has been preempted by preceding put_lit
 *	with n2s.
 *	-no runtime module looks at anything but nm, so call to n2s is dispensed with. -mwm
 */

int m_zwrite(void)
{
	boolean_t	parse_warn, pat;
	char		c;
	int		index;
	int4		pcount;			/* parameter count */
	triple		*count, *head, *last, *ref, *ref1;
	mint		code, subscount;
	mval		mv;
	opctype 	op;
	oprtype 	limit, name;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	subscount = 0;
	count = 0;
	pat = FALSE;
	if (TK_CIRCUMFLEX == TREF(window_token))
	{
		advancewindow();
		op = OC_GVZWRITE;
	} else
		op = OC_LVZWRITE;
	switch (TREF(window_token))
	{
	case TK_SPACE:
	case TK_EOL:
		if (OC_GVZWRITE == op)
		{
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		op = OC_LVPATWRITE;
		head = maketriple(op);
		head->operand[0] = put_ilit((mint)3);		/* count */
		ref1 = newtriple(OC_PARAMETER);
		head->operand[1] = put_tref(ref1);
		ref1->operand[0] = put_ilit(0);			/* shows not from zshow */
		ref = newtriple(OC_PARAMETER);
		ref1->operand[1] = put_tref(ref);
		ref->operand[0] = put_str((char *)pat_everything, sizeof_pat_everything);
		MV_FORCE_MVAL(&mv, ZWRITE_ASTERISK) ;
		ref->operand[1] = put_lit(&mv);
		ins_triple(head);
		return TRUE;
	case TK_IDENT:
		name = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
		advancewindow();
		break;
	case TK_DOLLAR:
		advancewindow();
		if ((TK_IDENT != TREF(window_token)) || (OC_GVZWRITE == op))
		{
			stx_error(ERR_SVNEXPECTED);
			return FALSE;
		}
		parse_warn = FALSE;
		index = namelook(svn_index, svn_names, (TREF(window_ident)).addr, (TREF(window_ident)).len);
		if (0 > index)
		{
			STX_ERROR_WARN(ERR_INVSVN);     /* sets "parse_warn" to TRUE */
		} else
		{
			if (!VALID_SVN(index))
			{
				STX_ERROR_WARN(ERR_FNOTONSYS);  /* sets "parse_warn" to TRUE */
			}
		}
		advancewindow();
		switch (TREF(window_token))
		{
		case TK_SPACE:
		case TK_EOL:
		case TK_COMMA:
			if (!parse_warn)
			{
				assert(SV_NUM_SV > svn_data[index].opcode);
				ref = maketriple(OC_ZWRITESVN);
				ref->operand[0] = put_ilit(svn_data[index].opcode);
				ins_triple(ref);
			} else
			{       /* OC_RTERROR triple would have been inserted in curtchain by ins_errtriple
				 * (invoked by stx_error). No need to do anything else.
				 */
				assert(OC_RTERROR == (TREF(curtchain))->exorder.bl->exorder.bl->exorder.bl->opcode);
			}
			return TRUE;
		default:
			stx_error(ERR_SVNEXPECTED);
			return FALSE;
		}
		break;
	case TK_LPAREN:
		if (OC_GVZWRITE != op) /* naked reference */
		{
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		name = put_str((TREF(window_ident)).addr, 0);
		break;
	case TK_ATSIGN:
		if (!indirection(&name))
			return FALSE;
		if ((OC_LVZWRITE == op) && (TK_LPAREN != TREF(window_token)))
		{
			ref = maketriple(OC_COMMARG);
			ref->operand[0] = name;
			ref->operand[1] = put_ilit(indir_zwrite);
			ins_triple(ref);
			return TRUE;
		}
		ref = newtriple(OC_INDPAT);
		ref->operand[0] = name;
		name = put_tref(ref);
		break;
	case TK_QUESTION:
		advancewindow();
		source_column = TREF(last_source_column);
		if (!compile_pattern(&name, FALSE))
			return FALSE;
		if (op == OC_LVZWRITE)
			op = OC_LVPATWRITE;
		pat = TRUE;
		break;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	head = maketriple(op);
	last = newtriple(OC_PARAMETER);
	head->operand[1] = put_tref(last);
	pcount = 1;
	if ((OC_LVPATWRITE == op) || (OC_GVZWRITE == op))
	{
		pcount++;
		last->operand[0] = put_ilit(((OC_GVZWRITE == op)? pat : 0));
		ref = newtriple(OC_PARAMETER);
		last->operand[1] = put_tref(ref);
		last = ref;
		if (OC_GVZWRITE == op)
		{
			pcount++;
			count = last;
			ref = newtriple(OC_PARAMETER);
			last->operand[1] = put_tref(ref);
			last = ref;
		}
	}
	last->operand[0] = name;
	if (TK_LPAREN != TREF(window_token))
	{
		pcount++;
		if (pat)
		{
			MV_FORCE_MVAL(&mv, ZWRITE_END);
		} else
		{
			subscount++ ;
			MV_FORCE_MVAL(&mv, ZWRITE_ASTERISK);
		}
		last->operand[1] = put_lit(&mv);
		head->operand[0] = put_ilit(pcount);
		if (count)
			count->operand[0] = put_ilit(subscount);
		ins_triple(head);
		return TRUE;
	}
	advancewindow();
	for(;;)
	{
		ref = newtriple(OC_PARAMETER);
		last->operand[1] = put_tref(ref);
		switch (TREF(window_token))
		{
		case TK_RPAREN:
			dqdel(ref,exorder);
			advancewindow();
			MV_FORCE_MVAL(&mv, ZWRITE_END);
			last->operand[1] = put_lit(&mv);
			pcount++;
			head->operand[0] = put_ilit((mint)pcount);
			if (count)
				count->operand[0] = put_ilit(subscount);
			ins_triple(head);
			return TRUE;
		case TK_ASTERISK:
			dqdel(ref,exorder);
			advancewindow();
			if (TK_RPAREN != TREF(window_token))
			{
				stx_error(ERR_RPARENMISSING);
				return FALSE;
			}
			advancewindow();
			MV_FORCE_MVAL(&mv, ZWRITE_ASTERISK);
			last->operand[1] = put_lit(&mv);
			pcount++;
			subscount++;
			head->operand[0] = put_ilit((mint)pcount);
			if (count)
				count->operand[0] = put_ilit(subscount);
			ins_triple(head);
			return TRUE;
		case TK_QUESTION:
			advancewindow();
			source_column = TREF(last_source_column);
			if (!compile_pattern(&limit, FALSE))
				return FALSE;
			if ((TK_COMMA != TREF(window_token)) && (TK_RPAREN != TREF(window_token)))
			{
				stx_error(ERR_ZWRSPONE);
				return FALSE;
			}
			if (TK_COMMA == TREF(window_token))
				advancewindow();
			subscount++;
			MV_FORCE_MVAL(&mv, ZWRITE_PATTERN);
			ref->operand[0] = put_lit(&mv);
			pcount++;
			ref1 = newtriple(OC_PARAMETER);
			ref->operand[1] = put_tref(ref1);
			ref1->operand[0] = limit;
			last = ref1;
			pcount++;
			continue;
		case TK_COLON:
			if (TK_RPAREN != (c = TREF(director_token)))	/* NOTE assignment */
			{
				if (TK_COMMA != c)
				{
					advancewindow();
					MV_FORCE_MVAL(&mv, ZWRITE_UPPER);
					ref->operand[0] = put_lit(&mv);
					pcount++;
					subscount++;
					break;
				}
				advancewindow();
			}
			/* caution: fall through */
		case TK_COMMA:
			advancewindow();
			MV_FORCE_MVAL(&mv, ZWRITE_ALL);
			ref->operand[0] = put_lit(&mv);
			pcount++;
			subscount++;
			last = ref;
			continue;
		default:
			if (EXPR_FAIL == expr(&limit, MUMPS_EXPR))
				return FALSE;
			subscount++;
			last = newtriple(OC_PARAMETER);
			ref->operand[1] = put_tref(last);
			last->operand[0] = limit;
			pcount++;
			if (TK_COLON == (c = TREF(window_token)))	/* NOTE assignment */
			{
				code = ZWRITE_LOWER;
				advancewindow();
				c = TREF(window_token);
			} else
				code = ZWRITE_VAL;
			switch (c)
			{
			case TK_COMMA:
				advancewindow();
				/* caution: fall through */
			case TK_RPAREN:
				MV_FORCE_MVAL(&mv, code) ;
				ref->operand[0] = put_lit(&mv);
				pcount++;
				continue;
			default:
				if (code == ZWRITE_VAL)
				{
					stx_error(ERR_COMMA);
					return FALSE;
				}
				MV_FORCE_MVAL(&mv, ZWRITE_BOTH) ;
				ref->operand[0] = put_lit(&mv);
				pcount++;
				ref = last;
				break;
			}
			break;
		}
		if (EXPR_FAIL == expr(&limit, MUMPS_EXPR))
			return FALSE;
		last = newtriple(OC_PARAMETER);
		ref->operand[1] = put_tref(last);
		last->operand[0] = limit;
		pcount++;
		if (TK_COMMA == TREF(window_token))
			advancewindow();
	}
}
