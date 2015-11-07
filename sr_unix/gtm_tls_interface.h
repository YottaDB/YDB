/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _GTM_TLS_INTERFACE_H
#define _GTM_TLS_INTERFACE_H

#ifndef _GTM_TLS_INTERFACE_DEFINITIONS_INCLUDED
#define _GTM_TLS_INTERFACE_DEFINITIONS_INCLUDED

#define GTM_TLS_API_VERSION		0x00000001

#define MAX_X509_LEN			256
#define MAX_ALGORITHM_LEN		64
#define MAX_TIME_STRLEN			32
#define MAX_TLSID_LEN			32
#define MAX_SESSION_ID_LEN		64

#define INVALID_TLS_CONTEXT		NULL
#define INVALID_TLS_SOCKET		NULL
/* SSL/TLS protocol allows for renegoitations (change in cipher spec, etc.) to take place at any point during the communication. So,
 * it is likely for a call to `gtm_tls_recv' or `gtm_tls_send' to initiate a renegoitation sequence in which case a `gtm_tls_recv'
 * might have to 'send' data and `gtm_tls_send' to 'recv' data. In such cases, the SSL/TLS reference implementation returns one of
 * the following two return codes. They do not indicate an error situation. Instead, they indicate that the reference implementation
 * could not successfully complete the operation without reading or writing more data to the underlying TCP/IP connection and gives
 * a chance to the application to wait for the underlying TCP/IP pipe to become ready for a read (if GTMTLS_WANT_READ is returned)
 * or write (if GTMTLS_WANT_WRITE is returned).
 */
#define GTMTLS_WANT_READ		-2
#define GTMTLS_WANT_WRITE		-3

#define GTMTLS_PASSWD_ENV_PREFIX	"gtmtls_passwd_"

/* Whether the library is loaded in an interactive environment so that password prompting can happen if needed. */
#define GTMTLS_OP_INTERACTIVE_MODE	0x00000001
/* Turn-on compression for SSL/TLS protocol. */
#define GTMTLS_OP_ENABLE_COMPRESSION	0x00000002
/* Socket created for a client-mode operation. */
#define GTMTLS_OP_CLIENT_MODE		0x00000004
/* Peer verification is needed. */
#define GTMTLS_OP_VERIFY_PEER		0x00000008

#define GTMTLS_IS_FIPS_MODE(CTX)	(TRUE == CTX->fips_mode)
#define GTMTLS_RUNTIME_LIB_VERSION(CTX)	(CTX->runtime_version)

typedef struct gtm_tls_conn_info_struct
{
	/* SSL Session Information */
	char		protocol[MAX_ALGORITHM_LEN];	/* Descriptive name of the negoitiated protocol (for instance, TLSv1). */
	char		session_algo[MAX_ALGORITHM_LEN];/* Algorithm negoitated by the SSL/TLS session. */
	char		session_id[MAX_SESSION_ID_LEN + 1]; /* Hexadecimal representation of the negotiated Session-ID. */
	char		*compression;			/* Compression method used for the SSL/TLS session. */
	int		secure_renegotiation;		/* 1 if SSL/TLS renegotiation is supported and 0 otherwise. */
	int		reused;				/* Is the session reused? */
	long		session_expiry_timeout;		/* Time at which this session expires (-1 if doesn't expire). */
	/* Remote Certificate Information */
	char		cert_algo[MAX_ALGORITHM_LEN];	/* Certificate's asymmetric cryptography algorithm. */
	int		cert_nbits;			/* Strength of the certificate's asymmetric cryptography. */
	char		subject[MAX_X509_LEN];		/* To whom the certificate belongs? */
	char		issuer[MAX_X509_LEN];		/* CA who issued the certificate. */
	char		not_before[MAX_TIME_STRLEN];	/* Date before which the certificate is not valid. */
	char		not_after[MAX_TIME_STRLEN];	/* Date after which the certificate is not valid. */
} gtm_tls_conn_info;

