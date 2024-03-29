/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "fullbool.h"

GBLREF spdesc		stringpool;
GBLREF int		source_column;

int compile_pattern(oprtype *opr, boolean_t is_indirect)
{
	int		status;
	ptstr		retstr;
	mval		retmval;
	mstr		instr;
	triple		*oldchain, *ref;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (is_indirect)
	{
		if (SHIFT_SIDE_EFFECTS)
		{
			START_GVBIND_CHAIN(&save_state, oldchain);
			if (!indirection(opr))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			ref = newtriple(OC_INDPAT);
			PLACE_GVBIND_CHAIN(&save_state, oldchain);
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
		instr.addr = ((TREF(source_buffer)).addr + source_column - 1);
		instr.len = STRLEN(instr.addr);
		status = patstr(&instr, &retstr, NULL);
		TREF(last_source_column) = (short int)(instr.addr - (TREF(source_buffer)).addr);
		assert(TREF(last_source_column));
		if (status)
		{	/* status == syntax error when non-zero */
			stx_error(status);
			return FALSE;
		}
		memset(&retmval, 0, SIZEOF(mval));
		retmval.mvtype = MV_STR;
		retmval.str.len = retstr.len * SIZEOF(uint4);
		ENSURE_STP_FREE_SPACE(retmval.str.len);
		retmval.str.addr = (char *)stringpool.free;
		memcpy(stringpool.free, &retstr.buff[0], retmval.str.len);
		stringpool.free += retmval.str.len;
		*opr = put_lit(&retmval);
		TREF(lexical_ptr) = instr.addr;
		advancewindow();
		advancewindow();
		return TRUE;
	}
}
