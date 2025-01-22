/****************************************************************
 *								*
 * Copyright (c) 2014-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#ifdef GTM_TLS

#include <errno.h>
#include "gtm_poll.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "eintr_wrappers.h"

#include "io.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "gtm_conv.h"
#include "gtmio.h"
#include "gtm_utf8.h"
#include "rel_quant.h"
#include "send_msg.h"
#include "error.h"
#include "min_max.h"
#include "gtm_caseconv.h"
#include "fgncalsp.h"
#include "iotimer.h"
#include "gtm_tls.h"

GBLREF io_pair			io_curr_device;
GBLREF char			dl_err[MAX_ERRSTR_LEN];
GBLREF int			dollar_truth;
GBLREF void 			(*primary_exit_handler)(void);

GBLREF	gtm_tls_ctx_t		*tls_ctx;

error_def(ERR_CURRSOCKOFR);
error_def(ERR_TLSCONVSOCK);
error_def(ERR_TLSDLLNOOPEN);
error_def(ERR_TLSHANDSHAKE);
error_def(ERR_TLSINIT);
error_def(ERR_TLSPARAM);
error_def(ERR_TLSRENEGOTIATE);
error_def(ERR_NOSOCKETINDEV);
error_def(ERR_SOCKPASSDATAMIX);
error_def(ERR_TEXT);
error_def(ERR_ZINTRECURSEIO);
error_def(ERR_GETSOCKOPTERR);
error_def(ERR_SETSOCKOPTERR);


#define	MAX_TLSOPTION	12
#define TLSLABEL	"tls: { "
#define COLONBRACKET	": { "
#define BRACKETSSEMIS	" }; };"
#define RENEGOTIATE	"RENEGOTIATE"
#define FLUSH_OUTPUT_ERROR	"unable to flush pending output"
#define UNREAD_INPUT	"unread input"

typedef enum
{
	tlsopt_invalid,
	tlsopt_client,
	tlsopt_server,
	tlsopt_renegotiate
} tls_option;

void	iosocket_tls(mval *optionmval, int4 msec_timeout, mval *tlsid, mval *password, mval *extraarg)
{
	int4			devlen, extrastr_len, flags, len, length, save_errno, status, status2, timeout, tls_errno;
	int4			errlen, errlen2;
	int			fcntl_flags, fcntl_res;
	io_desc			*iod;
	d_socket_struct 	*dsocketptr;
	socket_struct		*socketptr;
	char			idstr[MAX_TLSID_LEN + SIZEOF(RENEGOTIATE) + 1], optionstr[MAX_TLSOPTION];
	char			passwordstr[GTM_PASSPHRASE_MAX_ASCII + 1];
	const char		*errp, *errp2;
	char			*charptr, *extraptr, *extrastr, *errptr;
	tls_option		option;
	gtm_tls_socket_t	*tlssocket;
	ABS_TIME		cur_time, end_time;
	struct pollfd		fds;
	boolean_t		ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	iod = io_curr_device.out;
	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	if (0 >= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSOCKETINDEV);
		return;
	}
	if (dsocketptr->n_socket <= dsocketptr->current_socket)
	{
		assert(FALSE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return;
	}
	if (dsocketptr->mupintr)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZINTRECURSEIO);
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
	ENSURE_DATA_SOCKET(socketptr);
	if (socket_tcpip != socketptr->protocol)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_LIT("/TLS"),
			LEN_AND_LIT("but socket is not TCP"));
		return;
	}
	if (socket_connected != socketptr->state)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_LIT("/TLS"),
			LEN_AND_LIT("but socket not connected"));
		return;
	}
	if (NULL != tlsid)
	{
		length = tlsid->str.len;
		if (MAX_TLSID_LEN < (length + 1))	/* for null */
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_LIT("TLSID"), LEN_AND_LIT("too long"));
			return;
		}
		STRNCPY_STR(idstr, tlsid->str.addr, length);
		idstr[length] = '\0';
	} else
		idstr[0] = '\0';
	if (('\0' != idstr[0]) && (NULL != password))
	{	/* password only usable if tlsid provided - iosocket_iocontrol checks */
		length = password->str.len;
		if (GTM_PASSPHRASE_MAX_ASCII < length)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_LIT("passphrase"),
				LEN_AND_LIT("too long"));
			return;
		}
		STRNCPY_STR(passwordstr, password->str.addr, length);
		passwordstr[length] = '\0';
	} else if (NULL != password)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_LIT("passphrase"), LEN_AND_LIT("requires TLSID"));
		return;
	} else
		passwordstr[0] = '\0';
	length = MIN((SIZEOF(optionstr) - 1), optionmval->str.len);
	lower_to_upper((uchar_ptr_t)optionstr, (uchar_ptr_t)optionmval->str.addr, length);
	optionstr[length] = '\0';
	if (0 == memcmp(optionstr, "CLIENT", MIN(length, STRLEN("CLIENT"))))
		option = tlsopt_client;
	else if (0 == memcmp(optionstr, "SERVER", MIN(length, STRLEN("SERVER"))))
		option = tlsopt_server;
	else if (0 == memcmp(optionstr, "RENEGOTIATE", length))
		option = tlsopt_renegotiate;
	else
		option = tlsopt_invalid;
	memcpy(iod->dollar.device, "0", SIZEOF("0"));
	if (NO_M_TIMEOUT != (msec_timeout * (uint8)NANOSECS_IN_MSEC))
	{
		sys_get_curr_time(&cur_time);
		add_int_to_abs_time(&cur_time, msec_timeout, &end_time);
	}
	if ((tlsopt_client == option) || (tlsopt_server == option))
	{	/* most of the setup is common */
		if (socketptr->tlsenabled)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_STR(optionstr),
				LEN_AND_LIT("but TLS already enabled"));
			return;
		}
		assertpro((0 >= socketptr->buffered_length) && (0 >= socketptr->obuffer_length));
		if (NULL == tls_ctx)
		{	/* first use of TLS in process */
			if (-1 == gtm_tls_loadlibrary())
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSDLLNOOPEN, 0, ERR_TEXT, 2, LEN_AND_STR(dl_err));
				return;
			}
			if (NULL == (tls_ctx = (gtm_tls_init(GTM_TLS_API_VERSION, GTMTLS_OP_INTERACTIVE_MODE))))
			{
				errp = gtm_tls_get_error(NULL);
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, errp, errlen);
				if (socketptr->ioerror)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_TLSINIT, 0, ERR_TEXT, 2, LEN_AND_STR(errp));
				if (NO_M_TIMEOUT != (msec_timeout * (uint8)NANOSECS_IN_MSEC))
					dollar_truth = FALSE;
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return;
			}
			if (primary_exit_handler)
				atexit(primary_exit_handler);
		}
		socketptr->tlsenabled = TRUE;
		flags = GTMTLS_OP_SOCKET_DEV | ((tlsopt_client == option) ? GTMTLS_OP_CLIENT_MODE : 0);
		if (NULL != extraarg)
		{
			if ('\0' == idstr[0])
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_STR(optionstr),
					LEN_AND_LIT("TLSID required for configuration string"));
				return;
			}
			length = extraarg->str.len;
			extrastr = malloc(length + 1 + SIZEOF(TLSLABEL) - 1 + tlsid->str.len + SIZEOF(COLONBRACKET) - 1
				+ SIZEOF(BRACKETSSEMIS) - 1);
			MEMCPY_LIT(extrastr, TLSLABEL);
			extraptr = extrastr + SIZEOF(TLSLABEL) - 1;
			memcpy(extraptr, tlsid->str.addr, tlsid->str.len);
			extraptr += tlsid->str.len;
			MEMCPY_LIT(extraptr, COLONBRACKET);
			extraptr = extraptr + SIZEOF(COLONBRACKET) - 1;
			STRNCPY_STR(extraptr, extraarg->str.addr, length);
			extraptr += length;
			MEMCPY_LIT(extraptr, BRACKETSSEMIS);
			extraptr += SIZEOF(BRACKETSSEMIS) - 1;
			*extraptr = '\0';
			if (0 > gtm_tls_add_config(tls_ctx, idstr, extrastr))
			{	/* error string available */
				socketptr->tlsenabled = FALSE;
				free(extrastr);
				errp = gtm_tls_get_error(NULL);
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, errp, errlen);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSCONVSOCK, 0, ERR_TEXT, 2, errlen, errp);
				return;
			} else
				free(extrastr);
		}
		if ('\0' != passwordstr[0])
		{
			if ('\0' == idstr[0])
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_STR(optionstr),
					LEN_AND_LIT("TLSID required for passphrase"));
				return;
			}
			if (0 > gtm_tls_store_passwd(tls_ctx, idstr, passwordstr))
			{	/* error string available */
				socketptr->tlsenabled = FALSE;
				errp = gtm_tls_get_error(NULL);
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, errp, errlen);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSCONVSOCK, 0, ERR_TEXT, 2, errlen, errp);
				return;
			}
		}
		if (socketptr->nonblocked_output)
		{
			if (GTM_TLS_API_VERSION_NONBLOCK > tls_ctx->plugin_version)
			{	/* Due to lack of gtm_tls_version(), old pre GTM_TLS_API_VERSION_NONBLOCK plugins will not even
				 * load but include this test as an example of how to check if the plugin loaded is new enough */
				socketptr->tlsenabled = FALSE;
				errp = gtm_tls_get_error(NULL);
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, errp, errlen);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSCONVSOCK, 0, ERR_TEXT, 2, errlen, errp);
				return;
			}
			flags |= GTMTLS_OP_NONBLOCK_WRITE;
			if (!socketptr->nonblocking)
			{	/* set O_NONBLOCK if needed before calling plugin */
				FCNTL2(socketptr->sd, F_GETFL, fcntl_flags);
				if (fcntl_flags < 0)
				{
					iod->dollar.za = 9;
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_GETSOCKOPTERR, 5,
						LEN_AND_LIT("F_GETFL FOR NON BLOCKING WRITE"),
						save_errno, LEN_AND_STR(errptr));
					return;
				}
				FCNTL3(socketptr->sd, F_SETFL, fcntl_flags | (O_NDELAY | O_NONBLOCK), fcntl_res);
				if (fcntl_res < 0)
				{
					iod->dollar.za = 9;
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
						LEN_AND_LIT("F_SETFL FOR NON BLOCKING WRITE"),
						save_errno, LEN_AND_STR(errptr));
					return;
				}
				socketptr->nonblocking = TRUE;
