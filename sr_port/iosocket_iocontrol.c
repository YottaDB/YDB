/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_iocontrol.c */

#include "mdef.h"
#include "mvalconv.h"

#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "iott_setterm.h"
#include "gtm_caseconv.h"
#include "stringpool.h"
#include "min_max.h"
#include "error.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "jnl.h"
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "getzposition.h"
#include "is_equ.h"		/* for MV_FORCE_NSTIMEOUT macro */
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#include "wbox_test_init.h"
#endif

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;
GBLREF int		ydb_non_blocked_write_retries;	/* number of retries for non_blocked write to socket */

LITREF	mval		literal_notimeout;
LITREF	mval		skiparg;

error_def(ERR_EXPR);
error_def(ERR_INVCTLMNE);
error_def(ERR_CURRSOCKOFR);
error_def(ERR_NOSOCKETINDEV);
error_def(ERR_SOCKBLOCKERR);
error_def(ERR_ZINTRECURSEIO);
error_def(ERR_GETSOCKOPTERR);
error_def(ERR_SETSOCKOPTERR);


/* for iosocket_dlr_zkey */
#define FORMATTIMESTR	"FORMAT"
#define LISTENING	"LISTENING|"
#define READ		"READ|"
#define WRITE		"WRITE|"
#define MAXEVENTLITLEN	(SIZEOF(LISTENING)-1)
#define MAXZKEYITEMLEN	(MAX_HANDLE_LEN + SA_MAXLITLEN + MAXEVENTLITLEN + 2)	/* 1 pipe and a semicolon */

void	iosocket_block_iocontrol(io_desc *iod, mval *option, mval *returnarg);

void	iosocket_iocontrol(mstr *mn, int4 argcnt, va_list args)
{
	char		action[MAX_DEVCTL_LENGTH];
	unsigned short 	depth;
	int		length, n;
	uint8		nsec_timeout;
	pid_t		pid;
	mval		*arg, *handlesvar = NULL, *option, *returnarg = NULL;
	mval		*whatop = NULL, *handle = NULL;
#ifdef	GTM_TLS
	mval		*tlsid, *password, *extraarg;
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 == mn->len)
		return;
	assert(MAX_DEVCTL_LENGTH > mn->len);
	lower_to_upper((uchar_ptr_t)action, (uchar_ptr_t)mn->addr, mn->len);
	length = mn->len;
	action[length] = '\0';	/* needed for "STRCMP" below */
	if (0 == STRCMP(action, "LISTEN"))
	{
		if (1 > argcnt)
			depth = DEFAULT_LISTEN_DEPTH;
		else
		{
			arg = va_arg(args, mval *);
			if ((NULL == arg) || M_ARG_SKIPPED(arg) || !MV_DEFINED(arg))
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXPR);
				return;
			}
			depth = MV_FORCE_INTD(arg); /* Negative values are downcast to unsigned short */
		}
		iosocket_listen(io_curr_device.in, depth);
	} else if (0 == STRCMP(action, "WAIT"))
	{
		arg = (0 < argcnt) ? va_arg(args, mval *) : (mval *)&literal_notimeout;
		if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
			MV_FORCE_NSTIMEOUT(arg, nsec_timeout, "/WAIT");
		else
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXPR);
			return;
		}
		if (2 <= argcnt)
		{	/* what operation to check */
			arg = va_arg(args, mval *);
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				whatop = arg;
		}
		if (3 <= argcnt)
		{	/* which handle aka socket to check */
			arg = va_arg(args, mval *);
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				handle = arg;
		}
		iosocket_wait(io_curr_device.in, nsec_timeout, whatop, handle);
	} else if (0 == STRCMP(action, "PASS"))
	{
		n = argcnt;
		if (1 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				pid = MV_FORCE_INTD(arg); /* Negative values are excluded */
			else
				pid = -1;
		} else
			pid = -1;
		if (2 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL == arg) || M_ARG_SKIPPED(arg) || !MV_DEFINED(arg))
				arg = (mval *)&literal_notimeout;
		} else
			arg = (mval *)&literal_notimeout;
		MV_FORCE_NSTIMEOUT(arg, nsec_timeout, "/PASS");
		iosocket_pass_local(io_curr_device.out, pid, nsec_timeout, n, args);
	} else if (0 == STRCMP(action, "ACCEPT"))
	{
		n = argcnt;
		if (1 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL != arg) && !M_ARG_SKIPPED(arg))
				handlesvar = arg;
		}
		if (2 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL != arg) && !M_ARG_SKIPPED(arg) && MV_DEFINED(arg))
				pid = MV_FORCE_INTD(arg); /* Negative values are excluded */
			else
				pid = -1;
		} else
			pid = -1;
		if (3 <= argcnt)
		{
			arg = va_arg(args, mval *);
			n--;
			if ((NULL == arg) || M_ARG_SKIPPED(arg) || !MV_DEFINED(arg))
				arg = (mval *)&literal_notimeout;
		} else
			arg = (mval *)&literal_notimeout;
		MV_FORCE_NSTIMEOUT(arg, nsec_timeout, "/ACCEPT");
		iosocket_accept_local(io_curr_device.in, handlesvar, pid, nsec_timeout, n, args);
