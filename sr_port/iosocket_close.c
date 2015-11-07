/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_close.c - close a socket connection
 *  Parameters-
 *  iod -- I/O descriptor for the current device.
 *
 *  pp -- mval that carries the device parameters
 *
 */
#include "mdef.h"

#include "gtm_string.h"
#include "gtm_iconv.h"
#include "gtm_stdio.h"

#include "gtm_socket.h"
#include "gtm_inet.h"

#include "copy.h"
#include "io_params.h"
#include "io.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "stringpool.h"

GBLREF tcp_library_struct	tcp_routines;
GBLREF io_desc		*active_device;
LITREF unsigned char		io_params_size[];
error_def(ERR_SOCKNOTFND);

void iosocket_close(io_desc *iod, mval *pp)
{
	boolean_t	socket_specified = FALSE;
	unsigned char	ch;
	int		handle_len;
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	char		sock_handle[MAX_HANDLE_LEN];
	int4		ii, jj, start, end, index;
	int		p_offset = 0;
	boolean_t socket_destroy = FALSE;

	assert(iod->type == gtmsocket);
	dsocketptr = (d_socket_struct *)iod->dev_sp;

	while (iop_eol != (ch = *(pp->str.addr + p_offset++)))
	{
		switch (ch)
		{
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&iod->error_handler);
			break;
		case iop_socket:
			handle_len = (short)(*(pp->str.addr + p_offset));
			assert(handle_len > 0);
			memcpy(sock_handle, (char *)(pp->str.addr + p_offset + 1), handle_len);
			socket_specified = TRUE;
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
		case iop_destroy:
			socket_destroy = TRUE;
			break;
		case iop_nodestroy:
			socket_destroy = FALSE;
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
	if (socket_specified)
	{
		if (0 > (index = iosocket_handle(sock_handle, &handle_len, FALSE, dsocketptr)))
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKNOTFND, 2, handle_len, sock_handle);
			return;
		}
		start = end = index;
	}
	else
	{
		start = dsocketptr->n_socket - 1;
		end = 0;
	}
	for (ii = start; ii >= end; ii--)
	{
		socketptr = dsocketptr->socket[ii];
		tcp_routines.aa_close(socketptr->sd);
		SOCKET_FREE(socketptr);
		if (dsocketptr->current_socket >= ii)
			dsocketptr->current_socket--;
		for (jj = ii + 1; jj <= dsocketptr->n_socket - 1; jj++)
			dsocketptr->socket[jj - 1] = dsocketptr->socket[jj];
		dsocketptr->n_socket--;
	}
	if (!socket_specified)
	{
		iod->state = dev_closed;
		if (socket_destroy)
		{
			active_device = 0;
			iosocket_destroy(iod);
		}
	}
}