typedef struct gtm_tls_session_struct
{
	int			flags;
	void			*ssl;
	void			*session;
} gtm_tls_socket_t;

typedef struct gtm_tls_ctx_struct
{
	int			flags;
	int			fips_mode;
	unsigned long		compile_time_version;	/* OpenSSL version that this library is compiled with. */
	unsigned long		runtime_version;	/* OpenSSL version that this library is currently running with. */
	void			*ctx;
} gtm_tls_ctx_t;

#endif	/* _GTM_TLS_INTERFACE_DEFINITIONS_INCLUDED */

/* Note: The below function prototypes should be kept in sync with the corresponding declarations/definitions in sr_unix/gtm_tls.h
 * and sr_unix/gtm_tls_funclist.h.
 */

/* Returns the most recent error (null-terminated) related to the workings of the SSL/TLS reference implementation. */
const char		*gtm_tls_get_error(void);

/* If the most recent invocation of the SSL/TLS reference implementation resulted in a system call error, `gtm_tls_errno' returns
 * the value of `errno'. Otherwise, -1 is returned in which case `gtm_tls_get_error' provides more information.
 */
int		gtm_tls_errno(void);

/* Initializes the SSL/TLS context for a process. Typically invoked only once (unless the previous attempt failed). Attributes
 * necessary to initialize the SSL/TLS context are obtained from the configuration file pointed to by `$gtmcrypt_config'.
 *
 * Arguments:
 *   `version' : The API version that the caller understands. Current version is 0x1.
 *   `flags'   : Initialization flags as a bitmask. Currently, the only one the API understands is GTMTLS_OP_INTERACTIVE_MODE.
 *               Set this bitmask if the process doing the SSL/TLS initialization is run in an interactive mode. This lets
 *               the API decide if it can prompt for a password if a need arises while decrypting the private key.
 *
 * Returns a value, of type gtm_tls_ctx_t, representing the initialized SSL/TLS context. This context can be used, later by the
 * application, to create as many SSL/TLS aware sockets as needed. In case of an error, INVALID_TLS_CONTEXT is returned in which
 * case gtm_tls_get_error() provides the necessary error detail.
 */
gtm_tls_ctx_t	*gtm_tls_init(int version, int flags);

/* Prefetches the password corresponding to a private key.
 *
 * Arguments:
 *    `tls_ctx'  : The SSL/TLS context corresponding to this process.
 *    `env_name' : The name of the environment variable that corresponds to the private key in question. The SSL/TLS API identifies
 *                 a private key by a name that is specified in the configuration file. The name of the environment variable is then
 *                 obtained by prefixing "gtmtls_passwd_" to the identifier. So, if the identifier is `PRODUCTION', then the name
 *                 of the environment variable is "gtmtls_passwd_PRODUCTION".
 *
 * No return value. Since this is only an attempt to prefetch the password, no error is reported. Another attempt will be made,
 * later to acquire the password when actually decrypting the private key.
 *
 * Note 1: This function is typically invoked whenever the application detects that an environment variable of the form
 * "gtmtls_passwd_<identifier>" is present in the environment but doesn't have any assoicated value. In such a case the below
 * function prompts the user to provide the password before continuing any further. The password is not validated, but is stored
 * for future use.
 *
 * Note 2: The function honors the GTMTLS_OP_INTERACTIVE_MODE flag passed to the `gtm_tls_init' function. If the application has
 * initialized the SSL/TLS API in a non-interactive mode, the API does not prompt the user for password.
 */
void		gtm_tls_prefetch_passwd(gtm_tls_ctx_t *tls_ctx, char *env_name);

