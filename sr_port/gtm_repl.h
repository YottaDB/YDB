/****************************************************************
 *								*
 * Copyright (c) 2013-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _GTM_REPL_H
#define _GTM_REPL_H

#ifdef GTM_TLS
#include "gtm_tls.h"

typedef enum
{
	REPLTLS_RENEG_STATE_NONE,
	REPLTLS_WAITING_FOR_RENEG_TIMEOUT,
	REPLTLS_WAITING_FOR_RENEG_ACK,
	REPLTLS_SKIP_RENEGOTIATION,
	REPLTLS_WAITING_FOR_RENEG_COMPLETE	/* Used only by the receiver. */
} repl_tls_reneg_state;

typedef struct repl_tls_info_struct
{
	char			id[MAX_TLSID_LEN];
	boolean_t		plaintext_fallback;
	boolean_t		enabled;
	boolean_t		notls_retry;
	repl_tls_reneg_state	renegotiate_state;
	gtm_tls_socket_t	*sock;
} repl_tls_info_t;

GBLREF	repl_tls_info_t		repl_tls;

error_def(ERR_TLSCLOSE);

#define REPL_TLS_REQUESTED		('\0' != repl_tls.id[0])
#define CLEAR_REPL_TLS_REQUESTED	repl_tls.id[0] = '\0'
#define REPL_TLS_ENABLED		(repl_tls.enabled)
#define CLEAR_REPL_TLS_ENABLED		repl_tls.enabled = FALSE
#define PLAINTEXT_FALLBACK		(repl_tls.plaintext_fallback)

#define DEFAULT_RENEGOTIATE_TIMEOUT	(2 * 60) /* About 2 hours between renegotiation. */
#define MIN_RENEGOTIATE_TIMEOUT		1

#define REPLTLS_SET_NEXT_RENEGOTIATE_HRTBT(NEXT_RENEG_HRTBT)									\
{																\
	if (0 < gtmsource_options.renegotiate_interval)										\
	{															\
		repl_tls.renegotiate_state = REPLTLS_WAITING_FOR_RENEG_TIMEOUT;							\
		TIMEOUT_DONE_NOCH(NEXT_RENEG_HRTBT);										\
		TIMEOUT_INIT_NOCH(NEXT_RENEG_HRTBT, gtmsource_options.renegotiate_interval * MILLISECS_IN_SEC);			\
	}															\
}

#define ISSUE_REPLNOTLS(ERRID, STR1, STR2)											\
{																\
	if (!PLAINTEXT_FALLBACK)												\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERRID, 4, LEN_AND_LIT(STR1), LEN_AND_LIT(STR2));			\
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_WARNING(ERRID), 4, LEN_AND_LIT(STR1), LEN_AND_LIT(STR2));		\
}

void	repl_log_tls_info(FILE *logfp, gtm_tls_socket_t *socket);
int 	repl_do_tls_handshake(FILE *logfp, int sock_fd, boolean_t do_accept, int *poll_direction);
void	repl_do_tls_init(FILE *logfp);

#endif	/* GTM_TLS */

#endif
