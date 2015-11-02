/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "min_max.h"

GBLREF command_qualifier	cmd_qlf;

int4* find_line_addr (rhdtyp *routine, mstr *label, int4 offset, mident **lent_name)
{
 	lab_tabent	*base, *top, *ptr;
	rhdtyp		*real_routine;
	mident_fixed	target_label;
	mident		lname;
	lnr_tabent	*line_table, *first_line;
	int		stat, n;
	error_def(ERR_LABELONLY);

	if (!routine)
		return NULL;
	real_routine = CURRENT_RHEAD_ADR(routine);
	first_line = LNRTAB_ADR(real_routine);

	if (!label->len  ||  !*label->addr)
	{ /* No label specified. Return the first line */
		base = LABTAB_ADR(real_routine);
		assert(0 == base->lab_name.len);
		if (lent_name)
			*lent_name = &base->lab_name;
		line_table = first_line;
	}
	else
	{
		lname.len = (label->len <= MAX_MIDENT_LEN) ? label->len : MAX_MIDENT_LEN;
		if (cmd_qlf.qlf & CQ_LOWER_LABELS)
			lname.addr = label->addr;
		else
		{
			lower_to_upper((uchar_ptr_t)&target_label.c[0], (uchar_ptr_t)label->addr, lname.len);
			lname.addr = &target_label.c[0];
		}

		ptr = base = LABTAB_ADR(real_routine);
		top = base + real_routine->labtab_len;
		for (  ;  ;  )
		{
			n = (int)(top - base) / 2;
			ptr = base + n;
			MIDENT_CMP(&lname, &ptr->lab_name, stat);
			if (0 == stat)
			{
				if (lent_name)
					*lent_name = &ptr->lab_name;
				line_table = LABENT_LNR_ENTRY(real_routine, ptr);
				break;
			}
			else if (stat > 0)
				base = ptr;
			else
				top = ptr;

			if (n < 1)
				return NULL;
		}
	}

	line_table += offset;
	if (line_table < first_line  ||  line_table >= first_line + real_routine->lnrtab_len)
		return NULL;

	return line_table;
}
