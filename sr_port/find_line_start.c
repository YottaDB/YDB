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
#include <rtnhdr.h>

GBLREF unsigned char *stackbase, *stacktop;

unsigned char *find_line_start(unsigned char *in_addr, rhdtyp *routine)
{
	unsigned char	*result;
	lab_tabent	*max_label, *label_table, *last_label;
	lnr_tabent	*line_table, *last_line;
	int4		in_addr_offset;

	result = (unsigned char *)0;

	if (!ADDR_IN_CODE(in_addr, routine))
		return result;
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
/* Used as offset !! */
	in_addr_offset = (int4)(in_addr - CODE_BASE_ADDR(routine));
	last_line = LNRTAB_ADR(routine);
	last_line += routine->lnrtab_len;
	for( ; ++line_table < last_line ;)
	{	/* Find first line that is > input addr. The previous line is the target line */
		if (in_addr_offset <= *line_table)
		{
			result = LINE_NUMBER_ADDR(routine, (line_table - 1));
			break;
		}
	}
	if (line_table >= last_line)
		result = LINE_NUMBER_ADDR(routine, (line_table - 1));

	return result;
}

