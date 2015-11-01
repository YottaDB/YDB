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

#include "rtnhdr.h"
#include "cmd_qlf.h"
#include "gtm_caseconv.h"

GBLREF command_qualifier	cmd_qlf;

int4 *find_line_addr (rhdtyp *routine, mstr *label, short int offset)
{
 	LAB_TABENT	*base, *top, *ptr;
	rhdtyp		*real_routine;
	mident		target_label;
	LNR_TABENT	*line_table, *first_line;
	int		stat, n;
	error_def(ERR_LABELONLY);

	if (!routine)
		return 0;

	real_routine = CURRENT_RHEAD_ADR(routine);
	first_line = LNRTAB_ADR(real_routine);

	if (!label->len  ||  !*label->addr)
		line_table = first_line;
	else
	{
		memset(&target_label.c[0], 0, sizeof(mident));
		if (cmd_qlf.qlf & CQ_LOWER_LABELS)
			memcpy(&target_label.c[0], label->addr,
				(label->len <= sizeof(mident)) ? label->len : sizeof(mident));
		else
			lower_to_upper((uchar_ptr_t)&target_label.c[0], (uchar_ptr_t)label->addr,
				(label->len <= sizeof(mident)) ? label->len : sizeof(mident));

		ptr = base = LABTAB_ADR(real_routine);
		top = base + real_routine->labtab_len;
		for (  ;  ;  )
		{
			n = (top - base) / 2;
			ptr = base + n;
			stat = memcmp(&target_label.c[0], &ptr->lab_name.c[0], sizeof(mident));
			if (!stat)
			{
				line_table = LABENT_LNR_ENTRY(real_routine, ptr);
				break;
			}
			else if (stat > 0)
				base = ptr;
			else
				top = ptr;

			if (n < 1)
				return 0;
		}
	}

	line_table += offset;
	if (line_table < first_line  ||  line_table >= first_line + real_routine->lnrtab_len)
		return 0;

	return line_table;
}
