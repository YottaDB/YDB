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

#include <iodef.h>
#include <ssdef.h>
#include <trmdef.h>
#include <ttdef.h>
#include <tt2def.h>
#include <efndef.h>


#include "efn.h"
#include "io.h"
#include "iottdef.h"
#include "io_params.h"
#include "outofband.h"
#include "stringpool.h"

GBLREF short		astq_dyn_alloc;
GBLREF short		astq_dyn_avail;
GBLREF io_pair		io_std_device;
GBLREF uint4		std_dev_outofband_msk;
LITREF unsigned char	io_params_size[];

void iott_close(io_desc *v, mval *pp)	/* exception is the only deviceparameter allowed */
{
	unsigned short		iosb[4];
	uint4			dummy_msk, enable_msk, status;
	d_tt_struct		*tt_ptr;
	t_cap			s_mode;
	params			ch;
	int			p_offset;

	assert(v->type == tt);
	if (v->state != dev_open)
		return;

	assert((v->pair.in == v) || (v->pair.out == v));
	tt_ptr = (d_tt_struct *)(v->dev_sp);

	if (v->pair.out == v)
	{
		status = sys$dclast(iott_wtclose, tt_ptr, 0);
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
	}

	if (v->pair.in == v && tt_ptr->term_chars_twisted)
	{
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel, IO$_SENSEMODE,
			iosb, 0, 0, &s_mode, 12, 0, 0, 0, 0);
		if (status == SS$_NORMAL)
			status = iosb[0];
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
		s_mode.ext_cap &= (~TT2$M_PASTHRU & ~TT2$M_EDITING);
		s_mode.ext_cap |= (tt_ptr->ext_cap & (TT2$M_PASTHRU | TT2$M_EDITING));
		s_mode.term_char &= (~TT$M_ESCAPE);
		s_mode.term_char |= (tt_ptr->term_char & TT$M_ESCAPE);
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel, IO$_SETMODE,
			iosb, 0, 0, &s_mode, 12, 0, 0, 0, 0);
		if (status == SS$_NORMAL)
			status = iosb[0];
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
	}

	if (v->pair.in == io_std_device.in)
	{
		enable_msk = std_dev_outofband_msk & CTRLY_MSK;
		if (enable_msk == CTRLY_MSK)
			status = lib$enable_ctrl(&enable_msk, &dummy_msk);
			if (status != SS$_NORMAL)
				rts_error(VARLSTCNT(1) status);
	}

	status = sys$dassgn(tt_ptr->channel);
	if (status != SS$_NORMAL)
		rts_error(VARLSTCNT(1) status);

	p_offset = 0;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		if ((ch = *(pp->str.addr + p_offset++)) == iop_exception)
		{
			v->error_handler.len = *(pp->str.addr + p_offset);
			v->error_handler.addr = pp->str.addr + p_offset + 1;
			s2pool(&v->error_handler);
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}

	v->state = dev_closed;
	astq_dyn_alloc += TERMINAL_STATIC_ASTS;
	astq_dyn_avail += TERMINAL_STATIC_ASTS;
	free(tt_ptr->sb_buffer);
	free(tt_ptr->io_buffer);

	return;
}
