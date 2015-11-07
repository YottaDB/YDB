/****************************************************************
 *								*
 *	Copyright 2012, 2013 Fidelity Information Services, Inc	*
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
#include "gtm_conv.h"
#include "gtm_utf8.h"
#include "gtm_netdb.h"

GBLREF 	tcp_library_struct	tcp_routines;
GBLREF	d_socket_struct		*socket_pool, *newdsocket;
GBLREF	io_pair			io_std_device;	/* standard device */
GBLREF	boolean_t		gtm_utf8_mode;
GBLREF	int4			gtm_max_sockets;
GBLREF	boolean_t		dollar_zininterrupt;
LITREF 	unsigned char		io_params_size[];
LITREF	mstr			chset_names[];

error_def(ERR_ABNCOMPTINC);
error_def(ERR_ADDRTOOLONG);
error_def(ERR_DELIMSIZNA);
error_def(ERR_DELIMWIDTH);
error_def(ERR_DEVPARINAP);
error_def(ERR_DEVPARMNEG);
error_def(ERR_GETNAMEINFO);
error_def(ERR_ILLESOCKBFSIZE);
error_def(ERR_MRTMAXEXCEEDED);
error_def(ERR_SOCKETEXIST);
error_def(ERR_SOCKMAX);
error_def(ERR_TEXT);
error_def(ERR_ZFF2MANY);
error_def(ERR_ZINTRECURSEIO);

#define ESTABLISHED		"ESTABLISHED"

