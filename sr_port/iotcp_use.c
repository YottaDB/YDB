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

#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_iconv.h"
#include "gtm_stdio.h"

#include "copy.h"
#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "iotcpdef.h"
#include "stringpool.h"

typedef struct
{
	unsigned short mem;
	unsigned short grp;
} uic_struct;

LITREF unsigned char	io_params_size[];

void	iotcp_use(io_desc *iod, mval *pp)
{
	unsigned char	c;
	int4		length, width;
	d_tcp_struct	*tcpptr, newtcp;
	int		p_offset;

	error_def(ERR_DEVPARMNEG);
	error_def(ERR_RMWIDTHPOS);

#ifdef DEBUG_TCP
	PRINTF("%s >>>\n", __FILE__);
#endif
	p_offset = 0;
	tcpptr = (d_tcp_struct *)iod->dev_sp;
	/* copy existing parameters */
	memcpy(&newtcp, tcpptr, SIZEOF(d_tcp_struct));
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		assert((params) *(pp->str.addr + p_offset) < (params) n_iops);
		switch (c = *(pp->str.addr + p_offset++))
		{
		case iop_width:
			GET_LONG(width, pp->str.addr + p_offset);
			if (width == 0)
				newtcp.width = TCPDEF_WIDTH;
			else  if (width > 0)
				newtcp.width = width;
			else
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			break;
		case iop_length:
			GET_LONG(length, pp->str.addr + p_offset);
			if (length == 0)
				newtcp.length = TCPDEF_LENGTH;
			else  if (length > 0)
				newtcp.length = length;
			else
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			break;
		case iop_urgent:
			newtcp.urgent = TRUE;
			break;
		case iop_nourgent:
			newtcp.urgent = FALSE;
			break;
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&iod->error_handler);
			break;
		case iop_ipchset:
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
			if ( (iconv_t)0 != iod->input_conv_cd )
			{
				ICONV_CLOSE_CD(iod->input_conv_cd);
			}
			SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
			if (DEFAULT_CODE_SET != iod->in_code_set)
				ICONV_OPEN_CD(iod->input_conv_cd, INSIDE_CH_SET, (char *)(pp->str.addr + p_offset + 1));
#endif
			break;
                case iop_opchset:
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
			if ( (iconv_t)0 != iod->output_conv_cd )
			{
				ICONV_CLOSE_CD(iod->output_conv_cd);
			}
			SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
			if (DEFAULT_CODE_SET != iod->out_code_set)
				ICONV_OPEN_CD(iod->output_conv_cd, (char *)(pp->str.addr + p_offset + 1), INSIDE_CH_SET);
#endif
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[c]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[c]);
	}

	/* commit changes */
	memcpy(tcpptr, &newtcp, SIZEOF(d_tcp_struct));
#ifdef DEBUG_TCP
	PRINTF("%s <<<\n", __FILE__);
#endif
	return;
}
