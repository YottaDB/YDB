/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <iodef.h>
#include <ssdef.h>
#include <efndef.h>

#include "io.h"
#include "io_params.h"
#include "iombdef.h"
#include "compiler.h"
#include "stringpool.h"
#include "copy.h"

LITREF unsigned char io_params_size[];

void iomb_use(io_desc *iod, mval *pp)
{
	int4		length, width;
	uint4		status;
	d_mb_struct	*mb_ptr;
	params		ch;
	int		p_offset;

	error_def(ERR_DEVPARMNEG);

	p_offset = 0;
	iomb_flush(iod);
	mb_ptr = (d_mb_struct *)iod->dev_sp;
	while ((ch = *(pp->str.addr + p_offset++)) != iop_eol)
	{
		switch (ch)
		{
		case iop_writeof:
			status = sys$qiow(EFN$C_ENF, mb_ptr->channel
					,IO$_WRITEOF | IO$M_NOW, &mb_ptr->stat_blk
					,NULL, 0
					,0 ,0, 0, 0, 0, 0);
			if (status != SS$_NORMAL)
				rts_error(VARLSTCNT(1) status);
			break;
		case iop_delete:
			if (mb_ptr->prmflg && mb_ptr->del_on_close)
			{
				if ((status = sys$delmbx(mb_ptr->channel)) != SS$_NORMAL)
					rts_error(VARLSTCNT(1) status);
			}
			break;
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = pp->str.addr + p_offset + 1;
			s2pool(&iod->error_handler);
			break;
		case iop_length:
			GET_LONG(length, pp->str.addr + p_offset);
			if (length < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			iod->length = length;
			break;
		case iop_wait:
			mb_ptr->write_mask &= ~IO$M_NOW;
			break;
		case iop_nowait:
			mb_ptr->write_mask |= IO$M_NOW;
			break;
		case iop_width:
			GET_LONG(width, pp->str.addr + p_offset);
			if (width < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			if (width == 0)
			{
				iod->wrap = FALSE;
				iod->width = mb_ptr->maxmsg;
			} else
			{
				iod->width = width;
				iod->wrap = TRUE;
			}
			break;
		case iop_wrap:
			iod->wrap = TRUE;
			break;
		case iop_nowrap:
			iod->wrap = FALSE;
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
}
