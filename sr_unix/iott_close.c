/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_unistd.h"

#include <errno.h>

#include "io.h"
#include "iottdef.h"
#include "io_params.h"
#include "gtmio.h"
#include "stringpool.h"
#include "setterm.h"
#include "error.h"

GBLREF io_pair		io_std_device;
LITREF unsigned char	io_params_size[];

error_def(ERR_SYSCALL);

void iott_close(io_desc *v, mval *pp)
{
	d_tt_struct	*ttptr;
	params		ch;
	int		status;
	int		p_offset;
	boolean_t	ch_set;

	assert(v->type == tt);
	if (v->state != dev_open)
		return;
	ESTABLISH_GTMIO_CH(&v->pair, ch_set);
	iott_flush(v);
	if (v->pair.in != v)
		assert(v->pair.out == v);
	ttptr = (d_tt_struct *)v->dev_sp;
	if (v->pair.out != v)
		assert(v->pair.in == v);
	v->state = dev_closed;
	resetterm(v);

	p_offset = 0;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		if ((ch = *(pp->str.addr + p_offset++)) == iop_exception)
		{
			v->error_handler.len = *(pp->str.addr + p_offset);
			v->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&v->error_handler);
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
	if (v == io_std_device.in || (v == io_std_device.out))
	{
		REVERT_GTMIO_CH(&v->pair, ch_set);
		return;
	}

	CLOSEFILE_RESET(ttptr->fildes, status);	/* resets "ttptr->fildes" to FD_INVALID */
	if (0 != status)
	{
		assert(status == errno);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				RTS_ERROR_LITERAL("iott_close(CLOSEFILE)"), CALLFROM, status);
	}
	if (ttptr->recall_buff.addr)
	{
		free(ttptr->recall_buff.addr);
		ttptr->recall_buff.addr = NULL;
	}
	if (v->dollar.devicebuffer)
	{
		free(v->dollar.devicebuffer);
		v->dollar.devicebuffer = NULL;
	}
	REVERT_GTMIO_CH(&v->pair, ch_set);
	return;
}
