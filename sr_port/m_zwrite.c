/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "toktyp.h"
#include "mdq.h"
#include "advancewindow.h"
#include "cmd.h"
#include "compile_pattern.h"
#include "mvalconv.h"

GBLREF char 		window_token;
GBLREF mval 		window_mval;
GBLREF mident 		window_ident;
GBLREF char 		director_token;
GBLREF short int 	source_column;
GBLREF short int 	last_source_column;
GBLREF uint4 		pat_everything[];
GBLREF uint4 		sizeof_pat_everything;

/******	CAUTION !!! ******
 *	All occurrences of put_lit should be replaced by put_ilit.  In order to maintain object
 *	code compatibility, however, this replacement has been preempted by preceding put_lit
 *	with n2s.
 *	-no runtime module looks at anything but nm, so call to n2s is dispensed with. -mwm
 */

int m_zwrite(void)
{
	int4		pcount;			/* parameter count */
	triple *ref,*ref1,*head,*last,*count;
	opctype op;
	oprtype name,limit;
	mval	mv;
	mint code;
	mint subscount;
	char c;
	bool pat;
	error_def(ERR_VAREXPECTED);
	error_def(ERR_RPARENMISSING);
	error_def(ERR_ZWRSPONE);
	error_def(ERR_COMMA);

	subscount = 0;
	count = 0;
	pat = FALSE;
	if (window_token == TK_CIRCUMFLEX)
	{
		advancewindow();
		op = OC_GVZWRITE;
	}
	else
	{	op = OC_LVZWRITE;
	}
	switch(window_token)
	{
	case TK_SPACE:
	case TK_EOL:
		if (op == OC_GVZWRITE)
		{
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		else
		{	 op = OC_LVPATWRITE;
		}
		head = maketriple(op);
		head->operand[0] = put_ilit((mint)3);
		ref1 = newtriple(OC_PARAMETER);
		head->operand[1] = put_tref(ref1);
		ref1->operand[0] = put_ilit(0);			/* shows not from zshow */
		ref = newtriple(OC_PARAMETER);
		ref1->operand[1] = put_tref(ref);
		ref->operand[0] = put_str((char *)pat_everything,sizeof_pat_everything);
		MV_FORCE_MVAL(&mv,ZWRITE_ASTERISK) ;
		ref->operand[1] = put_lit(&mv);
		ins_triple(head);
		return TRUE;
	case TK_IDENT:
		name = put_str(window_ident.addr, window_ident.len);
		advancewindow();
		break;
	case TK_LPAREN:
		if (op != OC_GVZWRITE) /* naked reference */
		{
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		name = put_str(window_ident.addr, 0);
		break;
	case TK_ATSIGN:
		if (!indirection(&name))
			return FALSE;
		if (op == OC_LVZWRITE && window_token != TK_LPAREN)
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
		source_column = last_source_column;
		if (!compile_pattern(&name,FALSE))
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
	if (op == OC_LVPATWRITE || op == OC_GVZWRITE)
	{
		pcount++;
		last->operand[0] = put_ilit((op == OC_GVZWRITE ? pat : 0));
		ref = newtriple(OC_PARAMETER);
		last->operand[1] = put_tref(ref);
		last = ref;
		if (op == OC_GVZWRITE)
		{
			pcount++;
			count = last;
			ref = newtriple(OC_PARAMETER);
			last->operand[1] = put_tref(ref);
			last = ref;
		}
	}
	last->operand[0] = name;
	if (window_token != TK_LPAREN)
	{
		pcount++;
		if (pat)
		{
			MV_FORCE_MVAL(&mv,ZWRITE_END) ;
		}
		else
		{	subscount++ ;
			MV_FORCE_MVAL(&mv,ZWRITE_ASTERISK) ;
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
		switch (window_token)
		{
			case TK_RPAREN:
				dqdel(ref,exorder);
				advancewindow();
				MV_FORCE_MVAL(&mv,ZWRITE_END) ;
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
				if (window_token != TK_RPAREN)
				{
					stx_error(ERR_RPARENMISSING);
					return FALSE;
				}
				advancewindow();
				MV_FORCE_MVAL(&mv,ZWRITE_ASTERISK) ;
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
				source_column = last_source_column;
				if (!compile_pattern(&limit,FALSE))
					return FALSE;
				if (window_token != TK_COMMA && window_token != TK_RPAREN)
				{	stx_error(ERR_ZWRSPONE);
					return FALSE;
				}
				if (window_token == TK_COMMA)
					advancewindow();
				subscount++;
				MV_FORCE_MVAL(&mv,ZWRITE_PATTERN) ;
				ref->operand[0] = put_lit(&mv);
				pcount++;
				ref1 = newtriple(OC_PARAMETER);
				ref->operand[1] = put_tref(ref1);
				ref1->operand[0] = limit;
				last = ref1;
				pcount++;
				continue;
			case TK_COLON:
				if ((c = director_token) != TK_RPAREN)
				{
					if (c != TK_COMMA)
					{
						advancewindow();
						MV_FORCE_MVAL(&mv,ZWRITE_UPPER) ;
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
				MV_FORCE_MVAL(&mv,ZWRITE_ALL) ;
				ref->operand[0] = put_lit(&mv);
				pcount++;
				subscount++;
				last = ref;
				continue;
			default:
				if (!expr(&limit))
					return FALSE;
				subscount++;
				last = newtriple(OC_PARAMETER);
				ref->operand[1] = put_tref(last);
				last->operand[0] = limit;
				pcount++;
				if ((c = window_token) == TK_COLON)
				{
					code = ZWRITE_LOWER;
					advancewindow();
					c = window_token;
				}
				else
					code = ZWRITE_VAL;
				switch (c)
				{
				case TK_COMMA:
					advancewindow();
					/* caution: fall through */
				case TK_RPAREN:
					MV_FORCE_MVAL(&mv,code) ;
					ref->operand[0] = put_lit(&mv);
					pcount++;
					continue;
				default:
					if (code == ZWRITE_VAL)
					{
						stx_error(ERR_COMMA);
						return FALSE;
					}
					MV_FORCE_MVAL(&mv,ZWRITE_BOTH) ;
					ref->operand[0] = put_lit(&mv);
					pcount++;
					ref = last;
					break;
				}
				break;
		}
		if (!expr(&limit))
			return FALSE;
		last = newtriple(OC_PARAMETER);
		ref->operand[1] = put_tref(last);
		last->operand[0] = limit;
		pcount++;
		if (window_token == TK_COMMA)
		{
			advancewindow();
		}
	}
}
