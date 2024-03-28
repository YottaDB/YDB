/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_netdb.h"
#include "gtm_ipv6.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "io.h"
#include "iosocketdef.h"
#include "gtmio.h"

error_def(ERR_SETSOCKOPTERR);

/* iosocket_tcp_keepalive.c */
/*
 * inputs: socket pointer; the keepalive_opt value used for idle; the name of the calling action; a Boolean giving need for cleanup
 * returns: TRUE for success, FALSE for failure if ioerror=notrap otherwise rts_error on failures if freesocket
 * the callers use a positive non-zero value of keepalive_opt to specify keepalive; used here to specify TCP_KEEPIDLE
 * or to use values from the socket struct set via the OPTIONS device parameter if keepalive_opt is negative
 * TCP_KEEPCNT and TCP_KEEPINTVL can only be set via OPTIONS except as part of a white box case that sets a short time
 */
boolean_t iosocket_tcp_keepalive(socket_struct *socketptr, int keepalive_opt, char *act, boolean_t freesocket)
{
	boolean_t		trap;
	char			*errptr, *sockopt_errptr;
	d_socket_struct		*dsocketptr;
	int			keepidle_got, keepalive_value, keepidle_value;
	int			keepintvl_got, keepintvl_value, keepcnt_got, keepcnt_value;
	int4			errlen, real_errno;
#	ifdef DEBUG
	socklen_t		keepidle_got_len, keepintvl_got_len, keepcnt_got_len;
#	endif
	ssize_t			status;

	real_errno = 0;
	DEBUG_ONLY(keepidle_got_len = SIZEOF(keepidle_got);)
	if (0 > keepalive_opt)
	{	/* SOCKOPTIONS_FROM_STRUCT so use values from socket struct */
		if (SOCKOPTIONS_PENDING & socketptr->options_state.alive)
		{
			keepalive_value = socketptr->keepalive;
			socketptr->options_state.alive &= ~SOCKOPTIONS_PENDING;
		} else
			keepalive_value = -1;	/* flag to skip setsockopt */
		if (SOCKOPTIONS_PENDING & socketptr->options_state.idle)
		{
			keepidle_value = socketptr->keepidle;
			socketptr->options_state.idle &= ~SOCKOPTIONS_PENDING;
		} else
			keepidle_value = -1;	/* flag to skip */
		if (SOCKOPTIONS_PENDING & socketptr->options_state.cnt)
		{
			keepcnt_value = socketptr->keepcnt;
			socketptr->options_state.cnt &= ~SOCKOPTIONS_PENDING;
		} else
			keepcnt_value = -1;	/* flag to skip */
		if (SOCKOPTIONS_PENDING & socketptr->options_state.intvl)
		{
			keepintvl_value = socketptr->keepintvl;
			socketptr->options_state.intvl &= ~SOCKOPTIONS_PENDING;
		} else
			keepintvl_value = -1;	/* flag to skip */
	} else
	{
		keepalive_value = keepidle_value = keepalive_opt;	/* use environment variable for both */
		keepcnt_value = keepintvl_value = -1;	/* flag to skip setsockopt */
	}
#	ifdef DEBUG
#	ifndef DEBUG_SOCK
	if ((0 <= keepalive_value) && WBTEST_ENABLED(WBTEST_SOCKET_KEEPALIVE))
	{	/* skip if nothing to do with keepalive */
#	endif
		flush_pio();
		if (0 < keepalive_opt)
			printf("%s gtm_socket_keepalive_idle: %d\n", act, keepalive_opt);
		else	/* if only KEEPALIVE in OPTIONS, idle may be -1 */
			printf("%s USE :options=keepalive: %d, idle: %d\n", act, keepalive_value, keepidle_value);
		FFLUSH(stdout);
		printf("%s %ssetting SO_KEEPALIVE\n", act, (0 < keepalive_value ? "" : "un"));
		FFLUSH(stdout);
#	ifndef DEBUG_SOCK
	}
#	endif
#	endif
	if (0 <= keepalive_value)
	{
		if (-1 == setsockopt(socketptr->sd, SOL_SOCKET, SO_KEEPALIVE, &keepalive_value, SIZEOF(keepalive_value)))
		{
			real_errno = errno;
			sockopt_errptr = "SO_KEEPALIVE";
		}
	}
	if ((0 == real_errno) && (0 <= keepidle_value))
	{
		sockopt_errptr = "TCP_KEEPIDLE";	/* in case of error */
		if (-1 == (status = setsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle_value,
				SIZEOF(keepidle_value))))
			real_errno = errno;
#		ifdef DEBUG
		else if (-1 == (status = getsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle_got,
				&keepidle_got_len)))
			real_errno = errno;
		if (0 == status)
		{
			assert(keepidle_got == keepidle_value);
			real_errno = 0;
		}
