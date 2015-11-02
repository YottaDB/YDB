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

#include "gtm_string.h"
#include "toktyp.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "indir_enum.h"
#include "cmd_qlf.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "do_indir_do.h"
#include "valid_mname.h"

GBLREF	stack_frame		*frame_pointer;
GBLREF	command_qualifier	cmd_qlf;
GBLREF	boolean_t		is_tracing_on;

int do_indir_do(mval *v, unsigned char argcode)
{
	mval		label;
	lnr_tabent	USHBIN_ONLY(*)*addr;
	mident_fixed	ident;
	rhdtyp		*current_rhead;

	if (valid_labname(&v->str))
	{
		memcpy(ident.c, v->str.addr, v->str.len);
		if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
			lower_to_upper((uchar_ptr_t)ident.c, (uchar_ptr_t)ident.c, v->str.len);
		label.mvtype = MV_STR;
		label.str.len = v->str.len;
		label.str.addr = &ident.c[0];
		addr = op_labaddr(frame_pointer->rvector, &label, 0);
		if (argcode == indir_do)
		{
			if (is_tracing_on)
				exfun_frame_sp();
			else
				exfun_frame();
		}
		current_rhead = CURRENT_RHEAD_ADR(frame_pointer->rvector);
		frame_pointer->mpc = LINE_NUMBER_ADDR(current_rhead, USHBIN_ONLY(*)addr);
#ifdef HAS_LITERAL_SECT
		frame_pointer->ctxt = (unsigned char *)LINKAGE_ADR(current_rhead);
#else
		frame_pointer->ctxt = PTEXT_ADR(current_rhead);
#endif
		return TRUE;
	} else
		return FALSE;
}
