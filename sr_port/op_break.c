/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "io.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "break.h"
#include "op.h"
#include "dm_setup.h"
#include "min_max.h"

GBLREF io_pair	io_curr_device;
GBLREF io_pair	io_std_device;
GBLREF stack_frame *frame_pointer;
GBLREF mval	dollar_zstatus;
GBLREF int4 	break_message_mask;
GBLREF bool	dec_nofac;

void op_break(void)
{
	bool		do_msg;
	char		*c, *c_top, *line_start;
	char		line[MAX_ENTRYREF_LEN];
	int		line_length;
	int		em;

	error_def(ERR_RTSLOC);
	error_def(ERR_BREAK);
	error_def(ERR_BREAKDEA);
	error_def(ERR_BREAKZBA);
	error_def(ERR_BREAKZST);
	error_def(ERR_NOTPRINCIO);
	error_def(ERR_TEXT);

	flush_pio();
	line_length = 0;
	if (frame_pointer->type & SFT_ZTRAP)
	{
		c = dollar_zstatus.str.addr;
		c_top = c + dollar_zstatus.str.len;
		/* Skip the error message # in $ZSTATUS */
		while (c < c_top) if (*c++ == ',') break;
		/* Note down the source-line information in $ZSTATUS */
		line_start = c;
		while (c < c_top) if (*c++ == ',') break;
		if (c < c_top)
		{
			line_length = (int)(c - 1 - line_start);	/* Go back one ',' */
			assert(SIZEOF(line) >= line_length);
			memcpy(line, line_start, MIN(line_length, SIZEOF(line)));
		}
		/* The last part of $ZSTATUS is the expanded error message text. Print that followed by the M-source line# */
		do_msg = (c < c_top);
		if (c < c_top)
		{
			dec_nofac = TRUE;
			dec_err(VARLSTCNT(4) ERR_TEXT, 2, c_top - c, c);
			dec_nofac = FALSE;
		}
	} else
	{
		do_msg = FALSE;
		em = 0;
		if (frame_pointer->type & SFT_DEV_ACT)
		{	if (break_message_mask & DEVBREAK_MASK)
				em = (int) ERR_BREAKDEA;
		}
		else if ( frame_pointer->type & SFT_ZBRK_ACT)
		{	if (break_message_mask & ZBREAK_MASK)
				em = (int) ERR_BREAKZBA;
		}
		else if ( frame_pointer->type & SFT_ZSTEP_ACT)
		{	if (break_message_mask & ZSTBREAK_MASK)
				em = (int) ERR_BREAKZST;
		}
		else if (break_message_mask & BREAK_MASK)
		{
			em = (int) ERR_BREAK;
		}
		if (em)
		{	dec_err(VARLSTCNT(1) em);
			do_msg = TRUE;
		}
		if (do_msg)
			line_length = (int)(get_symb_line((uchar_ptr_t)line, 0, 0) - (uchar_ptr_t)line);
	}
	if (do_msg && (0 != line_length))
	{
		dec_nofac = TRUE;
		dec_err(VARLSTCNT(4) ERR_RTSLOC, 2, line_length, line);
		dec_nofac = FALSE;
	}

	if (io_curr_device.out != io_std_device.out)
	{
		dec_err(VARLSTCNT(4) ERR_NOTPRINCIO, 2,
			io_curr_device.out->trans_name->len,
			io_curr_device.out->trans_name->dollar_io);
	}
	dm_setup();
}
