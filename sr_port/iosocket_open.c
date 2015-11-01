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
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#ifdef __MVS__
#include <arpa/inet.h>
#endif
#include <netinet/in.h>
#include "gtm_stdio.h"
#include "copy.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "iotcp_select.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "io_params.h"
#include "iosocketdef.h"
#include "gtm_caseconv.h"
#include "stringpool.h"

GBLREF 	tcp_library_struct	tcp_routines;
GBLREF	d_socket_struct		*socket_pool;
LITREF 	unsigned char		io_params_size[];
short	iosocket_open(io_log_name *dev, mval *pp, int file_des, mval *mspace, int4 timepar)
{
	char			addr[SA_MAXLITLEN], *errptr, sockaddr[SA_MAXLITLEN],
				temp_addr[SA_MAXLITLEN],dev_type[MAX_DEV_TYPE_LEN];
	unsigned char		ch, delimiter_blen, len, *c, *next, *top;
	short			length, width, handle_len;
	unsigned short		port;
	int4			errlen, msec_timeout, real_errno, p_offset = 0;
	int			ii, rv, size, on = 1, temp_1 = -2;
	ABS_TIME		cur_time, end_time;
	io_desc			*ioptr;
	struct sockaddr_in	peer;		/* socket address + port */
	fd_set			tcp_fd;
	uint4			bfsize = DEFAULT_SOCKET_BUFFER_SIZE, ibfsize;
        d_socket_struct         *dsocketptr, newdsocket;
	socket_struct		*socketptr;
	boolean_t		attach_specified = FALSE,
				listen_specified = FALSE,
				connect_specified = FALSE,
				delimiter_specified = FALSE,
				nodelimiter_specified = FALSE,
				ioerror_specified = FALSE,
				delay_specified = FALSE,
				nodelay_specified = FALSE,
				ibfsize_specified = FALSE;
	char 			delimiter_buffer[MAX_N_DELIMITER * (MAX_DELIM_LEN + 1)],
				ioerror, ip[3], tcp[4],
				sock_handle[MAX_HANDLE_LEN], delimiter[MAX_DELIM_LEN + 1];
	error_def(ERR_DELIMSIZNA);
	error_def(ERR_ADDRTOOLONG);
	error_def(ERR_SOCKETEXIST);
	error_def(ERR_ABNCOMPTINC);
	error_def(ERR_DEVPARINAP);
	error_def(ERR_ILLESOCKBFSIZE);
	ioptr = dev->iod;
	assert((params) *(pp->str.addr + p_offset) < (unsigned char)n_iops);
	assert(ioptr != 0);
	assert(ioptr->state >= 0 && ioptr->state < n_io_dev_states);
	assert(ioptr->type == gtmsocket);
	if ((ioptr->state == dev_closed) && mspace && mspace->str.len && mspace->str.addr)
	{
		lower_to_upper((uchar_ptr_t)dev_type, (uchar_ptr_t)mspace->str.addr, mspace->str.len);
		if (6 != mspace->str.len || 0 != memcmp(dev_type, "SOCKET", 6))
		{
			if (ioptr->dev_sp)
				free(ioptr->dev_sp);
			ioptr->state = dev_never_opened;
		}
	}
	if (ioptr->state == dev_never_opened)
	{
		ioptr->dev_sp = (void *)malloc(sizeof(d_socket_struct));
		memset(ioptr->dev_sp, 0, sizeof(d_socket_struct));
	}
	dsocketptr = (d_socket_struct *)ioptr->dev_sp;
	if (ioptr->state == dev_never_opened)
	{
		ioptr->state	= dev_closed;
		ioptr->width	= TCPDEF_WIDTH;
		ioptr->length	= TCPDEF_LENGTH;
		ioptr->wrap	= TRUE;
		if (-1 == iotcp_fillroutine())
			assert(FALSE);
	}
	ioptr->dollar.zeof = FALSE;
	newdsocket = *dsocketptr;
	memcpy(newdsocket.dollar_device, "0", sizeof("0"));
	while (iop_eol != (ch = *(pp->str.addr + p_offset++)))
	{
		switch  (ch)
		{
		case	iop_delimiter:
			delimiter_specified = TRUE;
			delimiter_blen = *(pp->str.addr + p_offset);
			if (delimiter_blen <= (MAX_DELIM_LEN + 1) * MAX_N_DELIMITER)
			{
				memset(delimiter_buffer, 0, (MAX_DELIM_LEN + 1) * MAX_N_DELIMITER);
				memcpy(delimiter_buffer, pp->str.addr + p_offset + 1, delimiter_blen);
			}
			else
				rts_error(VARLSTCNT(1) ERR_DELIMSIZNA);
			break;
		case	iop_nodelimiter:
			nodelimiter_specified = TRUE;
			break;
		case	iop_zdelay:
			delay_specified = TRUE;
			break;
		case	iop_znodelay:
			nodelay_specified = TRUE;
			break;
		case	iop_zbfsize:
			GET_ULONG(bfsize, pp->str.addr + p_offset);
			if ((0 == bfsize) || (MAX_SOCKET_BUFFER_SIZE < bfsize))
				rts_error(VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, bfsize);
			break;
		case	iop_zibfsize:
			ibfsize_specified = TRUE;
			GET_ULONG(ibfsize, pp->str.addr + p_offset);
			if ((0 == ibfsize) || (MAX_INTERNAL_SOCBUF_SIZE < ibfsize))
				rts_error(VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, bfsize);
			break;
		case	iop_zlisten:
			listen_specified = TRUE;
			len = *(pp->str.addr + p_offset);
                        if (len < SA_MAXLITLEN)
                        {
                                memset(sockaddr, 0, sizeof(sockaddr));
                                memcpy(sockaddr, pp->str.addr + p_offset + 1, len);
                        }
                        else
				rts_error(VARLSTCNT(1) ERR_ADDRTOOLONG);
                        break;
		case	iop_connect:
			connect_specified = TRUE;
			len = *(pp->str.addr + p_offset);
			if (len < SA_MAXLITLEN)
			{
				memset(sockaddr, 0, sizeof(sockaddr));
                                memcpy(sockaddr, pp->str.addr + p_offset + 1, len);
			}
			else
				rts_error(VARLSTCNT(1) ERR_ADDRTOOLONG);
			break;
		case	iop_ioerror:
			ioerror_specified = TRUE;
			ioerror = *(pp->str.addr + p_offset + 1);	/* the first char decides */
    			break;
		case	iop_exception:
			ioptr->error_handler.len = *(pp->str.addr + p_offset);
			ioptr->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&ioptr->error_handler);
			break;
		case	iop_attach:
			attach_specified = TRUE;
			handle_len = (short)(*(pp->str.addr + p_offset));
			if (handle_len > MAX_HANDLE_LEN)
			{
				handle_len = MAX_HANDLE_LEN;
			}
			memcpy(sock_handle, pp->str.addr + p_offset + 1, handle_len);
			break;
		case	iop_socket:
			rts_error(VARLSTCNT(1) ERR_DEVPARINAP);
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}

        if (listen_specified && connect_specified)
        {
		rts_error(VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("CONNECT"),
			LEN_AND_LIT("ZLISTEN"), LEN_AND_LIT("OPEN"));
                return FALSE;
        }
	if (delay_specified && nodelay_specified)
	{
		rts_error(VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("DELAY"),
			LEN_AND_LIT("NODELAY"), LEN_AND_LIT("OPEN"));
		return FALSE;
	}
	if (listen_specified || connect_specified)
	{
		if (NULL == (socketptr = iosocket_create(sockaddr, bfsize)))
		{
			return FALSE;
		}
		assert(listen_specified == socketptr->passive);
		if (ioerror_specified)
			socketptr->ioerror = ('T' == ioerror || 't' == ioerror);
		socketptr->nodelay = nodelay_specified;		/* defaults to DELAY */
		if (ibfsize_specified)
			socketptr->bufsiz = ibfsize;
                /* socket handle -- also check for duplication */
		if (attach_specified)
		{
			if (iosocket_handle(sock_handle, &handle_len, FALSE, &newdsocket) >= 0)
			{
                  		free(socketptr);
				rts_error(VARLSTCNT(4) ERR_SOCKETEXIST, 2, handle_len, sock_handle);
                  		return FALSE;
                	}
		}
		else
			iosocket_handle(sock_handle, &handle_len, TRUE, dsocketptr);
		socketptr->handle_len = handle_len;
		memcpy(socketptr->handle, sock_handle, handle_len);
                /* parse the delimiter: delimiter_buffer ==> socketptr->delimiter[...] */
                if (delimiter_specified || nodelimiter_specified)
                        iosocket_delimiter(delimiter_buffer, delimiter_blen, socketptr, nodelimiter_specified);
		/* connects newdsocket and socketptr (the new socket) */
                socketptr->dev = &newdsocket;
                newdsocket.socket[newdsocket.n_socket++] = socketptr;
		newdsocket.current_socket = newdsocket.n_socket - 1;
	}
	/* action */
	if ((listen_specified && (!iosocket_bind(socketptr, timepar, ibfsize_specified))) ||
		(connect_specified && (!iosocket_connect(socketptr, timepar, ibfsize_specified))))
       	{
		if (socketptr->sd > 0)
			(void)tcp_routines.aa_close(socketptr->sd);
		iosocket_delimiter((char *)0, 0, socketptr, TRUE);
		free(socketptr);
		return FALSE;
	}
	/* commit the changes to the list */
	if (listen_specified || connect_specified)
	{
		socketptr->dev = dsocketptr;
		*dsocketptr = newdsocket;
 		/* memcpy(dsocketptr, &newdsocket, sizeof(d_socket_struct)); */
	}
       	ioptr->state = dev_open;
	return TRUE;
}