#				ifdef	DEBUG
				if (WBTEST_ENABLED(WBTEST_SOCKET_NONBLOCK) && (0 < ydb_white_box_test_case_count))
				{
					if (-1 == setsockopt(socketptr->sd, SOL_SOCKET, SO_SNDBUF,
						&ydb_white_box_test_case_count, SIZEOF(ydb_white_box_test_case_count)))
					{
						save_errno = errno;
						errptr = (char *)STRERROR(save_errno);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
							LEN_AND_LIT("SO_SNDBUF"), save_errno, LEN_AND_STR(errptr));
						return;
					}
                                }
#				endif
			}
		}
		socketptr->tlssocket = gtm_tls_socket(tls_ctx, NULL, socketptr->sd, idstr, flags);
		if (NULL == socketptr->tlssocket)
		{
			socketptr->tlsenabled = FALSE;
			errp = gtm_tls_get_error(NULL);
			SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, errp, errlen);
			if (socketptr->ioerror)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_TLSCONVSOCK, 0, ERR_TEXT, 2, LEN_AND_STR(errp));
			if (NO_M_TIMEOUT != (msec_timeout * (uint8)NANOSECS_IN_MSEC))
				dollar_truth = FALSE;
			REVERT_GTMIO_CH(&iod->pair, ch_set);
			return;
		}
		status = 0;
		do
		{
			if (0 != status)
			{
				fds.fd = socketptr->sd;
				fds.events = (GTMTLS_WANT_READ == status) ? POLLIN : POLLOUT;
<<<<<<< HEAD
				if (-1 == (status2 = poll(&fds, 1, (NO_M_TIMEOUT == (msec_timeout * (uint8)NANOSECS_IN_MSEC)) ? -1 : msec_timeout)))
=======
				if (-1 == (status2 = poll(&fds, 1, (NO_M_TIMEOUT == msec_timeout) ? -1 : msec_timeout)))
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
				{
					save_errno = errno;
					if (EAGAIN == save_errno)
					{
						rel_quant();	/* allow resources to become available - likely sb nanosleep */
						status2 = 0;	/* treat as timeout */
					} else if (EINTR == save_errno)
					{
						eintr_handling_check();
						status2 = 0;
					}
				}
				HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			} else
				status2 = 1;	/* do accept/connect first time */
			if (0 < status2)
			{
				if (tlsopt_server == option)
					status = gtm_tls_accept((gtm_tls_socket_t *)socketptr->tlssocket);
				else
					status = gtm_tls_connect((gtm_tls_socket_t *)socketptr->tlssocket);
			}
			if ((0 > status2) || ((status != 0) && ((GTMTLS_WANT_READ != status) && (GTMTLS_WANT_WRITE != status))))
			{
				if (0 != status)
				{
					tls_errno = gtm_tls_errno();
					if (0 > tls_errno)
						errp = gtm_tls_get_error((gtm_tls_socket_t *)socketptr->tlssocket);
					else
						errp = STRERROR(tls_errno);
				} else
					errp = STRERROR(save_errno);
				socketptr->tlsenabled = FALSE;
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, errp, errlen);
				if (socketptr->ioerror)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_TLSHANDSHAKE, 0,
						      ERR_TEXT, 2, LEN_AND_STR(errp));
				if (NO_M_TIMEOUT != (msec_timeout * (uint8)NANOSECS_IN_MSEC))
					dollar_truth = FALSE;
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return;
			}
			if ((0 != status) && (0 <= status2))	/* not accepted/connected and not error */
			{	/* check for timeout if not error or want read or write */
				if ((0 != msec_timeout) && (NO_M_TIMEOUT != (msec_timeout * (uint8)NANOSECS_IN_MSEC)))
				{
					sys_get_curr_time(&cur_time);
					cur_time = sub_abs_time(&end_time, &cur_time);
					if (0 >= cur_time.tv_sec)
					{	/* no more time */
						gtm_tls_session_close((gtm_tls_socket_t **)&socketptr->tlssocket);
						socketptr->tlsenabled = FALSE;
						dollar_truth = FALSE;
						REVERT_GTMIO_CH(&iod->pair, ch_set);
						return;
					} else
<<<<<<< HEAD
					{	/* adjust msec_timeout for poll/select */
						msec_timeout = (cur_time.tv_sec * MILLISECS_IN_SEC) +
								DIVIDE_ROUND_UP(cur_time.tv_nsec, NANOSECS_IN_MSEC);
=======
					{	/* adjust msec_timeout for poll */
						msec_timeout = (cur_time.at_sec * MILLISECS_IN_SEC) +
							DIVIDE_ROUND_UP(cur_time.at_usec, MICROSECS_IN_MSEC);
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
					}
				} else if (0 == msec_timeout)
				{	/* only one chance */
					gtm_tls_session_close((gtm_tls_socket_t **)&socketptr->tlssocket);
					socketptr->tlsenabled = FALSE;
					dollar_truth = FALSE;
					REVERT_GTMIO_CH(&iod->pair, ch_set);
					return;
				}
				continue;
			}
		} while ((GTMTLS_WANT_READ == status) || (GTMTLS_WANT_WRITE == status));
		/* turn on output buffering */
		SOCKET_OBUFFER_INIT(socketptr, socketptr->buffer_size, DEFAULT_WRITE_WAIT, (DEFAULT_WRITE_WAIT * 2));
		socketptr->obuffer_in_use = TRUE;
	} else if (tlsopt_renegotiate == option)
	{
		if (!socketptr->tlsenabled)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_STR(optionstr),
				LEN_AND_LIT("but TLS not enabled"));
			return;		/* make compiler and analyzers happy */
		}
		tlssocket = (gtm_tls_socket_t *)socketptr->tlssocket;
		if (GTMTLS_OP_CLIENT_MODE & tlssocket->flags)
		{	/* only server can renegotiate */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_STR(optionstr),
				RTS_ERROR_LITERAL("not allowed when client"));
			return;
		}
		if ((0 < socketptr->buffered_length) || (0 < gtm_tls_cachedbytes((gtm_tls_socket_t *)socketptr->tlssocket)))
		{	/* if anything in input buffer or ready to be read then error */
			errp = UNREAD_INPUT;
			SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, errp, errlen);
			if (socketptr->ioerror)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_TLSRENEGOTIATE, 0,	ERR_TEXT, 2, LEN_AND_STR(errp));
			if (NO_M_TIMEOUT != (msec_timeout * (uint8)NANOSECS_IN_MSEC))
				dollar_truth = FALSE;
			REVERT_GTMIO_CH(&iod->pair, ch_set);
			return;
		}
		if (0 < socketptr->obuffer_length)
		{	/* flush pending output */
			iosocket_flush(iod);
			if (0 < socketptr->obuffer_length)
			{ /* get obuffer_error as additional ERR_TEXT */
				errp = FLUSH_OUTPUT_ERROR;
				if (-1 == socketptr->obuffer_errno)
					errp2 = gtm_tls_get_error((gtm_tls_socket_t *)socketptr->tlssocket);
				else
					errp2 = (char *)STRERROR(socketptr->obuffer_errno);
				SET_DOLLARDEVICE_ONECOMMA_ERRSTR1_ERRSTR2(iod, errp, errlen, errp2, errlen2);
				if (socketptr->ioerror)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(10) ERR_TLSRENEGOTIATE, 0,
						      ERR_TEXT, 2, LEN_AND_STR(errp), ERR_TEXT, 2, LEN_AND_STR(errp2));
				if (NO_M_TIMEOUT != (msec_timeout * (uint8)NANOSECS_IN_MSEC))
					dollar_truth = FALSE;
				REVERT_GTMIO_CH(&iod->pair, ch_set);
				return;
			}
		}
		if (NULL != extraarg)
		{
			length = extraarg->str.len;
			if ('\0' == idstr[0])
			{	/* create new section from initial tlsid -renegotiate */
				charptr = ((gtm_tls_socket_t *)socketptr->tlssocket)->tlsid;
				assert(MAX_TLSID_LEN >= STRLEN(charptr));
				len = SNPRINTF(idstr, SIZEOF(idstr), "%s-" RENEGOTIATE, charptr);
				assert(SIZEOF(idstr) > len);
				if (len >= SIZEOF(idstr))
					len = ((int4) SIZEOF(idstr)) - 1;
				idstr[len] = '\0';
			} else
				len = STRLEN(idstr);
			extrastr = malloc(length + 1 + STR_LIT_LEN(TLSLABEL) + len + STR_LIT_LEN(COLONBRACKET)
				+ STR_LIT_LEN(BRACKETSSEMIS));
			extrastr_len = length + 1 + STR_LIT_LEN(TLSLABEL) + len + STR_LIT_LEN(COLONBRACKET)
				+ STR_LIT_LEN(BRACKETSSEMIS);
			len = SNPRINTF(extrastr, extrastr_len, TLSLABEL "%s" COLONBRACKET "%.*s" BRACKETSSEMIS, idstr,
					extraarg->str.len, extraarg->str.addr);
			assert(len == extrastr_len - 1);	/* snprintf excludes null terminator in return value */
		} else
			extrastr = NULL;
		status = gtm_tls_renegotiate_options((gtm_tls_socket_t *)socketptr->tlssocket, msec_timeout, idstr,
						     extrastr, (NULL != tlsid));
		if (NULL != extrastr)
			free(extrastr);
		if (0 != status)
		{
			tls_errno = gtm_tls_errno();
			if (0 > tls_errno)
				errp = gtm_tls_get_error((gtm_tls_socket_t *)socketptr->tlssocket);
			else
				errp = STRERROR(tls_errno);
			SET_DOLLARDEVICE_ONECOMMA_ERRSTR(iod, errp, errlen);
			if (socketptr->ioerror)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_TLSRENEGOTIATE, 0,
					      ERR_TEXT, 2, LEN_AND_STR(errp));
			if (NO_M_TIMEOUT != (msec_timeout * (uint8)NANOSECS_IN_MSEC))
				dollar_truth = FALSE;
			REVERT_GTMIO_CH(&iod->pair, ch_set);
			return;
		}
	} else
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_TLSPARAM, 4, LEN_AND_STR(optionstr),
			LEN_AND_LIT("not a valid option"));
	if (NO_M_TIMEOUT != (msec_timeout * (uint8)NANOSECS_IN_MSEC))
		dollar_truth = TRUE;
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
#endif
