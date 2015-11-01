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
#include "rtnhdr.h"

GBLREF unsigned char *stackbase, *stacktop;

unsigned char *find_line_start(in_addr, routine)
unsigned char *in_addr;
rhdtyp		*routine;
{
	unsigned char	*result;
	lbl_tables	*max_label, *label_table, *last_label;
	uint4	*line_table, *last_line, len, ct;
	uint4	in_addr_offset;

	result = (unsigned char *)0;

	if (in_addr > stacktop && in_addr < stackbase)
		return result;
	if (in_addr < (unsigned char *) routine + routine->ptext_ptr ||
		in_addr >= (unsigned char *) routine + routine->vartab_ptr)
		return result;
	routine = (rhdtyp *)((unsigned char *) routine + routine->current_rhead_ptr);
	assert(routine->labtab_ptr >= 0);
	assert(routine->labtab_len >= 0);
	assert(routine->lnrtab_ptr >= 0);
	assert(routine->lnrtab_len >= 0);
	label_table = (lbl_tables *)((char *) routine + routine->labtab_ptr);
	last_label = label_table + routine->labtab_len;
	max_label = label_table++;
	while (label_table < last_label)
	{
		if (in_addr > (unsigned char *) routine + *((int4 *) ((char *) routine + label_table->lab_ln_ptr)))
		{
			if (max_label->lab_ln_ptr <= label_table->lab_ln_ptr)
				max_label = label_table;
		}
		label_table++;
	}
	line_table = (uint4 *)((char *) routine + max_label->lab_ln_ptr);
	in_addr_offset = in_addr - (unsigned char *) routine;
	last_line = (uint4 *)((char *) routine + routine->lnrtab_ptr);
	last_line += routine->lnrtab_len;
	for( ; ++line_table < last_line ;)
	{
		if (in_addr_offset <= *line_table)
		{
			result = (unsigned char *) routine + *(line_table - 1);
			break;
		}
	}
	if (line_table >= last_line)
		result = (unsigned char *) routine + *(line_table - 1);

	return result;
}

