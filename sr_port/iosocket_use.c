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

/* iosocket_use.c */
#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_unistd.h"
#include "gtm_iconv.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include <errno.h>
#include <netinet/in.h>
#ifndef __MVS__
#include <netinet/tcp.h>
#endif
#include "copy.h"
#include "io.h"
#include "io_params.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "nametabtyp.h"
#include "namelook.h"
#include "stringpool.h"

GBLREF 	io_pair          	io_curr_device;
GBLREF  io_pair			io_std_device;
GBLREF 	io_desc          	*active_device;
GBLREF	tcp_library_struct	tcp_routines;
GBLREF	d_socket_struct		*socket_pool;
LITREF	nametabent		filter_names[];
LITREF	unsigned char		filter_index[27];
LITREF 	unsigned char		io_params_size[];

void	iosocket_use(io_desc *iod, mval *pp)
{
	unsigned char	ch, len;
	short		handled_len, handlea_len, handles_len;
	int4		length, width;
	d_socket_struct *dsocketptr, newdsocket;
	socket_struct	*socketptr, newsocket;
	char		handlea[MAX_HANDLE_LEN], handles[MAX_HANDLE_LEN], handled[MAX_HANDLE_LEN];
 	char            addr[SA_MAXLITLEN], *errptr, sockaddr[SA_MAXLITLEN],
     			temp_addr[SA_MAXLITLEN], ioerror;
	unsigned char	delimiter_buffer[MAX_N_DELIMITER * (MAX_DELIM_LEN + 1)];
	unsigned char	zff_buffer[MAX_ZFF_LEN];
	boolean_t	attach_specified = FALSE,
			detach_specified = FALSE,
			connect_specified = FALSE,
			ioerror_specified = FALSE,
			listen_specified = FALSE,
			socket_specified = FALSE,
			delay_specified = FALSE,
			nodelay_specified = FALSE,
			bfsize_specified = FALSE,
			ibfsize_specified = FALSE,
			create_new_socket;
	int4 		index, n_specified, zff_len, delimiter_len;
	int		fil_type, nodelay, p_offset = 0;
	uint4		bfsize = DEFAULT_SOCKET_BUFFER_SIZE, ibfsize;
	mident		tab;
	int		save_errno;

	error_def(ERR_ABNCOMPTINC);
        error_def(ERR_ADDRTOOLONG);
	error_def(ERR_ANCOMPTINC);
	error_def(ERR_ACOMPTBINC);
	error_def(ERR_CURRSOCKOFR);
	error_def(ERR_DELIMSIZNA);
	error_def(ERR_ILLESOCKBFSIZE);
	error_def(ERR_SETSOCKOPTERR);
	error_def(ERR_SOCKBFNOTEMPTY);
	error_def(ERR_SOCKNOTFND);
	error_def(ERR_TEXT);
	error_def(ERR_TTINVFILTER);
	error_def(ERR_ZFF2MANY);
	error_def(ERR_DEVPARMNEG);
	error_def(ERR_DELIMWIDTH);

        assert(iod->state == dev_open);
        assert(iod->type == gtmsocket);
	/* ---------------------------------- parse the command line ------------------------------------ */
	n_specified = 0;
	zff_len = -1; /* indicates neither ZFF nor ZNOFF specified */
	delimiter_len = -1; /* indicates neither DELIM nor NODELIM specified */
	while (iop_eol != (ch = *(pp->str.addr + p_offset++)))
	{
		assert((params)ch < (params)n_iops);
		switch (ch)
		{
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&iod->error_handler);
			break;
		case iop_filter:
			memset(&tab, 0, sizeof(mident));
			len = *(pp->str.addr + p_offset);
			memcpy(&tab.c[0], pp->str.addr + p_offset + 1, (len < sizeof(mident) ? len : sizeof(mident)));
			if ((fil_type = namelook(filter_index, filter_names, tab.c)) < 0)
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
		case iop_attach:
			n_specified++;
			attach_specified = TRUE;
			handlea_len = (short)(*(pp->str.addr + p_offset));
			memcpy(handlea, (char *)(pp->str.addr + p_offset + 1), handlea_len);
			break;
		case iop_detach:
			n_specified++;
			detach_specified = TRUE;
			handled_len = (short)(*(pp->str.addr + p_offset));
			memcpy(handled, (char *)(pp->str.addr + p_offset + 1), handled_len);
			break;
		case iop_connect:
			n_specified++;
			connect_specified = TRUE;
			len = *(pp->str.addr + p_offset);
                        if (len < SA_MAXLITLEN)
                        {
				memcpy(sockaddr, (char *)(pp->str.addr + p_offset + 1), len);
				sockaddr[len] = '\0';
                        } else
				rts_error(VARLSTCNT(6) ERR_ADDRTOOLONG, 4, len, pp->str.addr + p_offset + 1, len, SA_MAXLITLEN);
                        break;
		case iop_delimiter:
			n_specified++;
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
			bfsize_specified = TRUE;
			GET_ULONG(bfsize, pp->str.addr + p_offset);
			if ((0 == bfsize) || (MAX_SOCKET_BUFFER_SIZE < bfsize))
				rts_error(VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, bfsize);
			break;
		case	iop_zibfsize:
			ibfsize_specified = TRUE;
			GET_ULONG(ibfsize, pp->str.addr + p_offset);
			if ((0 == ibfsize) || (MAX_INTERNAL_SOCBUF_SIZE < ibfsize))
				rts_error(VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, ibfsize);
			break;
		case iop_ioerror:
			n_specified++;
			ioerror_specified = TRUE;
			ioerror = *(char *)(pp->str.addr + p_offset + 1);
			break;
		case iop_zlisten:
			n_specified++;
			listen_specified = TRUE;
			len = *(pp->str.addr + p_offset);
                        if (len < SA_MAXLITLEN)
                        {
				memcpy(sockaddr, (char *)(pp->str.addr + p_offset + 1), len);
				sockaddr[len] = '\0';
                        } else
				rts_error(VARLSTCNT(6) ERR_ADDRTOOLONG, 4, len, pp->str.addr + p_offset + 1, len, SA_MAXLITLEN);
                        break;
		case iop_socket:
			n_specified++;
			socket_specified = TRUE;
			handles_len = (short)(*(pp->str.addr + p_offset));
			memcpy(handles, (char *)(pp->str.addr + p_offset + 1), handles_len);
			break;
		case iop_ipchset:
			if ((iconv_t)0 != iod->input_conv_cd)
				ICONV_CLOSE_CD(iod->input_conv_cd);
			SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
			if (DEFAULT_CODE_SET != iod->in_code_set)
				ICONV_OPEN_CD(iod->input_conv_cd, INSIDE_CH_SET, (char *)(pp->str.addr + p_offset + 1));
                       	break;
                case iop_opchset:
			if ((iconv_t)0 != iod->output_conv_cd)
				ICONV_CLOSE_CD(iod->output_conv_cd);
			SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
			if (DEFAULT_CODE_SET != iod->out_code_set)
				ICONV_OPEN_CD(iod->output_conv_cd, (char *)(pp->str.addr + p_offset + 1), INSIDE_CH_SET);
                       	break;
		case iop_zff:
			if (MAX_ZFF_LEN >= (zff_len = (int4)(unsigned char)*(pp->str.addr + p_offset)))
				memcpy(zff_buffer, (char *)(pp->str.addr + p_offset + 1), zff_len);
			else
				rts_error(VARLSTCNT(4) ERR_ZFF2MANY, 2, zff_len, MAX_ZFF_LEN);
			break;
		case iop_znoff:
			zff_len = 0;
			break;
		case iop_length:
			GET_LONG(length, pp->str.addr + p_offset);
			if (length < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			iod->length = length;
			break;
		case iop_width:
			/* SOCKET WIDTH is handled the same way as TERMINAL WIDTH */
			GET_LONG(width, pp->str.addr + p_offset);
			if (width < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			if (0 == width)
			{
				iod->width = TCPDEF_WIDTH;
				iod->wrap = FALSE;
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
			/* ignore deviceparm */
			break;
		}
		p_offset += ((io_params_size[ch] == IOP_VAR_SIZE) ?
				(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
	/* ------ return immediately if no flag, worth a check because it is mostly true ------------ */
	if (1 == p_offset)
		return;
	/* ------------------------------ compatibility verification -------------------------------- */
	if ((socket_specified) && ((n_specified > 2) || ((2 == n_specified) && (0 >= delimiter_len))))
	{
		rts_error(VARLSTCNT(8) ERR_ACOMPTBINC, 6, LEN_AND_LIT("SOCKET"), LEN_AND_LIT("DELIMITER"), LEN_AND_LIT("USE"));
		return;
	}
	if (connect_specified && listen_specified)
	{
		rts_error(VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("CONNECT"), LEN_AND_LIT("ZLISTEN"), LEN_AND_LIT("USE"));
		return;
	}
	if (delay_specified && nodelay_specified)
	{
		rts_error(VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("DELAY"), LEN_AND_LIT("NODELAY"), LEN_AND_LIT("OPEN"));
		return;
	}
	/* ------------------ make a local copy of device structure to play with -------------------- */
        dsocketptr = (d_socket_struct *)(iod->dev_sp);
	newdsocket = *dsocketptr;
	/* --------------- handle the two special cases attach/detach first ------------------------- */
	if (detach_specified)
	{
		if (1 < n_specified)
		{
			rts_error(VARLSTCNT(6) ERR_ANCOMPTINC, 4, LEN_AND_LIT("DETACH"), LEN_AND_LIT("USE"));
			return;
		}
		if (NULL == socket_pool)
			iosocket_poolinit();
		iosocket_switch(handled, handled_len, &newdsocket, socket_pool);
		*dsocketptr = newdsocket;
		if (0 > dsocketptr->current_socket)
		{
			io_curr_device.in = io_std_device.in;
			io_curr_device.out = io_std_device.out;
		}
		return; /* detach can only be specified by itself */
	}
	if (attach_specified)
	{
		if (1 < n_specified)
		{
			rts_error(VARLSTCNT(6) ERR_ANCOMPTINC, 4, LEN_AND_LIT("ATTACH"), LEN_AND_LIT("USE"));
			return;
		}
		if (NULL == socket_pool)
		{
			rts_error(VARLSTCNT(4) ERR_SOCKNOTFND, 2, handlea_len, handlea);
			return;
		}
		iosocket_switch(handlea, handlea_len, socket_pool, &newdsocket);
		*dsocketptr = newdsocket;
		return; /* attach can only be specified by itself */
	}
	/* ------------ create/identify the socket to work on and make a local copy ----------------- */
	if (create_new_socket = (listen_specified || connect_specified))	/* real "=" */
	{
		/* allocate the structure for a new socket */
                if (NULL == (socketptr = iosocket_create(sockaddr, bfsize)))
                        return;
		/* give the new socket a handle */
		iosocket_handle(handles, &handles_len, TRUE, dsocketptr);
                socketptr->handle_len = handles_len;
                memcpy(socketptr->handle, handles, handles_len);
		socketptr->dev = &newdsocket;	/* use newdsocket temporarily for the sake of bind/connect */
	} else
	{
		if (socket_specified)
		{
			/* use the socket flag to identify which socket to apply changes */
			if (0 > (index = iosocket_handle(handles, &handles_len, FALSE, &newdsocket)))
			{
				rts_error(VARLSTCNT(4) ERR_SOCKNOTFND, 2, handles_len, handles);
				return;
			}
			newdsocket.current_socket = index;
			socketptr = newdsocket.socket[index];
		} else
		{
			socketptr = newdsocket.socket[newdsocket.current_socket];
     			if (newdsocket.n_socket <= newdsocket.current_socket)
	     		{
				assert(FALSE);
				rts_error(VARLSTCNT(4) ERR_CURRSOCKOFR, 2, newdsocket.current_socket, newdsocket.n_socket);
     				return;
     			}
		}
	}
	newsocket = *socketptr;
	/* ---------------------- apply changes to the local copy of the socket --------------------- */
	if (0 <= delimiter_len)
		iosocket_delimiter(delimiter_buffer, delimiter_len, &newsocket, (0 == delimiter_len));
	if (iod->wrap && 0 != newsocket.n_delimiter && iod->width < newsocket.delimiter[0].len)
		rts_error(VARLSTCNT(4) ERR_DELIMWIDTH, 2, iod->width, newsocket.delimiter[0].len);
	if (0 <= zff_len && /* ZFF or ZNOFF specified */
	    0 < (newsocket.zff.len = zff_len)) /* assign the new ZFF len, might be 0 from ZNOFF, or ZFF="" */
	{ /* ZFF="non-zero-len-string" specified */
		if (NULL == newsocket.zff.addr) /* we rely on newsocket.zff.addr being set to 0 in iosocket_create() */
			newsocket.zff.addr = (char *)malloc(MAX_ZFF_LEN);
		memcpy(newsocket.zff.addr, zff_buffer, zff_len);
	}
	if (ioerror_specified)
		newsocket.ioerror = ('T' == ioerror || 't' == ioerror);
	if (nodelay_specified || delay_specified)
		newsocket.nodelay = nodelay_specified;	/* defaults to DELAY */
	if (ibfsize_specified)
		newsocket.bufsiz = ibfsize;
	if (!create_new_socket)
	{
		/* these changes apply to only pre-existing sockets */
		if (bfsize_specified)
			newsocket.buffer_size = bfsize;
#ifdef TCP_NODELAY
		nodelay = newsocket.nodelay ? 1 : 0;
		if ((socketptr->nodelay != newsocket.nodelay) &&
				(-1 == tcp_routines.aa_setsockopt(newsocket.sd, IPPROTO_TCP,
						TCP_NODELAY, &nodelay, sizeof(nodelay))))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
                	rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("TCP_NODELAY"), save_errno, LEN_AND_STR(errptr));
			return;
		}
#endif
		if ((socketptr->bufsiz != newsocket.bufsiz) &&
				(-1 == tcp_routines.aa_setsockopt(newsocket.sd, SOL_SOCKET,
						SO_RCVBUF, &newsocket.bufsiz, sizeof(newsocket.bufsiz))))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
                	rts_error(VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("SO_RCVBUF"), save_errno, LEN_AND_STR(errptr));
			return;
		}
		if (socketptr->buffer_size != newsocket.buffer_size)
		{
			if (socketptr->buffered_length > bfsize)
				rts_error(VARLSTCNT(4) ERR_SOCKBFNOTEMPTY, 2, bfsize, socketptr->buffered_length);
			newsocket.buffer = (char *)malloc(bfsize);
			if (0 < socketptr->buffered_length)
			{
				memcpy(newsocket.buffer, socketptr->buffer + socketptr->buffered_offset,
						socketptr->buffered_length);
				newsocket.buffered_offset = 0;
			}
		}
	}
        /* -------------------------------------- action -------------------------------------------- */
        if ((listen_specified && (!iosocket_bind(&newsocket, NO_M_TIMEOUT, ibfsize_specified))) ||
                (connect_specified && (!iosocket_connect(&newsocket, 0, ibfsize_specified))))
        {	/* error message should be printed from bind/connect */
                if (socketptr->sd > 0)
                        (void)tcp_routines.aa_close(socketptr->sd);
                iosocket_delimiter((unsigned char *)NULL, 0, &newsocket, TRUE);
                free(socketptr);
                return;
        }
	/* ------------------------------------ commit changes -------------------------------------- */
	if (create_new_socket)
	{
		/* a new socket is created. so add to the list */
		newsocket.dev = dsocketptr;
                newdsocket.socket[newdsocket.n_socket++] = socketptr;
                newdsocket.current_socket = newdsocket.n_socket - 1;
	}
	else if (socketptr->buffer_size != newsocket.buffer_size)
		free(socketptr->buffer);
	*socketptr = newsocket;
	*dsocketptr = newdsocket;
	return;
}
