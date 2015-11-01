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
#include "gtm_unistd.h"

#include <errno.h>

#include "io.h"
#include "iombdef.h"
#include "io_params.h"
#include "stringpool.h"

LITREF unsigned char io_params_size[];

void  iomb_close(io_desc *device, mval *pp)
{
	char		*path;
	unsigned char	ch;
	int		status;
	d_mb_struct	*mb_ptr;
	int		p_offset;

	p_offset = 0;
	mb_ptr = (d_mb_struct *)device->dev_sp;
	if (device->state == dev_open)
	{
		while ((ch = *(pp->str.addr + p_offset++)) != iop_eol)
		{
			if (ch == iop_delete)
			{
				mb_ptr->del_on_close = TRUE;
				break;
			}
			if (ch == iop_exception)
			{
				device->error_handler.len = *(pp->str.addr + p_offset);
				device->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
				s2pool(&device->error_handler);
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
				(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
		}
		close(mb_ptr->channel);
		if (mb_ptr->del_on_close || (!mb_ptr->prmflg))
		{
			path = device->trans_name->dollar_io;
			if ((status = UNLINK(path)) == -1)
				rts_error(VARLSTCNT(1) errno);
		}
	}
	device->state = dev_closed;
	return;
}
