/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_tcp_keepalive.c */
/*
 * inputs: socket pointer; the keepalive_opt value used for idle; the name of the calling action; a Boolean giving need for cleanup
 * returns: TRUE for success, FALSE return is never really used because it's preceeded by the rts_error
 * the callers use a positive non-zero value of keepalive_opt to specify keepalive; used here to specify TCP_KEEPIDLE
 * this does not currently manipulate TCP_KEEPCNT  or TCP_KEEPINTVL except as part of a white box case that sets a short time
 * this function requires more sophistication if implementing control of the other two factors
 * control based on a deviceparameter rather than an environment variable for the process would require changes elsewhere
 */
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

boolean_t iosocket_tcp_keepalive(socket_struct *socketptr, int keepalive_opt, char *act)
{
	char			*errptr;
	d_socket_struct		*dsocketptr;
	int			keepalive_got;
	int4			errlen, real_errno;
#	ifdef DEBUG
	socklen_t		keepalive_got_len;
#	endif
	ssize_t			status;

	assert(0 <keepalive_opt);
#	ifdef DEBUG
#	ifndef DEBUG_SOCK
	if (WBTEST_ENABLED(WBTEST_SOCKET_KEEPALIVE))
	{
#	endif
		flush_pio();
		printf("%s ydb_socket_keepalive_idle: %d\n", act, keepalive_opt);
		FFLUSH(stdout);
		printf("%s setting SO_KEEPALIVE\n", act);
		FFLUSH(stdout);
#	ifndef DEBUG_SOCK
	}
#	endif
#	endif
real_errno = 0;
	DEBUG_ONLY(keepalive_got_len = SIZEOF(keepalive_got);)
	if (-1 == (status = setsockopt(socketptr->sd, SOL_SOCKET, SO_KEEPALIVE, &keepalive_opt, SIZEOF(keepalive_opt))))
	{
		real_errno = errno;
		keepalive_opt = 0;			/* clear as a flag determining message for subsequent rts_error */
	} else if (-1 == (status = setsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_opt, SIZEOF(keepalive_opt))))
		real_errno = errno;
#	ifdef DEBUG
	else if (-1 == (status = getsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_got, &keepalive_got_len)))
		real_errno = errno;
	if (0 == status)
	{
		errptr = (char *)STRERROR(real_errno);
		assert(keepalive_got == keepalive_opt);
		real_errno = 0;
	}
#	endif
	if (real_errno)
	{
		dsocketptr = socketptr->dev;
		errptr = (char *)STRERROR(real_errno);
		errlen = STRLEN(errptr);
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
		if (FD_INVALID != socketptr->sd)
		{
			close(socketptr->sd);	/* Don't leave a dangling socket around */
			socketptr->sd = FD_INVALID;
		}
		SOCKET_FREE(socketptr);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SETSOCKOPTERR, 5,
			      LEN_AND_STR(keepalive_opt ? "TCP_KEEPIDLE" : "SO_KEEPALIVE"), real_errno, errlen, errptr);
		return FALSE;
	}
#	ifdef DEBUG
	if (WBTEST_ENABLED(WBTEST_SOCKET_KEEPALIVE))
	{	/* when implementing socket-level control, set keepalive interval of 2 to recognize missing peer in 6 seconds */
		keepalive_opt = 2;		/* while in a whitebox case, a single value suffices*/
		flush_pio();
		printf("%s setting TCP_KEEP options\n wb enabled: %d, wb #: %d. opt: %d\n",
		       act, ydb_white_box_test_case_enabled, ydb_white_box_test_case_number, keepalive_opt);
		FFLUSH(stdout);
		if (-1 == setsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_opt, SIZEOF(keepalive_opt)))
			assert(FALSE);		/* while a white box case, we can ignore errors */
		if (-1 == getsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_got, &keepalive_got_len))
			assert(FALSE);		/* while a white box case, we can ignore errors */
		if (keepalive_got != keepalive_opt)
		{
			flush_pio();
			printf("cnt setopt: %d, getopt: %d\n", keepalive_opt, keepalive_got);
			FFLUSH(stdout);
		}
		if (-1 == setsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_opt, SIZEOF(keepalive_opt)))
			assert(FALSE);		/* while a white box case, we can ignore errors */
		if (-1 == getsockopt(socketptr->sd, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_got, &keepalive_got_len))
			assert(FALSE);		/* while a white box case, we can ignore errors */
		if (keepalive_got != keepalive_opt)
		{
			flush_pio();
			printf("intvl setopt: %d, getopt: %d\n", keepalive_opt, keepalive_got);
			FFLUSH(stdout);
		}
	}
#	endif
	return TRUE;
}
