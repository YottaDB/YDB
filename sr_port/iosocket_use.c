/****************************************************************
 *								*
 * Copyright (c) 2013-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_inet.h"
#include "copy.h"
#include "io.h"
#include "io_params.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "nametabtyp.h"
#include "namelook.h"
#include "stringpool.h"
#include "gtm_conv.h"
#include "error.h"
#include "op.h"
#include "indir_enum.h"

GBLREF 	io_pair          	io_curr_device;
GBLREF  io_pair			io_std_device;
GBLREF 	io_desc          	*active_device;
GBLREF	d_socket_struct		*socket_pool;
GBLREF	boolean_t		gtm_utf8_mode;
GBLREF	spdesc			stringpool;
GBLREF	UConverter  		*chset_desc[];
GBLREF	int4			gtm_max_sockets;
GBLREF	d_socket_struct		*newdsocket;
GBLREF	boolean_t		dollar_zininterrupt;

LITREF	nametabent		filter_names[];
LITREF	unsigned char		filter_index[27];
LITREF 	unsigned char		io_params_size[];
LITREF	mstr			chset_names[];

error_def(ERR_ABNCOMPTINC);
error_def(ERR_ACOMPTBINC);
error_def(ERR_ADDRTOOLONG);
error_def(ERR_CHSETALREADY);
error_def(ERR_ANCOMPTINC);
error_def(ERR_CURRSOCKOFR);
error_def(ERR_DELIMSIZNA);
error_def(ERR_DELIMWIDTH);
error_def(ERR_DEVPARMNEG);
error_def(ERR_ILLESOCKBFSIZE);
error_def(ERR_MRTMAXEXCEEDED);
error_def(ERR_NOSOCKETINDEV);
error_def(ERR_SETSOCKOPTERR);
error_def(ERR_SOCKBFNOTEMPTY);
error_def(ERR_SOCKNOTFND);
error_def(ERR_SOCKMAX);
error_def(ERR_TEXT);
error_def(ERR_TTINVFILTER);
error_def(ERR_ZFF2MANY);
error_def(ERR_ZINTRECURSEIO);

void	iosocket_use(io_desc *iod, mval *pp)
{
	unsigned char	ch, len;
	static char	*conv_buff = NULL;
	int		new_ozff_len, conv_len, handled_len, handlea_len, handles_len, int_len, soc_cnt;
	int4		length, width, new_len;
	d_socket_struct *dsocketptr;
	socket_struct	*socketptr, newsocket, *localsocketptr;
	char		handlea[MAX_HANDLE_LEN], handles[MAX_HANDLE_LEN], handled[MAX_HANDLE_LEN];
	char		addr[SA_MAXLITLEN], *errptr, sockaddr[SA_MAXLITLEN],
			temp_addr[SA_MAXLITLEN], ioerror, *free_ozff = NULL;
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
			ochset_specified = FALSE,
			ichset_specified = FALSE,
			ibfsize_specified = FALSE,
			moreread_specified = FALSE,
			flush_specified = FALSE,
			create_new_socket;
	int4 		index, n_specified, zff_len, delimiter_len, moreread_timeout;
	int		fil_type, nodelay, p_offset = 0;
	uint4		bfsize = DEFAULT_SOCKET_BUFFER_SIZE, ibfsize;
	char		*tab;
	int		save_errno;
	mstr		chset_mstr;
	gtm_chset_t	temp_ochset, temp_ichset;
	size_t		d_socket_struct_len;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(iod->state == dev_open);
	assert(iod->type == gtmsocket);
	dsocketptr = (d_socket_struct *)(iod->dev_sp);
	/* ---------------------------------- parse the command line ------------------------------------ */
	n_specified = 0;
	zff_len = -1; /* indicates neither ZFF nor ZNOFF specified */
	delimiter_len = -1; /* indicates neither DELIM nor NODELIM specified */
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);

	/* A read or wait was interrupted for this device. Allow only parmless use in $zinterrupt code for
	 * and interrupted device.
	 */
	if (iop_eol != *(pp->str.addr + p_offset))
	{	/* Parameters were specified */
		if (dsocketptr->mupintr)
		{	/* And if we are in $zinterrupt code this is not allowed */
			if (dollar_zininterrupt)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
			/* We are not in $zinterrupt code and this device was not resumed properly
			 * so clear its restartability.
			 */
			io_find_mvstent(iod, TRUE);
			dsocketptr->mupintr = FALSE;
		}
	} else if (dsocketptr->mupintr && !dollar_zininterrupt)
	{	/* The interrupted read was not properly resumed so clear it now */
		dsocketptr->mupintr = FALSE;
		dsocketptr->sock_save_state.who_saved = sockwhich_invalid;
		io_find_mvstent(iod, TRUE);
	}
	while (iop_eol != (ch = *(pp->str.addr + p_offset++)))
	{
		assert((params)ch < (params)n_iops);
		switch (ch)
		{
			case iop_exception:
				DEF_EXCEPTION(pp, p_offset, iod);
				break;
			case iop_filter:
				len = *(pp->str.addr + p_offset);
				tab = pp->str.addr + p_offset + 1;
				if ((fil_type = namelook(filter_index, filter_names, tab, len)) < 0)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TTINVFILTER);
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
				handlea_len = (int)(*(pp->str.addr + p_offset));
				memcpy(handlea, (char *)(pp->str.addr + p_offset + 1), handlea_len);
				break;
			case iop_detach:
				n_specified++;
				detach_specified = TRUE;
				handled_len = (int)(*(pp->str.addr + p_offset));
				memcpy(handled, (char *)(pp->str.addr + p_offset + 1), handled_len);
				break;
			case iop_connect:
				n_specified++;
				connect_specified = TRUE;
				int_len = (int)(*(pp->str.addr + p_offset));
				if (int_len < USR_SA_MAXLITLEN)
				{
					memcpy(sockaddr, (char *)(pp->str.addr + p_offset + 1), int_len);
					sockaddr[int_len] = '\0';
				} else
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ADDRTOOLONG,
						  4, int_len, pp->str.addr + p_offset + 1, int_len, USR_SA_MAXLITLEN);
				break;
			case iop_delimiter:
				n_specified++;
				delimiter_len = (int4)(unsigned char)*(pp->str.addr + p_offset);
				if (((MAX_DELIM_LEN + 1) * MAX_N_DELIMITER) >= delimiter_len)
					memcpy(delimiter_buffer, (pp->str.addr + p_offset + 1), delimiter_len);
				else
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DELIMSIZNA);
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
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, bfsize);
				break;
			case	iop_zibfsize:
				ibfsize_specified = TRUE;
				GET_ULONG(ibfsize, pp->str.addr + p_offset);
				if ((0 == ibfsize) || (MAX_INTERNAL_SOCBUF_SIZE < ibfsize))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, ibfsize);
				break;
			case iop_ioerror:
				n_specified++;
				ioerror_specified = TRUE;
				ioerror = *(char *)(pp->str.addr + p_offset + 1);
				break;
			case iop_zlisten:
				n_specified++;
				listen_specified = TRUE;
				int_len = (int)(*(pp->str.addr + p_offset));
				if (int_len < USR_SA_MAXLITLEN)
				{
					memcpy(sockaddr, (char *)(pp->str.addr + p_offset + 1), int_len);
					sockaddr[int_len] = '\0';
				} else
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ADDRTOOLONG,
						  4, int_len, pp->str.addr + p_offset + 1, int_len, USR_SA_MAXLITLEN);
				break;
			case iop_socket:
				n_specified++;
				socket_specified = TRUE;
				handles_len = (int)(*(pp->str.addr + p_offset));
				memcpy(handles, (char *)(pp->str.addr + p_offset + 1), handles_len);
				break;
			case iop_ipchset:
