/****************************************************************
 *								*
 * Copyright (c) 2013-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

STATICFNDEF gtmtls_passwd_list_t *gtm_tls_find_pwent(const char *input_env_name);

#define GET_SOCKFD(TLS)			SSL_get_fd((SSL *)TLS)
#define CLIENT_MODE(FLAGS)		(FLAGS & GTMTLS_OP_CLIENT_MODE)
#define DEFAULT_SESSION_TIMEOUT		3600		/* Old sessions can be reused upto 1 hour since the creation time. */

#endif
