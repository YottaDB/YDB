/****************************************************************
 *								*
 * Copyright (c) 2013-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_TLS_IMPL_H
#define GTM_TLS_IMPL_H

STATICFNDEF int format_ASN1_TIME(ASN1_TIME *tm, char *buf, int maxlen);
STATICFNDEF int ssl_generic_vfy_callback(int preverify_ok, X509_STORE_CTX *ctx);
STATICFNDEF int	passwd_callback(char *buf, int size, int rwflag, void *userdata);
STATICFNDEF int new_session_callback(SSL *ssl, SSL_SESSION *session);
STATICFNDEF void remove_session_callback(SSL_CTX *ctx, SSL_SESSION *session);
STATICFNDEF DH *read_dhparams(const char *dh_fn);
STATICFNDEF int init_dhparams(void);
STATICFNDEF DH *tmp_dh_callback(SSL *ssl, int is_export, int keylength);
STATICFNDEF int ssl_error(gtm_tls_socket_t *tls_sock, int err, long verify_result);

typedef struct gtmtls_passwd_list_struct
{
	struct gtmtls_passwd_list_struct *next;
	passwd_entry_t			 *pwent;
} gtmtls_passwd_list_t;

STATICFNDEF gtmtls_passwd_list_t *gtm_tls_find_pwent(ydbenvindx_t envindx, char *input_suffix);

#define GET_SOCKFD(TLS)			SSL_get_fd((SSL *)TLS)
#define VERIFY_PEER(FLAGS)		(FLAGS & GTMTLS_OP_VERIFY_PEER)
#define CLIENT_MODE(FLAGS)		(FLAGS & GTMTLS_OP_CLIENT_MODE)
#define DEFAULT_SESSION_TIMEOUT		3600		/* Old sessions can be reused upto 1 hour since the creation time. */

#ifdef DEBUG
/* Verify that the socket (about to be wrapped or linked to an existing SSL object is actually of blocking type. This library
 * currently supports only blocking SSL/TLS operations. If ever this check fails, either the callee needs to be examined OR the
 * implementation needs to account for non-blocking sockets.
 */
#define DBG_VERIFY_SOCK_IS_BLOCKING(SOCKFD)											\
{																\
	int		flags;													\
																\
	assert(0 <= SOCKFD);													\
	flags = fcntl(SOCKFD, F_GETFL);												\
	if (-1 == flags) {													\
		perror("%YDB-E-GETSOCKOPTERR, fcntl on SOCKFD failed");								\
	} else {														\
		assert(0 == (O_NONBLOCK & flags));										\
	}															\
}

#define DBG_VERIFY_AUTORETRY_SET(TLS_DESC)	assert(SSL_MODE_AUTO_RETRY & SSL_CTX_get_mode((SSL_CTX *)TLS_DESC));

#else
#define DBG_VERIFY_SOCK_IS_BLOCKING(SOCKFD)
#define DBG_VERIFY_AUTORETRY_SET(TLS_DESC)
#endif

#endif
