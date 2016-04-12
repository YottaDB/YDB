/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_iconv.h"
#include "gtm_stdlib.h"

#include "copy.h"
#include "io_params.h"
#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "nametabtyp.h"
#include "stringpool.h"
#include "namelook.h"

LITREF nametabent filter_names[];
LITREF unsigned char filter_index[27];
LITREF unsigned char io_params_size[];

void ionl_use(io_desc *iod, mval *pp)
{
	unsigned char	ch, len;
	int		fil_type;
	int4		width, length;
	io_desc		*d_in, *d_out;
	char		*tab;
	int		p_offset;

	error_def(ERR_TTINVFILTER);
	error_def(ERR_DEVPARMNEG);

	p_offset = 0;
	d_in = iod->pair.in;
	d_out = iod->pair.out;
	assert(iod->state == dev_open);
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		switch (ch = *(pp->str.addr + p_offset++))
		{
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&iod->error_handler);
			break;
		case iop_filter:
			len = *(pp->str.addr + p_offset);
			tab = pp->str.addr + p_offset + 1;
			if ((fil_type = namelook(filter_index, filter_names, tab, len)) < 0)
			{
				rts_error(VARLSTCNT(1) ERR_TTINVFILTER);
				return;
			}
			switch (fil_type)
			{
				case 0:
					iod->write_filter |= CHAR_FILTER;
					break;
				case 1:
					iod->write_filter |= ESC1;
					break;
				case 2:
					iod->write_filter &= ~CHAR_FILTER;
					break;
				case 3:
					iod->write_filter &= ~ESC1;
					break;
			}
			break;
		case iop_nofilter:
			iod->write_filter = 0;
			break;
		case iop_length:
			GET_LONG(length, pp->str.addr + p_offset);
			if (length < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			d_out->length = length;
			break;
		case iop_width:
			GET_LONG(width, pp->str.addr + p_offset);
			if (width < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			if (width == 0)
			{
				d_out->wrap = FALSE;
				d_out->width = TTDEF_PG_WIDTH;
			}
			else
			{
				d_out->width = width;
				d_out->wrap = TRUE;
			}
			break;
		case iop_wrap:
			d_out->wrap = TRUE;
			break;
		case iop_nowrap:
			d_out->wrap = FALSE;
			break;
		case iop_x:
		{
			int4 col;

			GET_LONG(col, pp->str.addr + p_offset);
			d_out->dollar.x = col;
			if ((int4)(d_out->dollar.x) < 0)
				d_out->dollar.x = 0;
			if (d_out->dollar.x > d_out->width && d_out->wrap)
				d_out->dollar.x %= d_out->width;
			break;
		}
		case iop_y:
		{
			int4 row;

			GET_LONG(row, (pp->str.addr + p_offset));
			d_out->dollar.y = row;
			if ((int4)(d_out->dollar.y) < 0)
				d_out->dollar.y = 0;
			if (d_out->length)
				d_out->dollar.y %= d_out->length;
			break;
		}
		case iop_ipchset:
			{
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
				if ( (iconv_t)0 != iod->input_conv_cd )
				{
					ICONV_CLOSE_CD(iod->input_conv_cd);
				}
				SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->in_code_set)
					ICONV_OPEN_CD(iod->input_conv_cd, (char *)(pp->str.addr + p_offset + 1), INSIDE_CH_SET);
#endif
                        	break;
			}
                case iop_opchset:
			{
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
				if ( (iconv_t) 0 != iod->output_conv_cd )
				{
					ICONV_CLOSE_CD(iod->output_conv_cd);
				}
				SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->out_code_set)
					ICONV_OPEN_CD(iod->output_conv_cd, INSIDE_CH_SET, (char *)(pp->str.addr + p_offset + 1));
#endif
                        	break;
			}
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
	return;
}
