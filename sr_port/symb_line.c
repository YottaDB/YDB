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
#include "stack_frame.h"

#define OFFSET_LEN 5

GBLREF unsigned char *stackbase, *stacktop;

unsigned char *symb_line(unsigned char *in_addr, unsigned char *out, unsigned char **b_line, rhdtyp *routine)
{
	unsigned char	temp[OFFSET_LEN];
	lab_tabent	*max_label, *label_table, *last_label;
	lnr_tabent	*line_table, *last_line;
	int4		len, ct, offset;
        int4	        in_addr_offset;

	if (!ADDR_IN_CODE(in_addr, routine))
		return out;
	routine = CURRENT_RHEAD_ADR(routine);
	USHBIN_ONLY(
		assert(routine->labtab_adr);
		assert(routine->lnrtab_adr);
		);
	NON_USHBIN_ONLY(
		assert(routine->labtab_ptr >= 0);
		assert(routine->lnrtab_ptr >= 0);
		);
	assert(routine->labtab_len >= 0);
	assert(routine->lnrtab_len >= 0);
	label_table = LABTAB_ADR(routine);
	last_label = label_table + routine->labtab_len;
	max_label = label_table++;
	while (label_table < last_label)
	{	/* Find first label that goes past the input addr. The previous label is then the target line */
		if (in_addr > LABEL_ADDR(routine, label_table))
		{
			if (max_label->LABENT_LNR_OFFSET <= label_table->LABENT_LNR_OFFSET)
				max_label = label_table;
		}
		label_table++;
	}
	line_table = LABENT_LNR_ENTRY(routine, max_label);
	len = max_label->lab_name.len;
	if (len)
	{
		memcpy(out, max_label->lab_name.addr, len);
		out += len;
	}
	offset = 0;
	in_addr_offset = (int4)(in_addr - CODE_BASE_ADDR(routine));
	last_line = LNRTAB_ADR(routine);
	last_line += routine->lnrtab_len;
	for( ; ++line_table < last_line ; offset++)
	{	/* Find first line that is > input addr. The previous line is the target line */
		if (in_addr_offset <= *line_table)
		{
			if (b_line)
				*b_line = LINE_NUMBER_ADDR(routine, (line_table - 1));
			break;
		}
	}
	if (line_table >= last_line && b_line)
		*b_line = LINE_NUMBER_ADDR(routine, (line_table - 1));
	if (offset)
	{
		*out++ = '+';
		ct = OFFSET_LEN;
		for ( ; ct > 0 ; )
		{
			temp[--ct] = (offset % 10) + '0';
			if ((offset /= 10) == 0)
				break;
		}
		len = OFFSET_LEN - ct;
		memcpy (out, &temp[ct], len);
		out += len;
	}
	*out++ = '^';
	len = routine->routine_name.len;
	memcpy(out, routine->routine_name.addr, len);
	out += len;
	return out;
}
