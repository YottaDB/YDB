/****************************************************************
 *								*
 * Copyright (c) 2013-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_use.c */
#include "mdef.h"

#include "gtm_limits.h"
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

GBLREF	boolean_t		gtm_utf8_mode;
GBLREF	d_socket_struct		*newdsocket, *socket_pool;
GBLREF	uint4			ydb_max_sockets;
GBLREF	io_desc			*active_device;
GBLREF	io_pair			io_curr_device, io_std_device;
GBLREF	spdesc			stringpool;
GBLREF	UConverter  		*chset_desc[];
GBLREF	volatile boolean_t	dollar_zininterrupt;

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
error_def(ERR_GETSOCKOPTERR);
error_def(ERR_SETSOCKOPTERR);
error_def(ERR_SOCKBFNOTEMPTY);
error_def(ERR_SOCKNOTFND);
error_def(ERR_SOCKMAX);
error_def(ERR_TTINVFILTER);
error_def(ERR_ZFF2MANY);
error_def(ERR_ZINTRECURSEIO);

void	iosocket_use(io_desc *iod, mval *pp)
{
	unsigned char	ch, len;
	static char	*conv_buff = NULL;
	int		new_ozff_len, conv_len, handled_len = -1, handlea_len = -1, handles_len, int_len, soc_cnt;
	int4		length, width, new_len;
	d_socket_struct *dsocketptr;
	socket_struct	*socketptr = NULL, *curr_socketptr = NULL, *localsocketptr;
	mstr_len_t	delim_len;
	char		handlea[MAX_HANDLE_LEN], handles[MAX_HANDLE_LEN], handled[MAX_HANDLE_LEN];
	char		addr[SA_MAXLITLEN], *errptr, sockaddr[SA_MAXLITLEN],
			temp_addr[SA_MAXLITLEN], ioerror = 0, *free_ozff = NULL;
	unsigned char	delimiter_buffer[MAX_N_DELIMITER * (MAX_DELIM_LEN + 1)];
	unsigned char	zff_buffer[MAX_ZFF_LEN], options_buffer[UCHAR_MAX + 1];
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
	int4 		index, n_specified, zff_len, delimiter_len, moreread_timeout = -1, options_len;
	int4		n_specified_dev, n_specified_socket;
	int4		n_incomplete_dev;	/* device level not done in iop loop */
	int		fil_type, nodelay, p_offset = 0, newbufsiz;
	GTM_SOCKLEN_TYPE	sockbuflen;
	uint4		bfsize = DEFAULT_SOCKET_BUFFER_SIZE, ibfsize = MAXUINT4;
	char		*tab;
	int		save_errno;
	mstr		chset_mstr, optionstr;
	gtm_chset_t	temp_ochset = CHSET_MAX_IDX_ALL, temp_ichset = CHSET_MAX_IDX_ALL;
	size_t		d_socket_struct_len;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(iod->state == dev_open);
	assert(iod->type == gtmsocket);
	dsocketptr = (d_socket_struct *)(iod->dev_sp);
	/* ---------------------------------- parse the command line ------------------------------------ */
	n_specified = n_specified_dev = n_specified_socket = n_incomplete_dev = 0;
	zff_len = -1; /* indicates neither ZFF nor ZNOFF specified */
	delimiter_len = -1; /* indicates neither DELIM nor NODELIM specified */
	options_len = 0;	/* no OPTIONS yet */
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);

	/* A read or wait was interrupted for this device. Allow only parmless use in $zinterrupt code for
	 * and interrupted device.
	 */
	if (iop_eol != *(pp->str.addr + p_offset))
	{	/* Parameters were specified */
		if (dsocketptr->mupintr)
		{	/* And if we are in $zinterrupt code this is not allowed */
			if (dollar_zininterrupt)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZINTRECURSEIO);
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
				n_specified_dev++;
				DEF_EXCEPTION(pp, p_offset, iod);
				break;
			case iop_filter:
				n_specified_dev++;
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
				n_specified_dev++;
				iod->write_filter = 0;
				break;
			case iop_attach:
				n_specified++;
				attach_specified = TRUE;
				handlea_len = (int)(unsigned char)*(pp->str.addr + p_offset);
				memcpy(handlea, (char *)(pp->str.addr + p_offset + 1), handlea_len);
				break;
			case iop_detach:
				n_specified++;
				detach_specified = TRUE;
				handled_len = (int)(unsigned char)*(pp->str.addr + p_offset);
				memcpy(handled, (char *)(pp->str.addr + p_offset + 1), handled_len);
				break;
			case iop_connect:
				n_specified++;
				connect_specified = TRUE;
				int_len = (int)(unsigned char)*(pp->str.addr + p_offset);
				if (int_len < USR_SA_MAXLITLEN)
				{
					memcpy(sockaddr, (char *)(pp->str.addr + p_offset + 1), int_len);
					sockaddr[int_len] = '\0';
				} else
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_ADDRTOOLONG,
						4, int_len, pp->str.addr + p_offset + 1, int_len, USR_SA_MAXLITLEN - 1);
				break;
			case iop_delimiter:
				n_specified++;
				delimiter_len = (int4)(unsigned char)*(pp->str.addr + p_offset);
				if (((MAX_DELIM_LEN + 1) * MAX_N_DELIMITER) >= delimiter_len)
					memcpy(delimiter_buffer, (pp->str.addr + p_offset + 1), delimiter_len);
				else
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_DELIMSIZNA);
				break;
			case	iop_nodelimiter:
				n_specified++;
				delimiter_len = 0;
				break;
			case	iop_zdelay:
				n_specified_socket++;
				delay_specified = TRUE;
				break;
			case	iop_znodelay:
				n_specified_socket++;
				nodelay_specified = TRUE;
				break;
			case	iop_zbfsize:
				n_specified_socket++;
				bfsize_specified = TRUE;
				GET_ULONG(bfsize, pp->str.addr + p_offset);
				if ((0 == bfsize) || (MAX_SOCKET_BUFFER_SIZE < bfsize))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, bfsize);
				break;
			case	iop_zibfsize:
				n_specified_socket++;
				ibfsize_specified = TRUE;
				GET_ULONG(ibfsize, pp->str.addr + p_offset);
				if (0 == ibfsize)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_ILLESOCKBFSIZE, 1, ibfsize);
				break;
			case iop_ioerror:
				n_specified++;
				ioerror_specified = TRUE;
				ioerror = *(char *)(pp->str.addr + p_offset + 1);
				break;
			case iop_zlisten:
				n_specified++;
				listen_specified = TRUE;
				int_len = (int)(unsigned char)*(pp->str.addr + p_offset);
				if ((USR_SA_MAXLITLEN > int_len) && (0 < int_len))
				{
					memcpy(sockaddr, (char *)(pp->str.addr + p_offset + 1), int_len);
					sockaddr[int_len] = '\0';
				} else
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_ADDRTOOLONG,
						4, int_len, pp->str.addr + p_offset + 1, int_len, USR_SA_MAXLITLEN - 1);
				break;
			case iop_socket:
				n_specified++;
				socket_specified = TRUE;
				handles_len = (int)(unsigned char)*(pp->str.addr + p_offset);
				memcpy(handles, (char *)(pp->str.addr + p_offset + 1), handles_len);
				break;
			case iop_ipchset:
				n_specified_dev++;
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
				n_incomplete_dev++;
				break;
			case iop_opchset:
				n_specified_dev++;
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
				n_incomplete_dev++;
				break;
			case iop_chset:
				n_specified_dev++;
				GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
				SET_ENCODING(temp_ochset, &chset_mstr);
				SET_ENCODING(temp_ichset, &chset_mstr);
				if (!gtm_utf8_mode && IS_UTF_CHSET(temp_ichset))
					break;	/* ignore UTF chsets if not utf8_mode */
				ochset_specified = TRUE;
				ichset_specified = TRUE;
				n_incomplete_dev++;
				break;
			case iop_zff:
				n_specified_socket++;
				zff_len = (int4)(unsigned char)*(pp->str.addr + p_offset);
				if (MAX_ZFF_LEN >= zff_len)
					memcpy(zff_buffer, (char *)(pp->str.addr + p_offset + 1), zff_len);
				else
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_ZFF2MANY, 2, zff_len, MAX_ZFF_LEN);
				break;
			case iop_znoff:
				n_specified_socket++;
				zff_len = 0;
				break;
			case iop_length:
				n_specified_dev++;
				GET_LONG(length, pp->str.addr + p_offset);
				if (length < 0)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_DEVPARMNEG);
				iod->length = length;
				break;
			case iop_width:
				n_specified_dev++;
				/* SOCKET WIDTH is handled the same way as TERMINAL WIDTH */
				GET_LONG(width, pp->str.addr + p_offset);
				if (width < 0)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_DEVPARMNEG);
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
				n_specified_dev++;
				iod->wrap = TRUE;
				break;
			case iop_nowrap:
				n_specified_dev++;
				iod->wrap = FALSE;
				break;
			case iop_morereadtime:
				n_specified_socket++;
				/* Time in milliseconds socket read will wait for more data before returning */
				GET_LONG(moreread_timeout, pp->str.addr + p_offset);
				if (-1 == moreread_timeout)
					moreread_timeout = DEFAULT_MOREREAD_TIMEOUT;
				else if (-1 > moreread_timeout)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_DEVPARMNEG);
				else if (MAX_MOREREAD_TIMEOUT < moreread_timeout)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_MRTMAXEXCEEDED, 1, MAX_MOREREAD_TIMEOUT);
				moreread_specified = TRUE;
				break;
			case iop_flush:
				n_specified++;
				flush_specified = TRUE;
				break;
			case iop_options:
				n_specified++;
				options_len = (int4)(unsigned char)*(pp->str.addr + p_offset);
				if (UCHAR_MAX >= options_len)
				{
					memcpy(options_buffer, (unsigned char *)(pp->str.addr + p_offset + 1), options_len);
					options_buffer[options_len] = '\0';
				}
				break;
			default:
				/* ignore deviceparm */
				break;
		}
		UPDATE_P_OFFSET(p_offset, ch, pp);	/* updates "p_offset" using "ch" and "pp" */
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
		assert(CHSET_MAX_IDX_ALL != temp_ochset);
		assert(CHSET_MAX_IDX_ALL != temp_ichset);
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
	/* --------------- handle the three special cases socket/attach/detach first ----------------------- */
	if (socket_specified)
	{
		/* use the socket flag to identify which socket to apply changes */
		if (0 > (index = iosocket_handle(handles, &handles_len, FALSE, dsocketptr)))
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKNOTFND, 2, handles_len, handles);
			return;
		}
		dsocketptr->current_socket = index;
		socketptr = dsocketptr->socket[index];
		if ((1 == n_specified) && (0 == n_specified_socket) && (0 == n_incomplete_dev))
		{	/* other device level parameters already applied so ignore */
			if (socket_listening == socketptr->state)
			{	/* accept a new connection if there is one */
				socketptr->pendingevent = socketptr->current_events = 0;
				iod->dollar.key[0] = '\0';
				save_errno = iosocket_accept(dsocketptr, socketptr, TRUE);
			}
			/* return early since nothing else to do */
			REVERT_GTMIO_CH(&iod->pair, ch_set);
			return;
		}
	}
	if (detach_specified)
	{
		if ((1 < n_specified) || (0 < n_specified_socket) || (0 < n_incomplete_dev))
		{	/* allow device level parameters already done */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ANCOMPTINC, 4, LEN_AND_LIT("DETACH"), LEN_AND_LIT("USE"));
			return;
		}
		if (NULL == socket_pool)
			iosocket_poolinit();
		assert(0 <= handled_len);
		iosocket_switch(handled, handled_len, dsocketptr, socket_pool);
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
		if ((1 < n_specified) || (0 < n_specified_socket) || (0 < n_incomplete_dev))
		{	/* allow device level parameters already done */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ANCOMPTINC, 4, LEN_AND_LIT("ATTACH"), LEN_AND_LIT("USE"));
			return;
		}
		assert(0 <= handlea_len);
		if (NULL == socket_pool)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKNOTFND, 2, handlea_len, handlea);
			return;
		}
		iosocket_switch(handlea, handlea_len, socket_pool, dsocketptr);
		REVERT_GTMIO_CH(&iod->pair, ch_set);
		return; /* attach can only be specified by itself */
	}
	/* ------------------ make a local copy of device structure to play with -------------------- */
	d_socket_struct_len = SIZEOF(d_socket_struct) + (SIZEOF(socket_struct) * (ydb_max_sockets - 1));
	memcpy(newdsocket, dsocketptr, d_socket_struct_len);
	/* ------------ create/identify the socket to work on and make a local copy ----------------- */
	if (create_new_socket = (listen_specified || connect_specified))	/* real "=" */
	{
		/* allocate the structure for a new socket */
		if (NULL == (socketptr = iosocket_create(sockaddr, bfsize, -1, listen_specified)))
		{
			REVERT_GTMIO_CH(&iod->pair, ch_set);
			return;
		}
		if (ydb_max_sockets <= newdsocket->n_socket)
		{
			if (FD_INVALID != socketptr->temp_sd)
				close(socketptr->temp_sd);
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_SOCKMAX, 1, ydb_max_sockets);
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
		curr_socketptr = socketptr;
	} else
	{
		if (!socket_specified)
		{
			if (0 >= newdsocket->n_socket)
			{
				if (iod == io_std_device.out)
					ionl_use(iod, pp);
				else
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_NOSOCKETINDEV);
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
		/* Make a copy of the socket_struct so most errors leave the previous structure as is.
		 * SOCKET_FREE should not be used on this copy since it frees other things such as buffers
		 * DELIMITER is an exception due to how iosocket_delimiter manages storage.
		 */
		assert(socketptr);
		curr_socketptr = (socket_struct *)malloc(sizeof(socket_struct));
		memcpy(curr_socketptr, socketptr, sizeof(socket_struct));
		socketptr->temp_sd = FD_INVALID;
	}
	assert(NULL != curr_socketptr);
	/* ---------------------- apply changes to the local copy of the socket --------------------- */
	if (0 <= delimiter_len)
	{	/* note that previous delimiters are freed by the following */
		iosocket_delimiter(delimiter_buffer, delimiter_len, curr_socketptr, (0 == delimiter_len));
		if (curr_socketptr != socketptr)
			socketptr->n_delimiter = 0;	/* prevent double frees or use of now freed memory if error */
	}
	if (iod->wrap && (0 != curr_socketptr->n_delimiter) && (iod->width < curr_socketptr->delimiter[0].len))
	{
		delim_len = curr_socketptr->delimiter[0].len;
		if (create_new_socket)
			SOCKET_FREE(socketptr)
		else
		{
			if (0 < delimiter_len)		/* free new delimiters */
				iosocket_delimiter((unsigned char *)NULL, 0, curr_socketptr, TRUE);
			free(curr_socketptr);	/* other pointers copied from socketptr so just free this */
		}
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_DELIMWIDTH, 2, iod->width, delim_len);
	}
	if (0 < options_len)
	{	/* call devoptions w/ options_buffer to update socket struct */
		optionstr.addr = (char *)options_buffer;
		optionstr.len = options_len;
		devoptions(NULL, curr_socketptr, &optionstr, "USE", IOP_USE_OK);
	}
	/* Process the CHSET changes */
	if (ichset_specified)
	{
		assert(CHSET_MAX_IDX_ALL != temp_ichset);
		CHECK_UTF16_VARIANT_AND_SET_CHSET_SOCKET(dsocketptr->ichset_utf16_variant, iod->ichset, temp_ichset,
								assert(!socketptr));
		newdsocket->ichset_utf16_variant = dsocketptr->ichset_utf16_variant;
		newdsocket->ichset_specified = dsocketptr->ichset_specified = TRUE;
	}
	if (ochset_specified)
	{
		assert(CHSET_MAX_IDX_ALL != temp_ochset);
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
	    0 < (curr_socketptr->zff.len = zff_len)) /* assign the new ZFF len, might be 0 from ZNOFF, or ZFF="" */
	{	/* ZFF="non-zero-len-string" specified */
		if (gtm_utf8_mode) /* Check if ZFF has any invalid UTF-8 character */
		{	/* Note: the ZFF string originates from the source program, so is in UTF-8 mode or M mode regardless of
			* OCHSET of this device. ZFF is output on WRITE # command, and MUST contain valid UTF-8 sequence.
			*/
			utf8_len_strict(zff_buffer, zff_len);
		}
		if ((NULL != curr_socketptr->ozff.addr) && (socketptr->ozff.addr != socketptr->zff.addr))
			free_ozff = curr_socketptr->ozff.addr;	/* previously converted */
		if (NULL == curr_socketptr->zff.addr) /* we rely on curr_socketptr->zff.addr being set to 0 in iosocket_create() */
		{
			socketptr->zff.addr = curr_socketptr->zff.addr = (char *)malloc(MAX_ZFF_LEN);
			socketptr->zff.len = zff_len;		/* in case error so SOCKET_FREE frees */
		}
		memcpy(curr_socketptr->zff.addr, zff_buffer, zff_len);
		curr_socketptr->ozff = curr_socketptr->zff;
	} else if (0 == zff_len)
	{
		if ((NULL != curr_socketptr->ozff.addr) && (socketptr->ozff.addr != socketptr->zff.addr))
			free_ozff = curr_socketptr->ozff.addr;	/* previously converted */
		curr_socketptr->ozff = curr_socketptr->zff;
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
		curr_socketptr->ioerror = ('T' == ioerror || 't' == ioerror);
	if (nodelay_specified || delay_specified)
		curr_socketptr->nodelay = nodelay_specified;	/* defaults to DELAY */
	if (ibfsize_specified)
	{
		assert(MAXUINT4 != ibfsize);
		curr_socketptr->bufsiz = ibfsize;
	}
	if (moreread_specified)
	{
		assert(0 <= moreread_timeout);
		curr_socketptr->moreread_timeout = moreread_timeout;
		curr_socketptr->def_moreread_timeout = TRUE;	/* need to know this was user-defined in iosocket_readfl.c */
	}
	if (!create_new_socket)
	{
		/* these changes apply to only pre-existing sockets */
		if (flush_specified)
			iosocket_flush(iod);	/* buffered output if any */
		if (bfsize_specified)
			curr_socketptr->buffer_size = bfsize;
		if (socket_local != curr_socketptr->protocol)
		{
			nodelay = curr_socketptr->nodelay ? 1 : 0;
			if ((socketptr->nodelay != curr_socketptr->nodelay) &&
			    (-1 == setsockopt(curr_socketptr->sd, IPPROTO_TCP,
						      TCP_NODELAY, &nodelay, SIZEOF(nodelay))))
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				assert(curr_socketptr != socketptr);	/* since not create new socket */
				if (0 < delimiter_len)		/* new delimiters */
					iosocket_delimiter((unsigned char *)NULL, 0, curr_socketptr, TRUE);
				free(curr_socketptr);		/* SOCKET_FREE would free storage used by existing socket */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("TCP_NODELAY"),
					save_errno, LEN_AND_STR(errptr));
				return;
			}
		}
		if (socketptr->bufsiz != curr_socketptr->bufsiz)
		{
			if (-1 == setsockopt(curr_socketptr->sd, SOL_SOCKET, SO_RCVBUF, &curr_socketptr->bufsiz,
					SIZEOF(curr_socketptr->bufsiz)))
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				assert(curr_socketptr != socketptr);	/* since not create new socket */
				if (0 < delimiter_len)		/* new delimiters */
					iosocket_delimiter((unsigned char *)NULL, 0, curr_socketptr, TRUE);
				free(curr_socketptr);		/* SOCKET_FREE would free storage used by existing socket */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("SO_RCVBUF"),
						save_errno, LEN_AND_STR(errptr));
				return;
			}
			curr_socketptr->options_state.rcvbuf |= SOCKOPTIONS_USER;
		}
		if (0 != (SOCKOPTIONS_PENDING & curr_socketptr->options_state.sndbuf))
		{
			if (-1 == iosocket_setsockopt(curr_socketptr, "SO_SNDBUF", SO_SNDBUF, SOL_SOCKET, &curr_socketptr->iobfsize,
					sizeof(curr_socketptr->iobfsize), TRUE))
			{
				assert(curr_socketptr != socketptr);	/* since not create new socket */
				if (0 < delimiter_len)		/* new delimiters */
					iosocket_delimiter((unsigned char *)NULL, 0, curr_socketptr, TRUE);
				free(curr_socketptr);		/* SOCKET_FREE would free storage used by existing socket */
				return;
			}
			curr_socketptr->options_state.sndbuf |= SOCKOPTIONS_USER;
			curr_socketptr->options_state.sndbuf &= ~SOCKOPTIONS_PENDING;
		}
		if ((0 < options_len) && ((SOCKOPTIONS_PENDING & curr_socketptr->options_state.alive)
			|| (SOCKOPTIONS_PENDING & curr_socketptr->options_state.cnt)
			|| (SOCKOPTIONS_PENDING & curr_socketptr->options_state.intvl)))
		{	/* options specified and pending keepalive related value to apply */
			if (!iosocket_tcp_keepalive(curr_socketptr, SOCKOPTIONS_FROM_STRUCT, "USE", FALSE))
			{
				assert(curr_socketptr != socketptr);	/* since not create new socket */
				if (0 < delimiter_len)		/* new delimiters */
					iosocket_delimiter((unsigned char *)NULL, 0, curr_socketptr, TRUE);
				free(curr_socketptr);		/* SOCKET_FREE would free storage used by existing socket */
				return;
			}
		}
		if (socketptr->buffer_size != curr_socketptr->buffer_size)
		{
			if (socketptr->buffered_length > bfsize)
			{
				assert(curr_socketptr != socketptr);	/* since not create new socket */
				if (0 < delimiter_len)		/* new delimiters */
					iosocket_delimiter((unsigned char *)NULL, 0, curr_socketptr, TRUE);
				free(curr_socketptr);		/* SOCKET_FREE would free storage used by existing socket */
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_SOCKBFNOTEMPTY, 2, bfsize, socketptr->buffered_length);
			}
			curr_socketptr->buffer = (char *)malloc(bfsize);
			if (0 < socketptr->buffered_length)
			{
				assert(curr_socketptr != socketptr);	/* since not create new socket */
				memcpy(curr_socketptr->buffer, socketptr->buffer + socketptr->buffered_offset,
				       socketptr->buffered_length);
				curr_socketptr->buffered_offset = 0;
			}
		}
	}
	/* -------------------------------------- action -------------------------------------------- */
	if ((listen_specified && ((!iosocket_bind(curr_socketptr, NO_M_TIMEOUT, ibfsize_specified, FALSE))
			|| (!iosocket_listen_sock(curr_socketptr, DEFAULT_LISTEN_DEPTH))))
		|| (connect_specified && (!iosocket_connect(curr_socketptr, 0, ibfsize_specified))))
	{	/* error message should be printed from bind/connect and SOCKET_FREE */
		assert(curr_socketptr == socketptr);	/* since create new socket */
		REVERT_GTMIO_CH(&iod->pair, ch_set);
		return;
	}
	/* ------------------------------------ commit changes -------------------------------------- */
	if (create_new_socket)
	{
		assert(curr_socketptr == socketptr);	/* since create new socket */
		/* a new socket is created. so add to the list */
		curr_socketptr->dev = dsocketptr;
		newdsocket->socket[newdsocket->n_socket++] = socketptr;
		newdsocket->current_socket = newdsocket->n_socket - 1;
	}
	else
	{
		if (NULL != free_ozff)
			free(free_ozff);
		if (socketptr->buffer_size != curr_socketptr->buffer_size)
			free(socketptr->buffer);
		/* no need to free socketptr delimiters since already done if new ones specified */
		memcpy(socketptr, curr_socketptr, sizeof(socket_struct));
		free(curr_socketptr);		/* SOCKET_FREE would free storage used by existing socket */
	}
	memcpy(dsocketptr, newdsocket, d_socket_struct_len);
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