short	iosocket_open(io_log_name *dev, mval *pp, int file_des, mval *mspace, int4 timepar)
{
	char			addr[SA_MAXLITLEN], *errptr, sockaddr[SA_MAXLITLEN],
		                temp_addr[SA_MAXLITLEN], dev_type[MAX_DEV_TYPE_LEN];
	unsigned char		ch, *c, *next, *top;
	int			handle_len, moreread_timeout, len;
	unsigned short		port;
	int4			errlen, msec_timeout, real_errno, p_offset = 0, zff_len, delimiter_len;
	int			d_socket_struct_len;
	ABS_TIME		cur_time, end_time;
	io_desc			*ioptr;
	fd_set			tcp_fd;
	uint4			bfsize = DEFAULT_SOCKET_BUFFER_SIZE, ibfsize;
        d_socket_struct         *dsocketptr;
	socket_struct		*socketptr;
	mv_stent		*mv_zintdev;
	boolean_t		zint_conn_restart = FALSE;
	socket_interrupt	*sockintr;
	mstr			chset_mstr;
	boolean_t		attach_specified = FALSE,
		                listen_specified = FALSE,
		                connect_specified = FALSE,
	                	ioerror_specified = FALSE,
		                delay_specified = FALSE,
		                nodelay_specified = FALSE,
		                ibfsize_specified = FALSE,
		                moreread_specified = FALSE,
		                is_principal = FALSE,	/* called from inetd */
		                ichset_specified,
		                ochset_specified;
	unsigned char 		delimiter_buffer[MAX_N_DELIMITER * (MAX_DELIM_LEN + 1)], zff_buffer[MAX_ZFF_LEN];
	char			ioerror, ip[3], tcp[4],
 		                sock_handle[MAX_HANDLE_LEN], delimiter[MAX_DELIM_LEN + 1];
	int			socketptr_delim_len;
	char			ipaddr[SA_MAXLEN];
	int			errcode;
	struct addrinfo		*ai_ptr, *remote_ai_ptr;

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
	d_socket_struct_len = SIZEOF(d_socket_struct) + (SIZEOF(socket_struct) * (gtm_max_sockets - 1));
	if (ioptr->state == dev_never_opened)
	{
		dsocketptr = ioptr->dev_sp = (void *)malloc(d_socket_struct_len);
		ioptr->newly_created = TRUE;
		memset(dsocketptr, 0, d_socket_struct_len);
		dsocketptr->iod = ioptr;
	} else
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
			/* called from io_init */
			is_principal = TRUE;
	}
        if (dsocketptr->mupintr)
	{	/* check if connect was interrupted */
		sockintr = &dsocketptr->sock_save_state;
		if (sockwhich_invalid == sockintr->who_saved)
			GTMASSERT;	/* Interrupt should never have an invalid save state */
		if (dollar_zininterrupt)
		{
			dsocketptr->mupintr = FALSE;
			sockintr->who_saved = sockwhich_invalid;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
		}
		if (sockwhich_connect != sockintr->who_saved)
			GTMASSERT;	/* ZINTRECURSEIO should have caught */
		mv_zintdev = io_find_mvstent(dsocketptr->iod, FALSE);
		if (mv_zintdev && mv_zintdev->mv_st_cont.mvs_zintdev.buffer_valid)
		{	/* mupintr will be reset and mvstent popped in iosocket_connect */
			connect_specified = TRUE;
			ibfsize_specified = sockintr->ibfsize_specified;
			assert(newdsocket);
			assert(newdsocket == sockintr->newdsocket);
			memcpy(newdsocket, (d_socket_struct *)mv_zintdev->mv_st_cont.mvs_zintdev.curr_sp_buffer.addr,
					d_socket_struct_len);
			socketptr = newdsocket->socket[newdsocket->current_socket];
			assert(socketptr == (socket_struct *)mv_zintdev->mv_st_cont.mvs_zintdev.socketptr);
			zint_conn_restart = TRUE;	/* skip what we already did, state == dev_closed */
		}
	} else
	{
		ioptr->dollar.zeof = FALSE;
		if (NULL == newdsocket)
			newdsocket = (d_socket_struct *)malloc(d_socket_struct_len);
		memcpy(newdsocket, dsocketptr, d_socket_struct_len);
		memcpy(ioptr->dollar.device, "0", SIZEOF("0"));
		zff_len = -1; /* indicates neither ZFF nor ZNOFF specified */
		delimiter_len = -1; /* indicates neither DELIM nor NODELIM specified */
		ichset_specified = ochset_specified = FALSE;
		while (iop_eol != (ch = *(pp->str.addr + p_offset++)))
		{
			switch(ch)
			{
				case iop_delimiter:
					delimiter_len = (int4)(unsigned char)*(pp->str.addr + p_offset);
					if (((MAX_DELIM_LEN + 1) * MAX_N_DELIMITER) >= delimiter_len)
						memcpy(delimiter_buffer, (pp->str.addr + p_offset + 1), delimiter_len);
					else
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DELIMSIZNA);
					break;
				case iop_ipchset:
					UNICODE_ONLY(
						if (gtm_utf8_mode)
					        {	/* Only change ipchset if in UTF8 mode */
							chset_mstr.addr = (char *)(pp->str.addr + p_offset + 1);
							chset_mstr.len = *(pp->str.addr + p_offset);
							SET_ENCODING(ioptr->ichset, &chset_mstr);
							ichset_specified = TRUE;
						}
					);
					break;
				case iop_opchset:
					UNICODE_ONLY(
						if (gtm_utf8_mode)
		 				{       /* Only change ipchset if in UTF8 mode */
							chset_mstr.addr = (char *)(pp->str.addr + p_offset + 1);
							chset_mstr.len = *(pp->str.addr + p_offset);
							SET_ENCODING(ioptr->ochset, &chset_mstr);
							ochset_specified = TRUE;
						}
					);
					break;
                        	case iop_chset:
					UNICODE_ONLY(
						if (gtm_utf8_mode)
						{       /* Only change ipchset/opchset if in UTF8 mode */
							chset_mstr.addr = (char *)(pp->str.addr + p_offset + 1);
							chset_mstr.len = *(pp->str.addr + p_offset);
							SET_ENCODING(ioptr->ichset, &chset_mstr);
							ioptr->ochset = ioptr->ichset;
							ichset_specified = ochset_specified = TRUE;
                                        	}
					);
					break;
			/* Note the following 4 cases (iop_m/utf16/utf16be/utf16le) have no corresponding device parameter
			   but are included here because they can be easily used in internal processing.
			*/
				case iop_m:
					UNICODE_ONLY(
						ioptr->ichset = ioptr->ochset = CHSET_M;
						ichset_specified = ochset_specified = TRUE;
					);
					break;
				case iop_utf16:
					UNICODE_ONLY(
						if (gtm_utf8_mode)
						{       /* Only change chset if in UTF8 mode */
							ioptr->ichset = ioptr->ochset = CHSET_UTF16;
							ichset_specified = ochset_specified = TRUE;
						}
					);
					break;
				case iop_utf16be:
					UNICODE_ONLY(
						if (gtm_utf8_mode)
						{       /* Only change chset if in UTF8 mode */
							ioptr->ichset = ioptr->ochset = CHSET_UTF16BE;
							ichset_specified = ochset_specified = TRUE;
						}
					);
					break;
				case iop_utf16le:
					UNICODE_ONLY(
						if (gtm_utf8_mode)
						{       /* Only change chset if in UTF8 mode */
							ioptr->ichset = ioptr->ochset = CHSET_UTF16LE;
							ichset_specified = ochset_specified = TRUE;
						}
					);
					break;
			/**********************************/
				case iop_nodelimiter:
					delimiter_len = 0;
					break;
				case iop_zdelay:
					delay_specified = TRUE;
					break;
				case iop_znodelay:
					nodelay_specified = TRUE;
					break;
				case iop_zbfsize:
					GET_ULONG(bfsize, pp->str.addr + p_offset);
					if ((0 == bfsize) || (MAX_SOCKET_BUFFER_SIZE < bfsize))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, bfsize);
					break;
				case iop_zibfsize:
					ibfsize_specified = TRUE;
					GET_ULONG(ibfsize, pp->str.addr + p_offset);
					if ((0 == ibfsize) || (MAX_INTERNAL_SOCBUF_SIZE < ibfsize))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, bfsize);
					break;
				case iop_zlisten:
					listen_specified = TRUE;
					len = (int)(*(pp->str.addr + p_offset));
					if (len < SA_MAXLITLEN)
					{
						memset(sockaddr, 0, SIZEOF(sockaddr));
						memcpy(sockaddr, pp->str.addr + p_offset + 1, len);
					} else
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ADDRTOOLONG, 4, len,
								 pp->str.addr + p_offset + 1, len, SA_MAXLITLEN);
					break;
				case iop_connect:
					connect_specified = TRUE;
					len = (int)(*(pp->str.addr + p_offset));
					if (len < SA_MAXLITLEN)
					{
						memset(sockaddr, 0, SIZEOF(sockaddr));
						memcpy(sockaddr, pp->str.addr + p_offset + 1, len);
					} else
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ADDRTOOLONG, 4,
								 len, pp->str.addr + p_offset + 1, len, SA_MAXLITLEN);
					break;
				case iop_ioerror:
					ioerror_specified = TRUE;
					ioerror = *(pp->str.addr + p_offset + 1);	/* the first char decides */
					break;
				case iop_exception:
					ioptr->error_handler.len = *(pp->str.addr + p_offset);
					ioptr->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
					s2pool(&ioptr->error_handler);
					break;
				case iop_attach:
					attach_specified = TRUE;
					handle_len = (int)(*(pp->str.addr + p_offset));
					if (handle_len > MAX_HANDLE_LEN)
						handle_len = MAX_HANDLE_LEN;
					memcpy(sock_handle, pp->str.addr + p_offset + 1, handle_len);
					break;
				case iop_socket:
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARINAP);
					break;
				case iop_zff:
					if (MAX_ZFF_LEN >= (zff_len = (int4)(unsigned char)*(pp->str.addr + p_offset)))
						memcpy(zff_buffer, (char *)(pp->str.addr + p_offset + 1), zff_len);
					else
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZFF2MANY, 2, zff_len, MAX_ZFF_LEN);
					break;
				case iop_znoff:
					zff_len = 0;
					break;
				case iop_wrap:
					ioptr->wrap = TRUE;
					break;
				case iop_nowrap:
					ioptr->wrap = FALSE;
					break;
				case iop_morereadtime:
					/* Time in milliseconds socket read will wait for more data before returning */
					GET_LONG(moreread_timeout, pp->str.addr + p_offset);
					if (-1 == moreread_timeout)
						moreread_timeout = DEFAULT_MOREREAD_TIMEOUT;
					else if (-1 > moreread_timeout)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARMNEG);
					else if (MAX_MOREREAD_TIMEOUT < moreread_timeout)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_MRTMAXEXCEEDED, 1,
								 MAX_MOREREAD_TIMEOUT);
					moreread_specified = TRUE;
					break;
				default:
					break;
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
				     (unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
		}
		if (!ichset_specified)
			ioptr->ichset = (gtm_utf8_mode) ? CHSET_UTF8 : CHSET_M;
		if (!ochset_specified)
			ioptr->ochset = (gtm_utf8_mode) ? CHSET_UTF8 : CHSET_M;
		if (CHSET_M != ioptr->ichset && CHSET_UTF16 != ioptr->ichset)
			get_chset_desc(&chset_names[ioptr->ichset]);
		if (CHSET_M != ioptr->ochset && CHSET_UTF16 != ioptr->ochset)
			get_chset_desc(&chset_names[ioptr->ochset]);
		if (listen_specified && connect_specified)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("CONNECT"),
					LEN_AND_LIT("ZLISTEN"), LEN_AND_LIT("OPEN"));
		return FALSE;
		}
		if (delay_specified && nodelay_specified)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("DELAY"),
					LEN_AND_LIT("NODELAY"), LEN_AND_LIT("OPEN"));
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
			if (moreread_specified)
			{
				socketptr->moreread_timeout = moreread_timeout;
				socketptr->def_moreread_timeout = TRUE; /* iosocket_readfl.c needs to know user specified */
			}
               	 /* socket handle -- also check for duplication */
			if (attach_specified)
			{
				if (iosocket_handle(sock_handle, &handle_len, FALSE, newdsocket) >= 0)
				{
					if (FD_INVALID != socketptr->temp_sd)
						tcp_routines.aa_close(socketptr->temp_sd);
					SOCKET_FREE(socketptr);
					assert(ioptr->newly_created == FALSE);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKETEXIST, 2, handle_len, sock_handle);
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
			{
				socketptr_delim_len = socketptr->delimiter[0].len;
				SOCKET_FREE(socketptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DELIMWIDTH, 2, ioptr->width, socketptr_delim_len);
				assert(FALSE);
			}
			/* connects newdsocket and socketptr (the new socket) */
			if (gtm_max_sockets <= newdsocket->n_socket)
			{
				assert(ioptr->newly_created == FALSE);
				if (FD_INVALID != socketptr->temp_sd)
					tcp_routines.aa_close(socketptr->temp_sd);
				SOCKET_FREE(socketptr);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SOCKMAX, 1, gtm_max_sockets);
				return FALSE;
			}
			socketptr->dev = newdsocket;
			newdsocket->socket[newdsocket->n_socket++] = socketptr;
			newdsocket->current_socket = newdsocket->n_socket - 1;
		}
		if (0 <= zff_len && /* ZFF or ZNOFF specified */
		    0 < (socketptr->zff.len = zff_len)) /* assign the new ZFF len, might be 0 from ZNOFF, or ZFF="" */
		{ /* ZFF="non-zero-len-string" specified */
			if (gtm_utf8_mode) /* Check if ZFF has any invalid UTF-8 character */
			{ /* Note: the ZFF string originates from the source program, so is in UTF-8 mode or M mode regardless
		   	   * of OCHSET of this device. ZFF is output on WRITE # command, and MUST contain valid UTF-8 sequence. */
				utf8_len_strict(zff_buffer, zff_len); /* triggers badchar error for invalid sequence */
			}
			if (NULL == socketptr->zff.addr) /* we rely on socketptr->zff.addr being set to 0 in iosocket_create() */
				socketptr->zff.addr = (char *)malloc(MAX_ZFF_LEN);
			memcpy(socketptr->zff.addr, zff_buffer, zff_len);
		}
	}
	/* action */
	if ((listen_specified && (!iosocket_bind(socketptr, timepar, ibfsize_specified))) ||
	    (connect_specified && (!iosocket_connect(socketptr, timepar, ibfsize_specified))))
       	{
		if (socketptr->sd > 0)
			(void)tcp_routines.aa_close(socketptr->sd);
		SOCKET_FREE(socketptr);
		return FALSE;
	} else if (is_principal)
	{	/* fill in what bind or connect would */
		ai_ptr = &(socketptr->local.ai);
		remote_ai_ptr = &(socketptr->remote.ai);
		/* translate internal address to numeric ip address */
		GETNAMEINFO(SOCKET_LOCAL_ADDR(socketptr), ai_ptr->ai_addrlen, ipaddr, SIZEOF(ipaddr), NULL, 0, NI_NUMERICHOST,
				errcode);
		if (0 != errcode)
		{
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return FALSE;
		}
		STRNDUP(ipaddr, SIZEOF(ipaddr), socketptr->local.saddr_ip);
		GETNAMEINFO(SOCKET_REMOTE_ADDR(socketptr), remote_ai_ptr->ai_addrlen, ipaddr, SIZEOF(ipaddr), NULL, 0,
				NI_NUMERICHOST, errcode);
		if (0 != errcode)
		{
			RTS_ERROR_ADDRINFO(NULL, ERR_GETNAMEINFO, errcode);
			return FALSE;
		}
		STRNDUP(ipaddr, SIZEOF(ipaddr), socketptr->remote.saddr_ip);
		len = SIZEOF(ESTABLISHED) - 1;
		memcpy(&ioptr->dollar.key[0], ESTABLISHED, len);
		ioptr->dollar.key[len++] = '|';
		memcpy(&ioptr->dollar.key[len], socketptr->handle, socketptr->handle_len);
		len += socketptr->handle_len;
		ioptr->dollar.key[len++] = '|';
		strncpy(&ioptr->dollar.key[len], socketptr->remote.saddr_ip, DD_BUFLEN - 1 - len);
		ioptr->dollar.key[DD_BUFLEN-1] = '\0';			/* In case we fill the buffer */
	}
	/* commit the changes to the list */
	if (listen_specified || connect_specified || is_principal)
	{
		socketptr->dev = dsocketptr;
		memcpy(dsocketptr, newdsocket, d_socket_struct_len);
	}
	ioptr->newly_created = FALSE;
	ioptr->state = dev_open;
	return TRUE;
}
