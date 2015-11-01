/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_time.h"
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
GBLREF	io_pair			io_std_device;	/* standard device */

#define ESTABLISHED		"ESTABLISHED"

short	iosocket_open(io_log_name *dev, mval *pp, int file_des, mval *mspace, int4 timepar)
{
	char			addr[SA_MAXLITLEN], *errptr, sockaddr[SA_MAXLITLEN],
				temp_addr[SA_MAXLITLEN],dev_type[MAX_DEV_TYPE_LEN];
	unsigned char		ch, len, *c, *next, *top;
	short			handle_len;
	unsigned short		port;
	int4			errlen, msec_timeout, real_errno, p_offset = 0, zff_len, delimiter_len;
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
				ioerror_specified = FALSE,
				delay_specified = FALSE,
				nodelay_specified = FALSE,
				ibfsize_specified = FALSE,
				is_principal = FALSE;	/* called from inetd */
	unsigned char 		delimiter_buffer[MAX_N_DELIMITER * (MAX_DELIM_LEN + 1)], zff_buffer[MAX_ZFF_LEN];
	char			ioerror, ip[3], tcp[4],
				sock_handle[MAX_HANDLE_LEN], delimiter[MAX_DELIM_LEN + 1];
	error_def(ERR_DELIMSIZNA);
	error_def(ERR_ADDRTOOLONG);
	error_def(ERR_SOCKETEXIST);
	error_def(ERR_ABNCOMPTINC);
	error_def(ERR_DEVPARINAP);
	error_def(ERR_ILLESOCKBFSIZE);
	error_def(ERR_ZFF2MANY);
	error_def(ERR_DELIMWIDTH);

	ioptr = dev->iod;
	assert((params) *(pp->str.addr + p_offset) < (unsigned char)n_iops);
	assert(ioptr != 0);
	assert(ioptr->state >= 0 && ioptr->state < n_io_dev_states);
	assert(ioptr->type == gtmsocket);
	if ((ioptr->state == dev_closed) && mspace && mspace->str.len && mspace->str.addr)
	{
		lower_to_upper((uchar_ptr_t)dev_type, (uchar_ptr_t)mspace->str.addr, mspace->str.len);
		if (STR_LIT_LEN("SOCKET") != mspace->str.len || 0 != memcmp(dev_type, "SOCKET", STR_LIT_LEN("SOCKET")))
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
		if (!io_std_device.in)
		{	/* called from io_init */
			is_principal = TRUE;
		}
	}
	ioptr->dollar.zeof = FALSE;
	newdsocket = *dsocketptr;
	memcpy(newdsocket.dollar_device, "0", sizeof("0"));
	zff_len = -1; /* indicates neither ZFF nor ZNOFF specified */
	delimiter_len = -1; /* indicates neither DELIM nor NODELIM specified */
	while (iop_eol != (ch = *(pp->str.addr + p_offset++)))
	{
		switch  (ch)
		{
		case	iop_delimiter:
			delimiter_len = (int4)(unsigned char)*(pp->str.addr + p_offset);
			if (((MAX_DELIM_LEN + 1) * MAX_N_DELIMITER) >= delimiter_len)
				memcpy(delimiter_buffer, (pp->str.addr + p_offset + 1), delimiter_len);
			else
				rts_error(VARLSTCNT(1) ERR_DELIMSIZNA);
			break;
		case	iop_nodelimiter:
			delimiter_len = 0;
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
                        } else
				rts_error(VARLSTCNT(6) ERR_ADDRTOOLONG, 4, len, pp->str.addr + p_offset + 1, len, SA_MAXLITLEN);
                        break;
		case	iop_connect:
			connect_specified = TRUE;
			len = *(pp->str.addr + p_offset);
			if (len < SA_MAXLITLEN)
			{
				memset(sockaddr, 0, sizeof(sockaddr));
                                memcpy(sockaddr, pp->str.addr + p_offset + 1, len);
			} else
				rts_error(VARLSTCNT(6) ERR_ADDRTOOLONG, 4, len, pp->str.addr + p_offset + 1, len, SA_MAXLITLEN);
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
				handle_len = MAX_HANDLE_LEN;
			memcpy(sock_handle, pp->str.addr + p_offset + 1, handle_len);
			break;
		case	iop_socket:
			rts_error(VARLSTCNT(1) ERR_DEVPARINAP);
			break;
		case	iop_zff:
			if (MAX_ZFF_LEN >= (zff_len = (int4)(unsigned char)*(pp->str.addr + p_offset)))
				memcpy(zff_buffer, (char *)(pp->str.addr + p_offset + 1), zff_len);
			else
				rts_error(VARLSTCNT(4) ERR_ZFF2MANY, 2, zff_len, MAX_ZFF_LEN);
			break;
		case	iop_znoff:
			zff_len = 0;
			break;
		case iop_wrap:
			ioptr->wrap = TRUE;
			break;
		case iop_nowrap:
			ioptr->wrap = FALSE;
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
				(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
        if (listen_specified && connect_specified)
        {
		rts_error(VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("CONNECT"), LEN_AND_LIT("ZLISTEN"), LEN_AND_LIT("OPEN"));
                return FALSE;
        }
	if (delay_specified && nodelay_specified)
	{
		rts_error(VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("DELAY"), LEN_AND_LIT("NODELAY"), LEN_AND_LIT("OPEN"));
		return FALSE;
	}
	if (listen_specified || connect_specified || is_principal)
	{
		if (NULL == (socketptr = iosocket_create(sockaddr, bfsize, is_principal ? file_des : -1)))
			return FALSE;
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
		} else
			iosocket_handle(sock_handle, &handle_len, TRUE, dsocketptr);
		socketptr->handle_len = handle_len;
		memcpy(socketptr->handle, sock_handle, handle_len);
                /* parse the delimiter: delimiter_buffer ==> socketptr->delimiter[...] */
                if (0 <= delimiter_len)
                        iosocket_delimiter(delimiter_buffer, delimiter_len, socketptr, (0 == delimiter_len));
		if (ioptr->wrap && 0 != socketptr->n_delimiter && ioptr->width < socketptr->delimiter[0].len)
			rts_error(VARLSTCNT(4) ERR_DELIMWIDTH, 2, ioptr->width, socketptr->delimiter[0].len);
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
		iosocket_delimiter((unsigned char *)NULL, 0, socketptr, TRUE);
		free(socketptr);
		return FALSE;
	}
	else if (is_principal)
	{	/* fill in what bind or connect would */
		strncpy(socketptr->local.saddr_ip, tcp_routines.aa_inet_ntoa(socketptr->local.sin.sin_addr),
			sizeof(socketptr->local.saddr_ip));
		strncpy(socketptr->remote.saddr_ip, tcp_routines.aa_inet_ntoa(socketptr->remote.sin.sin_addr),
			sizeof(socketptr->remote.saddr_ip));
		len = sizeof(ESTABLISHED) - 1;
		memcpy(&newdsocket.dollar_key[0], ESTABLISHED, len);
		newdsocket.dollar_key[len++] = '|';
		memcpy(&newdsocket.dollar_key[len], socketptr->handle, socketptr->handle_len);
		len += socketptr->handle_len;
		newdsocket.dollar_key[len++] = '|';
		memcpy(&newdsocket.dollar_key[len], socketptr->remote.saddr_ip, strlen(socketptr->remote.saddr_ip));
		len += strlen(socketptr->remote.saddr_ip);
		newdsocket.dollar_key[len++] = '\0';
	}
	/* commit the changes to the list */
	if (listen_specified || connect_specified || is_principal)
	{
		socketptr->dev = dsocketptr;
		*dsocketptr = newdsocket;
	}
	if (0 <= zff_len && /* ZFF or ZNOFF specified */
	    0 < (socketptr->zff.len = zff_len)) /* assign the new ZFF len, might be 0 from ZNOFF, or ZFF="" */
	{ /* ZFF="non-zero-len-string" specified */
		if (NULL == socketptr->zff.addr) /* we rely on socketptr->zff.addr being set to 0 in iosocket_create() */
			socketptr->zff.addr = (char *)malloc(MAX_ZFF_LEN);
		memcpy(socketptr->zff.addr, zff_buffer, zff_len);
	}
       	ioptr->state = dev_open;
	return TRUE;
}