/* Converts a Unix TCP/IP socket into a SSL/TLS aware socket.
 *
 * Arguments:
 *    `ctx'         : The SSL/TLS context corresponding to this process.
 *    `prev_socket` : Pointer to an existing `gtm_tls_socket_t' structure. A non-null value indicates reuse.
 *    `sockfd'      : The Unix TCP/IP socket identifier.
 *    `tls_id' : Identifier corresponding to the private key and certificate pair that should be used for future communication.
 *               The plugin searches for the identifier in the configuration file pointed to by `$gtmcrypt_config' to get other
 *               information corresponding to this connection (like, path to the private key, certificate and the format of the
 *               private key).
 *    `flags'  : Additional configuration options. See GTMTLS_OP* macros for more details.
 *
 * Returns a value, of type gtm_tls_socket_t *, representing the initialized SSL/TLS aware socket. This value can be used for actual
 * communication to provide security. In case of an error, INVALID_TLS_SOCKET is returned in which case gtm_tls_get_error()
 * provides the necessary error detail. If `prev_socket' is non-NULL, then that storage area is used for setting up the new socket
 * instead of creating a new storage area.
 *
 * Note 1: If the password corresponding to the `tls_id' has not yet been prefetched by the SSL/TLS API, then the API attempts to
 * read the password from the environment.
 *
 * Note 2: The function honors the GTMTLS_OP_INTERACTIVE_MODE flag passed to the `gtm_tls_init' function. If the application has
 * initialized the SSL/TLS API in a non-interactive mode, this function does not prompt the user for password.
 */
gtm_tls_socket_t *gtm_tls_socket(gtm_tls_ctx_t *ctx, gtm_tls_socket_t *prev_socket, int sockfd, char *id, int flags);

/* Connects using SSL/TLS aware socket. Assumes the other transport endpoint understands SSL/TLS.
 *
 * Arguments:
 *    `socket'       : The SSL/TLS socket (initialized using `gtm_tls_socket').
 *
 * Returns 0 if the connection is successful. Otherwise, one of -1, GTMTLS_WANT_READ or GTMTLS_WANT_WRITE is returned. In case of
 * -1, `gtm_tls_errno' and `gtm_tls_get_error' can be used to obtain the necessary error detail.
 *
 * Note: The function makes use of an existing SSL session (if one is available).
 */
int		gtm_tls_connect(gtm_tls_socket_t *socket);

/* Accepts an incoming connection using SSL/TLS aware socket. Assumes the other transport endpoint understands SSL/TLS.
 *
 * Arguments:
 *    `socket' : The SSL/TLS socket (initialized using `gtm_tls_socket').
 *
 * Returns 0 if the connection is successful. Otherwise, one of -1, GTMTLS_WANT_READ or GTMTLS_WANT_WRITE is returned. In case of
 * -1, `gtm_tls_errno' and `gtm_tls_get_error' can be used to obtain the necessary error detail.
 *
 */
int		gtm_tls_accept(gtm_tls_socket_t *socket);

/* Renegotiates an active SSL/TLS connection. Note: This function does the renegotiation in a blocking fashion and more importantly
 * handles EINTR internally by retrying the renegotiation.
 *
 * Arguments:
 *    `socket'   : The SSL/TLS socket (initialized using `gtm_tls_socket').
 *
 * Return value: none.
 */
int		gtm_tls_renegotiate(gtm_tls_socket_t *socket);

/* Obtains additional SSL/TLS related information on the peer. This function is typically invoked to log information for diagnostic
 * purposes.
 *
 * Arguments:
 *    `socket'   : The SSL/TLS socket (initialized using `gtm_tls_socket').
 *    `conn_info' : A pointer to the `gtm_tls_conn_info' structure.
 *
 * Returns 0 if the connection is successful (and conn_info structure is filled). Otherwise, one of -1, GTMTLS_WANT_READ or
 * GTMTLS_WANT_WRITE is returned. In case of -1, `gtm_tls_errno' and `gtm_tls_get_error' can be used to obtain the necessary error
 * detail.
 */
int		gtm_tls_get_conn_info(gtm_tls_socket_t *socket, gtm_tls_conn_info *conn_info);