#				if defined(KEEP_zOS_EBCDIC)
				if ((iconv_t)0 != iod->input_conv_cd)
					ICONV_CLOSE_CD(iod->input_conv_cd);
				SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->in_code_set)
					ICONV_OPEN_CD(iod->input_conv_cd, INSIDE_CH_SET, (char *)(pp->str.addr + p_offset + 1));
#				endif
				GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
				SET_ENCODING(temp_ichset, &chset_mstr);
				if (!gtm_utf8_mode && IS_UTF_CHSET(temp_ichset))
					break;	/* ignore UTF chsets if not utf8_mode */
				ichset_specified = TRUE;
				break;
			case iop_opchset:
#				if defined(KEEP_zOS_EBCDIC)
				if ((iconv_t)0 != iod->output_conv_cd)
					ICONV_CLOSE_CD(iod->output_conv_cd);
				SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->out_code_set)
					ICONV_OPEN_CD(iod->output_conv_cd, (char *)(pp->str.addr + p_offset + 1), INSIDE_CH_SET);
#				endif
				GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
				SET_ENCODING(temp_ochset, &chset_mstr);
				if (!gtm_utf8_mode && IS_UTF_CHSET(temp_ochset))
					break;	/* ignore UTF chsets if not utf8_mode */
				ochset_specified = TRUE;
				break;
			case iop_chset:
				GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
				SET_ENCODING(temp_ochset, &chset_mstr);
				SET_ENCODING(temp_ichset, &chset_mstr);
				if (!gtm_utf8_mode && IS_UTF_CHSET(temp_ichset))
					break;	/* ignore UTF chsets if not utf8_mode */
				ochset_specified = TRUE;
				ichset_specified = TRUE;
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
			case iop_length:
				GET_LONG(length, pp->str.addr + p_offset);
				if (length < 0)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARMNEG);
				iod->length = length;
				break;
			case iop_width:
				/* SOCKET WIDTH is handled the same way as TERMINAL WIDTH */
				GET_LONG(width, pp->str.addr + p_offset);
				if (width < 0)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARMNEG);
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
			case iop_morereadtime:
				/* Time in milliseconds socket read will wait for more data before returning */
				GET_LONG(moreread_timeout, pp->str.addr + p_offset);
				if (-1 == moreread_timeout)
					moreread_timeout = DEFAULT_MOREREAD_TIMEOUT;
				else if (-1 > moreread_timeout)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARMNEG);
				else if (MAX_MOREREAD_TIMEOUT < moreread_timeout)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_MRTMAXEXCEEDED, 1, MAX_MOREREAD_TIMEOUT);
				moreread_specified = TRUE;
				break;
			case iop_flush:
				n_specified++;
				flush_specified = TRUE;
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
	{
		REVERT_GTMIO_CH(&iod->pair, ch_set);
		return;
	}
	/* ------------------------------ compatibility verification -------------------------------- */
	if ((socket_specified) && ((n_specified > 2) || ((2 == n_specified) && (0 >= delimiter_len))))
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ACOMPTBINC, 6, LEN_AND_LIT("SOCKET"), LEN_AND_LIT("DELIMITER"),
				 LEN_AND_LIT("USE"));
		return;
	}
	if (connect_specified && listen_specified)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("CONNECT"), LEN_AND_LIT("ZLISTEN"),
				 LEN_AND_LIT("USE"));
		return;
	}
	if ((ichset_specified || ochset_specified) && (listen_specified || connect_specified))
	{	/* CHSET cannot be specified when opening a new socket, if there already are open sockets. */
		if (0 < dsocketptr->n_socket && ((temp_ochset != iod->ochset) || (temp_ichset != iod->ichset)))
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_CHSETALREADY, 8,
				chset_names[iod->ichset].len, chset_names[iod->ichset].addr,
				chset_names[iod->ochset].len, chset_names[iod->ochset].addr);
			return;
		}
	}
	if (delay_specified && nodelay_specified)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ABNCOMPTINC, 6, LEN_AND_LIT("DELAY"), LEN_AND_LIT("NODELAY"),
				 LEN_AND_LIT("OPEN"));
		return;
	}
	/* ------------------ make a local copy of device structure to play with -------------------- */
	d_socket_struct_len = SIZEOF(d_socket_struct) + (SIZEOF(socket_struct) * (gtm_max_sockets - 1));
	memcpy(newdsocket, dsocketptr, d_socket_struct_len);
	/* --------------- handle the two special cases attach/detach first ------------------------- */
	if (detach_specified)
	{
		if (1 < n_specified)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ANCOMPTINC, 4, LEN_AND_LIT("DETACH"), LEN_AND_LIT("USE"));
			return;
		}
		if (NULL == socket_pool)
		{
			iosocket_poolinit();
			memcpy(newdsocket, dsocketptr, d_socket_struct_len);
		}
		iosocket_switch(handled, handled_len, newdsocket, socket_pool);
		memcpy(dsocketptr, newdsocket, d_socket_struct_len);
		if (0 > dsocketptr->current_socket)
		{
			io_curr_device.in = io_std_device.in;
			io_curr_device.out = io_std_device.out;
		}
		REVERT_GTMIO_CH(&iod->pair, ch_set);
		return; /* detach can only be specified by itself */
	}
	if (attach_specified)
	{	/* NOTE: A socket could be moved from one device to another using DETACH/ATTACH. A socket does not carry I[O]CHSET
		 * with it while being moved. Such a socket will use the I[O]CHSET of the device it is ATTACHed to. If there is
		 * input still buffered, this may cause unintentional consequences in the application if I[O]CHSET changes. GT.M
		 * does not detect (or report) a change in I[O]CHSET due to DETACH/ATTACH.
		 */
		if (1 < n_specified)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ANCOMPTINC, 4, LEN_AND_LIT("ATTACH"), LEN_AND_LIT("USE"));
			return;
		}
		if (NULL == socket_pool)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKNOTFND, 2, handlea_len, handlea);
			return;
		}
		iosocket_switch(handlea, handlea_len, socket_pool, newdsocket);
		memcpy(dsocketptr, newdsocket, d_socket_struct_len);
		REVERT_GTMIO_CH(&iod->pair, ch_set);
		return; /* attach can only be specified by itself */
	}
	/* ------------ create/identify the socket to work on and make a local copy ----------------- */
	if (create_new_socket = (listen_specified || connect_specified))	/* real "=" */
	{
		/* allocate the structure for a new socket */
		if (NULL == (socketptr = iosocket_create(sockaddr, bfsize, -1, listen_specified)))
		{
			REVERT_GTMIO_CH(&iod->pair, ch_set);
			return;
		}
		if (gtm_max_sockets <= newdsocket->n_socket)
		{
			if (FD_INVALID != socketptr->temp_sd)
				close(socketptr->temp_sd);
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SOCKMAX, 1, gtm_max_sockets);
			return;
		}
		/* give the new socket a handle */
		iosocket_handle(handles, &handles_len, TRUE, dsocketptr);
		socketptr->handle_len = handles_len;
		memcpy(socketptr->handle, handles, handles_len);
		socketptr->dev = newdsocket;	/* use newdsocket temporarily for the sake of bind/connect */
		socketptr->filemode_mask = 0;
		socketptr->uic.mem = (uid_t)-1;
		socketptr->uic.grp = (gid_t)-1;
	} else
	{
		if (socket_specified)
		{
			/* use the socket flag to identify which socket to apply changes */
			if (0 > (index = iosocket_handle(handles, &handles_len, FALSE, newdsocket)))
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKNOTFND, 2, handles_len, handles);
				return;
			}
			newdsocket->current_socket = index;
			socketptr = newdsocket->socket[index];
			if ((1 == n_specified) && (socket_listening == socketptr->state))
			{	/* accept a new connection if there is one */
				socketptr->pendingevent = FALSE;
				iod->dollar.key[0] = '\0';
				save_errno = iosocket_accept(dsocketptr, socketptr, TRUE);
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return;
			}
		} else
		{
			if (0 >= newdsocket->n_socket)
			{
				if (iod == io_std_device.out)
					ionl_use(iod, pp);
				else
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSOCKETINDEV);
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return;
			}
			if (newdsocket->n_socket <= newdsocket->current_socket)
			{
				assert(FALSE);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CURRSOCKOFR, 2, newdsocket->current_socket,
						newdsocket->n_socket);
				return;
			}
			socketptr = newdsocket->socket[newdsocket->current_socket];
		}
		socketptr->temp_sd = FD_INVALID;
	}
	newsocket = *socketptr;
	/* ---------------------- apply changes to the local copy of the socket --------------------- */
	if (0 <= delimiter_len)
	{
		iosocket_delimiter(delimiter_buffer, delimiter_len, &newsocket, (0 == delimiter_len));
		socketptr->n_delimiter = 0;	/* prevent double frees if error */
	}
	if (iod->wrap && 0 != newsocket.n_delimiter && iod->width < newsocket.delimiter[0].len)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DELIMWIDTH, 2, iod->width, newsocket.delimiter[0].len);
	/* Process the CHSET changes */
	if (ichset_specified)
	{
		CHECK_UTF16_VARIANT_AND_SET_CHSET_SOCKET(dsocketptr->ichset_utf16_variant, iod->ichset, temp_ichset,
								assert(!socketptr));
		newdsocket->ichset_utf16_variant = dsocketptr->ichset_utf16_variant;
		newdsocket->ichset_specified = dsocketptr->ichset_specified = TRUE;
	}
	if (ochset_specified)
	{
		CHECK_UTF16_VARIANT_AND_SET_CHSET_SOCKET(dsocketptr->ochset_utf16_variant, iod->ochset, temp_ochset,
								assert(!socketptr));
		newdsocket->ochset_utf16_variant = dsocketptr->ochset_utf16_variant;
		newdsocket->ochset_specified = dsocketptr->ochset_specified = TRUE;
	}
	if ((CHSET_M != iod->ichset) && (CHSET_UTF16 != iod->ichset) && (CHSET_MAX_IDX > iod->ichset))
		get_chset_desc(&chset_names[iod->ichset]);
	if ((CHSET_M != iod->ochset) && (CHSET_UTF16 != iod->ochset) && (CHSET_MAX_IDX > iod->ichset))
		get_chset_desc(&chset_names[iod->ochset]);
	if (0 <= zff_len && /* ZFF or ZNOFF specified */
	    0 < (newsocket.zff.len = zff_len)) /* assign the new ZFF len, might be 0 from ZNOFF, or ZFF="" */
	{	/* ZFF="non-zero-len-string" specified */
		if (gtm_utf8_mode) /* Check if ZFF has any invalid UTF-8 character */
		{	/* Note: the ZFF string originates from the source program, so is in UTF-8 mode or M mode regardless of
			* OCHSET of this device. ZFF is output on WRITE # command, and MUST contain valid UTF-8 sequence.
			*/
			utf8_len_strict(zff_buffer, zff_len);
		}
		if ((NULL != newsocket.ozff.addr) && (socketptr->ozff.addr != socketptr->zff.addr))
			free_ozff = newsocket.ozff.addr;	/* previously converted */
		if (NULL == newsocket.zff.addr) /* we rely on newsocket.zff.addr being set to 0 in iosocket_create() */
		{
			socketptr->zff.addr = newsocket.zff.addr = (char *)malloc(MAX_ZFF_LEN);
			socketptr->zff.len = zff_len;		/* in case error so SOCKET_FREE frees */
		}
		memcpy(newsocket.zff.addr, zff_buffer, zff_len);
		newsocket.ozff = newsocket.zff;
	} else if (0 == zff_len)
	{
		if ((NULL != newsocket.ozff.addr) && (socketptr->ozff.addr != socketptr->zff.addr))
			free_ozff = newsocket.ozff.addr;	/* previously converted */
		newsocket.ozff = newsocket.zff;
	}
	if (gtm_utf8_mode)
	{	/* If CHSET is being changed to UTF-16, and delimitors are not converted, convert them
		 * 	But only if the UTF16 variant has already been determined.
		 * If CHSET is being changed to non-UTF-16, and delims are converted, free them
		 */
		if (ichset_specified)
			for (soc_cnt=0; soc_cnt < dsocketptr->n_socket; soc_cnt++)
			{
				localsocketptr = dsocketptr->socket[soc_cnt];
				if (!(localsocketptr && (0 < localsocketptr->n_delimiter)))
					continue;
				if (((localsocketptr->delimiter[0].addr == localsocketptr->idelimiter[0].addr)
						&& IS_UTF16_CHSET(iod->ichset) && IS_UTF16_CHSET(dsocketptr->ichset_utf16_variant))
						|| ((localsocketptr->delimiter[0].addr != localsocketptr->idelimiter[0].addr)
						&& !IS_UTF16_CHSET(iod->ichset)))
					iosocket_idelim_conv(localsocketptr, iod->ichset);
			}
		if (ochset_specified)
			for (soc_cnt=0; soc_cnt < dsocketptr->n_socket; soc_cnt++)
			{
				localsocketptr = dsocketptr->socket[soc_cnt];
				if (!(localsocketptr && (0 < localsocketptr->n_delimiter)))
					continue;
				if (((localsocketptr->delimiter[0].addr == localsocketptr->odelimiter0.addr) &&
						IS_UTF16_CHSET(iod->ochset) && IS_UTF16_CHSET(dsocketptr->ochset_utf16_variant))
					|| ((localsocketptr->delimiter[0].addr != localsocketptr->odelimiter0.addr)
						&& !IS_UTF16_CHSET(iod->ochset)))
							iosocket_odelim_conv(localsocketptr, iod->ochset);
			}
		if (ochset_specified)
		{	/* Now convert the ZFFs */
			if (!IS_UTF16_CHSET(iod->ochset))
			{	/* Changed to a non-UTF16 CHSET. free all converted ZFFs */
				for (soc_cnt=0; soc_cnt < dsocketptr->n_socket; soc_cnt++)
				{
					localsocketptr = dsocketptr->socket[soc_cnt];
					if (localsocketptr && (NULL != localsocketptr->ozff.addr) && (0 < localsocketptr->zff.len)
						&& (localsocketptr->ozff.addr != localsocketptr->zff.addr))
					{
						if (localsocketptr->ozff.addr == free_ozff)
							free_ozff = NULL;		/* Prevent double free of free_ozff */
						free(localsocketptr->ozff.addr);	/* previously converted */
					}
					localsocketptr->ozff = localsocketptr->zff;	/* contains converted UTF-16 form */
				}
			} else if (IS_UTF16_CHSET(dsocketptr->ochset_utf16_variant))
			{	/* Changed to UTF-16 CHSET. convert all ZFFs */
				conv_buff = malloc(MAX_ZFF_LEN);
				for (soc_cnt=0; soc_cnt < dsocketptr->n_socket; soc_cnt++)
				{
					localsocketptr = dsocketptr->socket[soc_cnt];
					if (localsocketptr && (NULL != localsocketptr->zff.addr) && (0 < localsocketptr->zff.len)
						&& (localsocketptr->ozff.addr == localsocketptr->zff.addr))
					{
						conv_len = MAX_ZFF_LEN;
						new_ozff_len = gtm_conv(chset_desc[CHSET_UTF8], chset_desc[iod->ochset],
									&localsocketptr->zff, conv_buff, &conv_len);
						assert(MAX_ZFF_LEN > new_ozff_len);
						localsocketptr->ozff.len = new_ozff_len;
						localsocketptr->ozff.addr = malloc(new_ozff_len);
						memcpy(localsocketptr->ozff.addr, conv_buff, new_ozff_len);
						memset(conv_buff, 0, MAX_ZFF_LEN);	/* Reset to be reused. */
					}
				}
			}
		}
	}
	if (ioerror_specified)
		newsocket.ioerror = ('T' == ioerror || 't' == ioerror);
	if (nodelay_specified || delay_specified)
		newsocket.nodelay = nodelay_specified;	/* defaults to DELAY */
	if (ibfsize_specified)
		newsocket.bufsiz = ibfsize;
	if (moreread_specified)
	{
		newsocket.moreread_timeout = moreread_timeout;
		newsocket.def_moreread_timeout = TRUE;	/* need to know this was user-defined in iosocket_readfl.c */
	}
	if (!create_new_socket)
	{
		/* these changes apply to only pre-existing sockets */
		if (flush_specified)
			iosocket_flush(iod);	/* buffered output if any */
		if (bfsize_specified)
			newsocket.buffer_size = bfsize;
#		ifdef TCP_NODELAY
		if (socket_local != newsocket.protocol)
		{
			nodelay = newsocket.nodelay ? 1 : 0;
			if ((socketptr->nodelay != newsocket.nodelay) &&
			    (-1 == setsockopt(newsocket.sd, IPPROTO_TCP,
						      TCP_NODELAY, &nodelay, SIZEOF(nodelay))))
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("TCP_NODELAY"),
					save_errno, LEN_AND_STR(errptr));
				return;
			}
		}
