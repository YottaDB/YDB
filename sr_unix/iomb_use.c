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

#include "gtm_string.h"

#include <errno.h>
#include "gtm_iconv.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iosp.h"
#include "iombdef.h"
#include "io_params.h"
#include "compiler.h"
#include "stringpool.h"

LITREF unsigned char io_params_size[];

void iomb_use(io_desc *iod, mval *pp)
{
	char		*path;
	uint4		status;
	d_mb_struct	*mb_ptr;
	params		ch;
	int		p_offset;

	p_offset = 0;
	iomb_flush(iod);
	mb_ptr = (d_mb_struct *)iod->dev_sp;
	while ( (ch = *(pp->str.addr + p_offset++)) != iop_eol)
	{
		switch (ch)
		{
		case iop_delete:
			if (mb_ptr->prmflg && mb_ptr->del_on_close)
			{
				close(mb_ptr->channel);
				path = iod->trans_name->dollar_io;
				if ((status = UNLINK(path)) == -1)
				rts_error(VARLSTCNT(1) errno);
			}
			break;
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&iod->error_handler);
			break;
		case iop_wait:
			mb_ptr->write_mask = 1;
			break;
		case iop_nowait:
			mb_ptr->write_mask = 0;
			break;
		case iop_ipchset:
			{
				if ( (iconv_t)0 != iod->input_conv_cd )
				{
					ICONV_CLOSE_CD(iod->input_conv_cd);
				}
				SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->in_code_set)
					ICONV_OPEN_CD(iod->input_conv_cd, (char *)(pp->str.addr + p_offset + 1), INSIDE_CH_SET);
                        	break;
			}
                case iop_opchset:
			{
				if ( (iconv_t) 0 != iod->output_conv_cd )
				{
					ICONV_CLOSE_CD(iod->output_conv_cd);
				}
				SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->out_code_set)
					ICONV_OPEN_CD(iod->output_conv_cd, INSIDE_CH_SET, (char *)(pp->str.addr + p_offset + 1));
                        	break;
			}
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[*(pp->str.addr + p_offset)]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[*(pp->str.addr + p_offset)]);
	}
}
