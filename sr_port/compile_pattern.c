/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "compiler.h"
#include "stringpool.h"
#include "opcode.h"
#include "mdq.h"
#include "advancewindow.h"
#include "compile_pattern.h"
#include "patcode.h"

GBLREF spdesc		stringpool;
GBLREF char		*lexical_ptr;
GBLREF unsigned char	*source_buffer;
GBLREF short int	source_column, last_source_column;
GBLREF bool		shift_gvrefs;
GBLREF triple		*expr_start;

int compile_pattern(oprtype *opr, bool is_indirect)
{
	ptstr		retstr;
	mval		retmval;
	mstr		instr;
	int		status;
	triple		*oldchain, tmpchain, *ref, *triptr;

	if (is_indirect)
	{
		if (shift_gvrefs)
		{
			dqinit(&tmpchain, exorder);
			oldchain = setcurtchain(&tmpchain);
			if (!indirection(opr))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			ref = newtriple(OC_INDPAT);
			newtriple(OC_GVSAVTARG);
			setcurtchain(oldchain);
			dqadd(expr_start, &tmpchain, exorder);
			expr_start = tmpchain.exorder.bl;
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(expr_start);
		} else
		{
			if (!indirection(opr))
				return FALSE;
			ref = newtriple(OC_INDPAT);
		}
		ref->operand[0] = *opr;
		*opr = put_tref(ref);
		return TRUE;
	} else
	{
		instr.addr = (char *)&source_buffer[source_column - 1];
		instr.len = strlen(instr.addr);
		status = patstr(&instr, &retstr, NULL);
		last_source_column = (short int)(instr.addr - (char *)source_buffer);
		assert(last_source_column);
		if (status)
		{	/* status == syntax error when non-zero */
			stx_error(status);
			return FALSE;
		}
		retmval.mvtype = MV_STR;
		retmval.str.len = retstr.len * sizeof(uint4);
		retmval.str.addr = stringpool.free;
		if (stringpool.top - stringpool.free < retmval.str.len)
			stp_gcol(retmval.str.len);
		memcpy(stringpool.free, &retstr.buff[0], retmval.str.len);
		stringpool.free += retmval.str.len;
		*opr = put_lit(&retmval);
		lexical_ptr = (char *)&source_buffer[last_source_column - 1];
		advancewindow();
		advancewindow();
		return TRUE;
	}
}