#		endif
		if ((socketptr->bufsiz != newsocket.bufsiz)
			&& (-1 == setsockopt(newsocket.sd, SOL_SOCKET, SO_RCVBUF, &newsocket.bufsiz, SIZEOF(newsocket.bufsiz))))
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("SO_RCVBUF"), save_errno,
				LEN_AND_STR(errptr));
			return;
		}
		if (socketptr->buffer_size != newsocket.buffer_size)
		{
			if (socketptr->buffered_length > bfsize)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBFNOTEMPTY, 2, bfsize, socketptr->buffered_length);
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
	if ((listen_specified && ((!iosocket_bind(&newsocket, NO_M_TIMEOUT, ibfsize_specified, FALSE))
			|| (!iosocket_listen_sock(&newsocket, DEFAULT_LISTEN_DEPTH))))
		|| (connect_specified && (!iosocket_connect(&newsocket, 0, ibfsize_specified))))
	{	/* error message should be printed from bind/connect */
		if (socketptr->sd > 0)
			(void)close(socketptr->sd);
		SOCKET_FREE(socketptr);
		REVERT_GTMIO_CH(&iod->pair, ch_set);
		return;
	}
	/* ------------------------------------ commit changes -------------------------------------- */
	if (create_new_socket)
	{
		/* a new socket is created. so add to the list */
		newsocket.dev = dsocketptr;
		newdsocket->socket[newdsocket->n_socket++] = socketptr;
		newdsocket->current_socket = newdsocket->n_socket - 1;
	}
	else
	{
		if (NULL != free_ozff)
			free(free_ozff);
		if (socketptr->buffer_size != newsocket.buffer_size)
			free(socketptr->buffer);
	}
	*socketptr = newsocket;
	memcpy(dsocketptr, newdsocket, d_socket_struct_len);
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
