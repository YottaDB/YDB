/****************************************************************
 *								*
 * Copyright (c) 2013-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_REPL_H_INCLUDED
#define GTM_REPL_H_INCLUDED
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

#define REPLTLS_SET_NEXT_RENEGOTIATE_HRTBT(NEXT_RENEG_HRTBT)								\
MBSTART {														\
	if (0 < gtmsource_options.renegotiate_interval)									\
	{														\
		repl_tls.renegotiate_state = REPLTLS_WAITING_FOR_RENEG_TIMEOUT;						\
		TIMEOUT_DONE_NOCH(NEXT_RENEG_HRTBT);									\
		TIMEOUT_INIT_NOCH(NEXT_RENEG_HRTBT, gtmsource_options.renegotiate_interval * (uint8)NANOSECS_IN_SEC);	\
	}														\
} MBEND

#define ISSUE_REPLNOTLS(ERRID, STR1, STR2)										\
MBSTART {														\
	if (!PLAINTEXT_FALLBACK)											\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERRID, 4, LEN_AND_LIT(STR1), LEN_AND_LIT(STR2));		\
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_WARNING(ERRID), 4, LEN_AND_LIT(STR1), LEN_AND_LIT(STR2));	\
} MBEND

/* The GTMSOURCE_HANDLE_TLSIOERROR and GTMRECV_HANDLE_TLSIOERROR macros handle an error from the TLS/SSL layer.
 * They check if plaintext fallback is specified by the user and if so, close the current connection and fall
 * back to non-tls (i.e. non-encrypted) communication or replication. If plaintext fallback is not specified,
 * then issue a TLSIOERROR error and terminate the caller replication server (source or receiver server).
 */
#define	GTMSOURCE_HANDLE_TLSIOERROR(SEND_OR_RECV)								\
MBSTART {													\
	assert(repl_tls.enabled);										\
	if (!PLAINTEXT_FALLBACK)										\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TLSIOERROR, 2, LEN_AND_LIT(SEND_OR_RECV),		\
							ERR_TEXT, 2, LEN_AND_STR(gtm_tls_get_error()));		\
	else													\
	{	/* Fall back from TLS to Plaintext */								\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)							\
			MAKE_MSG_WARNING(ERR_TLSIOERROR), 2, LEN_AND_LIT(SEND_OR_RECV),				\
						ERR_TEXT, 2, LEN_AND_STR(gtm_tls_get_error()));			\
		repl_log(gtmsource_log_fp, TRUE, TRUE,								\
				"Plaintext fallback enabled. Closing and reconnecting without TLS/SSL.\n");	\
		repl_close(&gtmsource_sock_fd);									\
		SHORT_SLEEP(GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN);						\
		gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;		\
		CLEAR_REPL_TLS_REQUESTED; /* As if -tlsid qualifier was never specified. */			\
	}													\
} MBEND

#define	GTMRECV_HANDLE_TLSIOERROR(SEND_OR_RECV)									\
MBSTART {													\
	assert(repl_tls.enabled);										\
	if (!PLAINTEXT_FALLBACK)										\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TLSIOERROR, 2, LEN_AND_LIT(SEND_OR_RECV),		\
							ERR_TEXT, 2, LEN_AND_STR(gtm_tls_get_error()));		\
	else													\
	{	/* Fall back from TLS to Plaintext */								\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)							\
			MAKE_MSG_WARNING(ERR_TLSIOERROR), 2, LEN_AND_LIT(SEND_OR_RECV),				\
						ERR_TEXT, 2, LEN_AND_STR(gtm_tls_get_error()));			\
		repl_log(gtmrecv_log_fp, TRUE, TRUE,								\
				"Plaintext fallback enabled. Closing and reconnecting without TLS/SSL.\n");	\
		repl_close(&gtmrecv_sock_fd);									\
		repl_connection_reset = TRUE;									\
		CLEAR_REPL_TLS_REQUESTED; /* As if -tlsid qualifier was never specified. */			\
	}													\
} MBEND

void	repl_log_tls_info(FILE *logfp, gtm_tls_socket_t *socket);
int 	repl_do_tls_handshake(FILE *logfp, int sock_fd, boolean_t do_accept, int *poll_direction);
void	repl_do_tls_init(FILE *logfp);

#endif	/* GTM_TLS */

#endif
