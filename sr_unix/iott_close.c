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
#include "gtm_unistd.h"

#include <errno.h>

#include "io.h"
#include "iottdef.h"
#include "io_params.h"
#include "gtmio.h"
#include "stringpool.h"
#include "setterm.h"

GBLREF io_pair		io_std_device;
LITREF unsigned char	io_params_size[];

void iott_close(io_desc *v, mval *pp)
{
	/* only exception allowed */
	error_def(ERR_SYSCALL);

	d_tt_struct	*ttptr;
	params		ch;
	int		status;
	int		p_offset;

	assert(v->type == tt);
	if (v->state != dev_open)
		return;
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
		return;

	CLOSEFILE_RESET(ttptr->fildes, status);	/* resets "ttptr->fildes" to FD_INVALID */
	if (0 != status)
	{
		assert(status == errno);
		rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("iott_close(CLOSEFILE)"), CALLFROM, status);
	}
	if (ttptr->recall_buff.addr)
	{
		free(ttptr->recall_buff.addr);
		ttptr->recall_buff.addr = NULL;
	}
	return;
}