#		endif
		PRO_ONLY(UNUSED(status));
	}
	if ((0 == real_errno) && (0 <= keepcnt_value))
	{
		if (-1 == setsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt_value, SIZEOF(keepcnt_value)))
		{
			sockopt_errptr = "TCP_KEEPCNT";
			real_errno = errno;
		}
	}
	if ((0 == real_errno) && (0 <= keepintvl_value))
	{
		if (-1 == setsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl_value, SIZEOF(keepintvl_value)))
		{
			sockopt_errptr = "TCP_KEEPINTVL";
			real_errno = errno;
		}
	}
	if (real_errno)
	{
		dsocketptr = socketptr->dev;
		errptr = (char *)STRERROR(real_errno);
		trap = socketptr->ioerror;
		SET_DOLLARDEVICE_ONECOMMA_ERRSTR(dsocketptr->iod, errptr, errlen);
#		ifdef DEBUG
#		ifndef DEBUG_SOCK
		if (WBTEST_ENABLED(WBTEST_SOCKET_KEEPALIVE))
		{
#		endif
			flush_pio();
			printf("fd: %d\n",socketptr->sd);
			FFLUSH(stdout);
#		ifndef DEBUG_SOCK
		}
#		endif
#		endif
		assert(FALSE);
		if (freesocket && trap)
		{
			if (FD_INVALID != socketptr->sd)
			{
				close(socketptr->sd);	/* Don't leave a dangling socket around */
				socketptr->sd = FD_INVALID;
			}
			SOCKET_FREE(socketptr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
			      LEN_AND_STR(sockopt_errptr), real_errno, errlen, errptr);
		}
		return FALSE;
	}
#	ifdef DEBUG
	if ((0 <= keepalive_opt) && WBTEST_ENABLED(WBTEST_SOCKET_KEEPALIVE))
	{	/* when implementing KEEPCNT and KEEPINTVL options, set to 2 to recognize missing peer in 6 seconds */
		keepcnt_value = keepintvl_value = 2;	/* for now force it in white box test */
		flush_pio();
		printf("%s setting TCP_KEEP options\n wb enabled: %d, wb #: %d. opt: %d\n",
		       act, ydb_white_box_test_case_enabled, ydb_white_box_test_case_number, keepcnt_value);
		FFLUSH(stdout);
		keepcnt_got_len = SIZEOF(keepcnt_got);
		if (-1 == setsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt_value, SIZEOF(keepcnt_value)))
		{
			real_errno = errno;
			DEBUG_ONLY(UNUSED(real_errno));
			assert(FALSE);		/* while a white box case, we can ignore errors */
		}
		if (-1 == getsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt_got, &keepcnt_got_len))
		{
			real_errno = errno;
			DEBUG_ONLY(UNUSED(real_errno));
			assert(FALSE);		/* while a white box case, we can ignore errors */
		}
		if (keepcnt_got != keepcnt_value)
		{
			flush_pio();
			printf("cnt setopt: %d, getopt: %d\n", keepcnt_value, keepcnt_got);
			FFLUSH(stdout);
		}
		if (-1 == setsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl_value, SIZEOF(keepintvl_value)))
		{
			real_errno = errno;
			DEBUG_ONLY(UNUSED(real_errno));
			assert(FALSE);		/* while a white box case, we can ignore errors */
		}
		keepintvl_got_len = SIZEOF(keepintvl_got);
		if (-1 == getsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl_got, &keepintvl_got_len))
		{
			real_errno = errno;
			DEBUG_ONLY(UNUSED(real_errno));
			assert(FALSE);		/* while a white box case, we can ignore errors */
		}
		if (keepintvl_got != keepintvl_value)
		{
			flush_pio();
			printf("intvl setopt: %d, getopt: %d\n", keepintvl_value, keepintvl_got);
			FFLUSH(stdout);
		}
	}
#	endif
	return TRUE;
}
