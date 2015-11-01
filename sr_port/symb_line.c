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
#include "stack_frame.h"

#define OFFSET_LEN 5

GBLREF unsigned char *stackbase, *stacktop;

unsigned char *symb_line(unsigned char *in_addr,unsigned char *out, unsigned char **b_line, rhdtyp *routine)
{
	unsigned char	temp[OFFSET_LEN];
	lbl_tables	*max_label, *label_table, *last_label;
	uint4	*line_table, *last_line, len, ct;
	uint4	offset, in_addr_offset;


	if (in_addr > stacktop && in_addr < stackbase)
		return out;
	if (in_addr < (unsigned char *) routine + routine->ptext_ptr ||
		in_addr >= (unsigned char *) routine + routine->vartab_ptr)
		return out;
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
	len = mid_len(&max_label->lab_name);
	if(len)
	{
		memcpy(out, &max_label->lab_name.c[0], len);
		out += len;
	}
	offset = 0;
	in_addr_offset = in_addr - (unsigned char *) routine;
	last_line = (uint4 *)((char *) routine + routine->lnrtab_ptr);
	last_line += routine->lnrtab_len;
	for( ; ++line_table < last_line ; offset++)
	{
		if(in_addr_offset <= *line_table)
		{
			if (b_line)
				*b_line = (unsigned char *) routine + *(line_table - 1);
			break;
		}
	}
	if (line_table >= last_line && b_line)
			*b_line = (unsigned char *) routine + *(line_table - 1);
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
	len = mid_len(&routine->routine_name);
	memcpy(out, &routine->routine_name.c[0], len);
	out += len;
	return out;
}
