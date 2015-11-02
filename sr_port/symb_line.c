/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"

#define OFFSET_LEN 5

GBLREF	unsigned char	*stackbase, *stacktop;
GBLREF	unsigned short	proc_act_type;
GBLREF	mstr		*err_act;
GBLREF	mval		dollar_ztrap;

unsigned char *symb_line(unsigned char *in_addr, unsigned char *out, unsigned char **b_line, rhdtyp *routine)
{
	unsigned char	temp[OFFSET_LEN], *adjusted_in_addr;
	lab_tabent	*max_label, *label_table, *last_label;
	lnr_tabent	*line_table, *last_line;
	int4		len, ct, offset, in_addr_offset;
	boolean_t	mpc_reset_to_linestart;

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
	if ((SFT_DEV_ACT == proc_act_type) || ((SFT_ZTRAP == proc_act_type) && (err_act == &dollar_ztrap.str)))
	{	/* This means we got an error while trying to compile the device-exception or ZTRAP string as
		 * part of handling yet another primary error. The primary error would have reset fp->mpc to
		 * the beginning of the line so note this down.
		 */
		mpc_reset_to_linestart = TRUE;
	} else
		mpc_reset_to_linestart = FALSE;
	label_table = LABTAB_ADR(routine);
	last_label = label_table + routine->labtab_len;
	max_label = label_table++;
	adjusted_in_addr = in_addr + (mpc_reset_to_linestart ? 1 : 0);
	while (label_table < last_label)
	{	/* Label table entries are sorted by label name (for faster lookup by indirects using op_labaddr) so we
		 * scan to find all of the label addresses that meet our specification (adjusted addr > label address) and
		 * keep the one with the lowest linenumber offset. This means we have to go through all of them rather than
		 * stop at the first label meeting our address criteria but this is not a high-use module so it is ok.
		 */
		if (adjusted_in_addr > LABEL_ADDR(routine, label_table))
		{	/* Now check if this label is a keeper by checking for minimum LNR_OFFSET */
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
	in_addr_offset = (int4)(adjusted_in_addr - CODE_BASE_ADDR(routine));
	last_line = LNRTAB_ADR(routine);
	last_line += routine->lnrtab_len;
	for ( ; ++line_table < last_line ; offset++)
	{	/* Find first line that is >= input addr. The previous line is the target line.
		 * In addition, ensure we never return NULL label and ZERO offset i.e. no ^MODULENAME.
		 * Line #s start at 1 so in this case, return +1^MODULENAME.
		 * The "(offset || len)" check below takes care of this.
		 */
		if ((in_addr_offset <= *line_table) && (offset || len))
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