/* Transmits message securely to the transport endpoint. This function should be invoked ONLY after successful invocations of either
 * `gtm_tls_connect' or `gtm_tls_accept'.
 *
 * Arguments:
 *    `socket'   : The SSL/TLS socket (initialized using `gtm_tls_socket').
 *    `buf'      : Buffer containing the message that has to be transmitted.
 *    `send_len' : Length of the message that has to be transmitted.
 *
 * If successful, returns the number of bytes (> 0) actually sent through the SSL/TLS connection. Otherwise, one of -1,
 * GTMTLS_WANT_READ or GTMTLS_WANT_WRITE is returned. In case of -1, `gtm_tls_errno' and `gtm_tls_get_error' can be used to obtain
 * the necessary error detail.
 */
int		gtm_tls_send(gtm_tls_socket_t *socket, char *buf, int send_len);

/* Receives message securely from the transport endpoint. This function should be invoked ONLY after successful invocations of
 * either `gtm_tls_connect' or `gtm_tls_accept'.
 *
 * Arguments:
 *    `socket'   : The SSL/TLS socket (initialized using `gtm_tls_socket').
 *    `buf'      : Buffer in which the received data (after having decrypted) has to be placed.
 *    `recv_len' : Upper bound on the amount of data that can be received.
 *                     become ready for a `send' or `recv'.
 *
 * If successful, returns the number of bytes (> 0) actually received from the SSL/TLS connection. Otherwise, one of -1,
 * GTMTLS_WANT_READ or GTMTLS_WANT_WRITE is returned. In case of -1, `gtm_tls_errno' and `gtm_tls_get_error' can be used to obtain
 * the necessary error detail.
 */
int		gtm_tls_recv(gtm_tls_socket_t *socket, char *buf, int recv_len);

/* Returns the number of bytes cached in the SSL/TLS layer and is ready for immediate retrieval with the `gtm_tls_recv'.
 *
 * Arguments:
 *    `socket'   : The SSL/TLS socket (initialized using `gtm_tls_socket').
 *
 * Note: Data from the peer is received in blocks. Therefore, data can be buffered inside the SSL/TLS layer and is ready for
 * immediate retrieval with `gtm_tls_recv'. Given this, it is important for the application to call this function, if `select'
 * or `poll' on the underlying TCP/IP socket indicates that the subsequent `recv' will block, and check if there are any bytes
 * readily available.
 */
int		gtm_tls_cachedbytes(gtm_tls_socket_t *socket);

/* Close the SSL/TLS socket connection.
 *
 * Arguments:
 *    `socket'   : Pointer to the SSL/TLS socket (initialized using `gtm_tls_socket').
 *
 * Note: This function merely shuts down the active SSL/TLS connection. The session is still preserved in the SSL/TLS internal
 * structures for reuse when a connection is made with the same server at a later point.
 *
 * Returns 0 if successful and -1 otherwise. In case of an error, `gtm_tls_errno' and `gtm_tls_get_error' can be used to obtain
 * necessary error detail.
 *
 */
void		gtm_tls_socket_close(gtm_tls_socket_t *socket);

/* Closes an active SSL/TLS session. This frees up the session and thus makes the session not resuable for a future connection.
 * Any subsequent connection will create a new session.
 *
 * Note: The function takes a pointer to the gtm_tls_socket_t structure. This is because it forces the actual `socket' value to be
 * INVALID_TLS_SOCKET.
 */
void		gtm_tls_session_close(gtm_tls_socket_t **socket);

/* Frees up any memory allocated by the SSL/TLS context. This function should typically be invoked at process exit.
 *
 * Arguments:
 *   `ctx'    :  Pointer to the SSL/TLS context (initialized using `gtm_tls_init').
 *
 * No return value.
 *
 * Note: The function takes a pointer to the gtm_tls_ctx_t structure. This is because it forces the actual `ctx' value to be
 * INVALID_TLS_CONTEXT.
 */
void		gtm_tls_fini(gtm_tls_ctx_t **ctx);

#endif