#ifdef	GTM_TLS
	} else if (0 == STRCMP(action, "TLS"))
	{	/*	WRITE /TLS(option[,[nsec_timeout][,tlsid[,password]]]) */
		if (1 <= argcnt)
		{
			option = va_arg(args, mval *);
			if ((NULL != option) && !M_ARG_SKIPPED(option) && MV_DEFINED(option))
				MV_FORCE_STRD(option);
			else
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXPR);
				return;
			}
		} else
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXPR);
			return;
		}
		if (2 <= argcnt)
		{
			arg = va_arg(args, mval *);
			if ((NULL == arg) || M_ARG_SKIPPED(arg) || !MV_DEFINED(arg))
				arg = (mval *)&literal_notimeout;
		} else
			arg = (mval *)&literal_notimeout;
		MV_FORCE_NSTIMEOUT(arg, nsec_timeout, "/TLS");
		if (3 <= argcnt)
		{
			tlsid = va_arg(args, mval *);
			if ((NULL != tlsid) && !M_ARG_SKIPPED(tlsid) && MV_DEFINED(tlsid))
				MV_FORCE_STRD(tlsid);
			else
				tlsid = NULL;
		} else
			tlsid = NULL;
		if (4 <= argcnt)
		{	/* password only valid if tlsid provided */
			password = va_arg(args, mval *);	/* need to do va_arg in case 5th arg */
			if ((NULL != tlsid) && (NULL != password) && !M_ARG_SKIPPED(password) && MV_DEFINED(password))
				MV_FORCE_STRD(password);
			else
				password = NULL;
		} else
			password = NULL;
		if (5 <= argcnt)
		{
			extraarg = va_arg(args, mval *);
			if ((NULL != extraarg) && !M_ARG_SKIPPED(extraarg) && MV_DEFINED(extraarg))
				MV_FORCE_STRD(extraarg);
			else
				extraarg = NULL;
		} else
			extraarg = NULL;
		assert(option);
		iosocket_tls(option, (int)(nsec_timeout / NANOSECS_IN_MSEC), tlsid, password, extraarg);
#	endif
	} else if (0 == memcmp(action, "BLOCK", length))
	{	/* OFF, CLEAR, COUNT .lvn, SENT .lvn */
		if (1 <= argcnt)
		{
			option = va_arg(args, mval *);
			if ((NULL != option) && !M_ARG_SKIPPED(option) && MV_DEFINED(option))
				MV_FORCE_STRD(option);
			else
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_EXPR);
		} else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_SOCKBLOCKERR, 2,
					LEN_AND_LIT("at least one option must be provided"));
		if (2 <= argcnt)
		{
			arg = va_arg(args, mval *);
			if ((NULL != arg) && !M_ARG_SKIPPED(arg))
				returnarg = arg;
		}
		iosocket_block_iocontrol(io_curr_device.out, option, returnarg);
	} else
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_INVCTLMNE);

	return;
}

void	iosocket_dlr_device(mstr *d)
{
	io_desc		*iod;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	iod = io_curr_device.in;
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
	PUT_DOLLAR_DEVICE_INTO_MSTR(iod, d);
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}

void	iosocket_dlr_key(mstr *d)
{
	io_desc		*iod;
	int		len;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	iod = io_curr_device.in;
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
	len = STRLEN(iod->dollar.key);
	/* verify internal buffer has enough space for $DEVICE string value */
	assert((int)d->len > len);
	if (len > 0)
		memcpy(d->addr, iod->dollar.key, len);
	d->len = len;
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}

void iosocket_dlr_zkey(mstr *d)
{
	int4		ii;
	int		len, thislen, totlen, totplusthislen;
	char		*zkeyptr, *charptr;
	mval		*dzkey = NULL;
	io_desc		*iod;
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	iod = io_curr_device.in;
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
	assertpro(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	zkeyptr = (char *)stringpool.free;
	totlen = 0;
	for (ii = 0; ii < dsocketptr->n_socket; ii++)
	{
		socketptr = dsocketptr->socket[ii];
		if ((socket_listening != socketptr->state) && (socket_connected != socketptr->state))
			continue;
		if ((socket_connected == socketptr->state) && (0 < socketptr->buffered_length))
		{	/* data to be read in buffer */
			if (!(SOCKPEND_READ & socketptr->pendingevent))
			{	/* may have been cleared by partial READ */
				socketptr->pendingevent |= SOCKPEND_READ;
				socketptr->readyforwhat |= SOCKREADY_READ;
				socketptr->readycycle = dsocketptr->waitcycle;
			}
		}
		if (socketptr->pendingevent)
		{
			thislen = len = 0;
			totplusthislen = totlen + MAXZKEYITEMLEN;
			if ((socket_connected == socketptr->state) &&
				(socketptr->readyforwhat == (SOCKREADY_WRITE | SOCKREADY_READ)))
				totplusthislen += MAXZKEYITEMLEN;	/* both READ and WRITE ready */
			if (!IS_STP_SPACE_AVAILABLE(totplusthislen))
			{
				if (totlen)
				{
					assert(NULL == dzkey);
					/* Temp MVAL to protect $zkey contents at the end of string pool from garbage collection */
					PUSH_MV_STENT(MVST_MVAL);
					dzkey = &mv_chain->mv_st_cont.mvs_mval;
					dzkey->mvtype = MV_STR;
					dzkey->str.len = totlen;
					dzkey->str.addr = (char *)stringpool.free;
					/* Advance stringpool free so that the contents are inside of it */
					stringpool.free += totlen;
				}
				INVOKE_STP_GCOL(totplusthislen);
				if (totlen)
				{
					assert(IS_IN_STRINGPOOL(dzkey->str.addr, totlen));
					if (!IS_AT_END_OF_STRINGPOOL(dzkey->str.addr, totlen))
					{	/* Move to top */
						memcpy(stringpool.free, dzkey->str.addr, totlen);
					} else
						stringpool.free -= totlen;	/* Backup over prior items */
					assert(dzkey);
					POP_MV_STENT(); /* Release temporary MVAL */
					dzkey = NULL;
				}
				zkeyptr = (char *)stringpool.free + totlen;
			}
			if (totlen)
			{	/* at least one item already */
				*zkeyptr++ = ';';
				totlen++;
			}
			/* add READ/WRITE/LISTENING|handle|remoteinfo;... */
			if (socket_listening == socketptr->state)
			{
				thislen = len = STR_LIT_LEN(LISTENING);
				memcpy(zkeyptr, LISTENING, len);
			} else if (socketptr->readyforwhat & SOCKREADY_READ)
			{
				thislen = len = STR_LIT_LEN(READ);
				memcpy(zkeyptr, READ, len);
			} else if (socketptr->readyforwhat & SOCKREADY_WRITE)
			{	/* only WRITE ready */
				thislen = len = STR_LIT_LEN(WRITE);
				memcpy(zkeyptr, WRITE, len);
			}
			zkeyptr += len;
			thislen += len;
			totlen += len;
			memcpy(zkeyptr, socketptr->handle, socketptr->handle_len);
			zkeyptr += socketptr->handle_len;
			*zkeyptr++ = '|';
			thislen += (socketptr->handle_len + 1);
			totlen += (socketptr->handle_len + 1);
			if (socket_local != socketptr->protocol)
			{
				if (socket_listening == socketptr->state)
					len = SNPRINTF(zkeyptr, MAXZKEYITEMLEN - thislen, "%d", socketptr->local.port);
				else
				{
					if (NULL != socketptr->remote.saddr_ip)
					{
						len = STRLEN(socketptr->remote.saddr_ip);
						memcpy(zkeyptr, socketptr->remote.saddr_ip, len);
					} else
						len = 0;
				}
				charptr = NULL;
#			ifndef VMS
			} else
			{
				if (NULL != socketptr->local.sa)
					charptr = ((struct sockaddr_un *)(socketptr->local.sa))->sun_path;
				else if (NULL != socketptr->remote.sa)
					charptr = ((struct sockaddr_un *)(socketptr->remote.sa))->sun_path;
				else
					charptr = "";
				len = STRLEN(charptr);
				len = MIN(len, (MAXZKEYITEMLEN - thislen));
				memcpy(zkeyptr, charptr, len);
#			endif
			}
			zkeyptr += len;
			totlen += len;
			if (socketptr->nonblocked_output && (socketptr->readyforwhat == (SOCKREADY_READ | SOCKREADY_WRITE)))
			{	/* already have READ now add WRITE item */
				assert(totplusthislen >= (totlen + MAXZKEYITEMLEN));
				*zkeyptr++ = ';';
				totlen++;
				thislen = len = STR_LIT_LEN(WRITE);
				memcpy(zkeyptr, WRITE, len);
				zkeyptr += len;
				thislen += len;
				totlen += len;
				memcpy(zkeyptr, socketptr->handle, socketptr->handle_len);
				zkeyptr += socketptr->handle_len;
				*zkeyptr++ = '|';
				thislen += (socketptr->handle_len + 1);
				totlen += (socketptr->handle_len + 1);
				if (socket_local != socketptr->protocol)
				{
					if (NULL != socketptr->remote.saddr_ip)
					{
						len = STRLEN(socketptr->remote.saddr_ip);
						memcpy(zkeyptr, socketptr->remote.saddr_ip, len);
					} else
						len = 0;
				} else
				{
					assert(charptr);
					len = STRLEN(charptr);		/* same as for READ */
					len = MIN(len, (MAXZKEYITEMLEN - thislen));
					memcpy(zkeyptr, charptr, len);
				}
				zkeyptr += len;
				totlen += len;
			}
		}
	}
	d->addr = (char *)stringpool.free;
	d->len = totlen;
	stringpool.free += totlen;
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}

#define	BLOCKOFF		"OFF"
#define	BLOCKCLEAR		"CLEAR"
#define	BLOCKCOUNT		"COUNT"
#define	BLOCKSENT		"SENT"
#define MAX_BLOCK_OPTION	5

void  iosocket_block_iocontrol(io_desc *iod, mval *option, mval *returnarg)
{
	int		tmpint, save_errno;
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	boolean_t	ch_set;
	char		option_buf[MAX_BLOCK_OPTION + 1], *errptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
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
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
		return;
	}
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	if (NULL != option)
	{
		int	len;

		len = MIN(SIZEOF(option_buf) - 1, option->str.len);
		lower_to_upper((uchar_ptr_t)option_buf, (uchar_ptr_t)(option->str.addr), len);
		option_buf[len] = '\0';	/* null terminate so we can later use STRCMP */
		if (0 == STRCMP(option_buf, BLOCKOFF))
		{	/* check if not already set or TLS already enabled */
			if (socketptr->nonblocked_output)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBLOCKERR, 2, LEN_AND_LIT("already non blocking"));
				return;
			}
			if (socket_connected != socketptr->state)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBLOCKERR, 2, LEN_AND_LIT("must be connected"));
				return;
			}
			if (socketptr->tlsenabled)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBLOCKERR,
					2, LEN_AND_LIT("TLS enabled before non blocking"));
				return;
			}
			if (NULL != returnarg)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBLOCKERR,
					2, LEN_AND_LIT("OFF does not take an argument"));
				return;
			}
			socketptr->nonblocked_output = TRUE;
			socketptr->max_output_retries = ydb_non_blocked_write_retries;
			socketptr->args_written = socketptr->lastarg_size = socketptr->lastarg_sent = 0;
			SOCKET_OBUFFER_INIT(socketptr, socketptr->buffer_size, 0, 0);
			socketptr->obuffer_in_use = FALSE;	/* only used if actually blocked or TLS */
#ifdef	DEBUG
			if (WBTEST_ENABLED(WBTEST_SOCKET_NONBLOCK) && (0 < ydb_white_box_test_case_count))
			{
				if (-1 == setsockopt(socketptr->sd, SOL_SOCKET, SO_SNDBUF,
					&ydb_white_box_test_case_count, SIZEOF(ydb_white_box_test_case_count)))
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5, LEN_AND_LIT("SO_SNDBUF"),
						save_errno, LEN_AND_STR(errptr));
					return;
				}
			}
#endif
		} else if (0 == STRCMP(option_buf, BLOCKCLEAR))
		{
			if (socketptr->nonblocked_output)
			{
				if (NULL != returnarg)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBLOCKERR,
						2, LEN_AND_LIT("CLEAR does not take an argument"));
					return;
				}
				socketptr->args_written = socketptr->lastarg_size = socketptr->lastarg_sent = 0;
			}
		} else if (0 == STRCMP(option_buf, BLOCKCOUNT))
		{
			if (socketptr->nonblocked_output)
			{
				if (NULL == returnarg)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBLOCKERR,
							2, LEN_AND_LIT("COUNT requires local variable passed by reference"));
					return;
				}
				tmpint = (int)socketptr->args_written;
				MV_FORCE_UMVAL(returnarg, (unsigned int)tmpint);
			}
		} else if (0 == STRCMP(option_buf, BLOCKSENT))
		{
			if (socketptr->nonblocked_output)
			{
				if (NULL == returnarg)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBLOCKERR,
							2, LEN_AND_LIT("SENT requires local variable passed by reference"));
					return;
				}
				tmpint = (int)socketptr->lastarg_sent;
				MV_FORCE_MVAL(returnarg, tmpint);
			}
		} else
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SOCKBLOCKERR, 2, LEN_AND_LIT("unknown option"));
			return;
		}
	}
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
