/****************************************************************
 *								*
 * Copyright (c) 2013-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dlfcn.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>

#include <libconfig.h>

#include "gtmxc_types.h"
#include "gtmcrypt_util.h"

#include "gtm_tls_interface.h"
#include "gtm_tls_impl.h"
#include "gtm_tls_externalcalls.h"
#ifdef DEBUG
#include <stdlib.h>
#endif

GBLDEF	int			tls_errno;
GBLDEF	gtmtls_passwd_list_t	*gtmtls_passwd_listhead;

STATICDEF	config_t	gtm_tls_cfg;
STATICDEF	gtm_tls_ctx_t	*gtm_tls_ctx = NULL;
#if OPENSSL_VERSION_NUMBER < 0x30000000L
STATICDEF DH			*dh512, *dh1024;	/* Diffie-Hellman structures for Ephemeral Diffie-Hellman key exchange. */
#endif
#ifdef DEBUG
STATICDEF	char		*wbox_enable = NULL, *wbox_tls_check = NULL,
				*wbox_count = NULL, *wbox_test_count = NULL;
STATICDEF	int		wbox_count_val = 0, wbox_test_count_val = 0, wbox_enable_val = 0, wbox_tls_check_val = 0;
#define	TRUE	1
#define	FALSE	0
#define	WBTEST_REPL_TLS_RECONN 169
#endif

#define MAX_CONFIG_LOOKUP_PATHLEN	64

/* Older, but still commonly used, OpenSSL versions don't have macros for TLSv1.1 and TLSv1.2 versions.
 * They are hard coded to 0x0302 and 0x0303 respectively. So, define them here for use in gtm_tls_get_conn_info.
*/
#ifndef TLS1_1_VERSION
#define	TLS1_1_VERSION	0x0302
#endif
#ifndef TLS1_2_VERSION
#define	TLS1_2_VERSION	0x0303
#endif
#ifndef TLS1_3_VERSION
#define	TLS1_3_VERSION	0x0304
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
#define PHA_MACROS_ENABLED	(SSL_VERIFY_PEER | SSL_VERIFY_POST_HANDSHAKE)
#endif
/* Below template translates to: Arrange ciphers in increasing order of strength after excluding the following:
 * ADH: Anonymous Diffie-Hellman Key Exchange (Since we want both encryption and authentication and ADH provides only former).
 * LOW: Low strength ciphers.
 * EXP: Export Ciphers.
 * MD5 : MD5 message digest.
 */
#define REPL_CIPHER_LIST			"ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"
#define GTM_DEFAULT_CIPHER_LIST			"DEFAULT:!SRP"

#define	CIPHER_LIST_SIZE	4096
#define	ERR_STR_SIZE		2048

#define OPTIONEND ':'
#define OPTIONENDSTR ":"
#define OPTIONNOT '!'
#define DEFINE_SSL_OP(OP_DEF)   { #OP_DEF , OP_DEF }

/* Macros for the external call function input parameters */
#define TLS1_2_STR		"tls1_2"
#define TLS1_3_STR		"tls1_3"
#define SOCKET_STR		"SOCKET"
#define REPLICATION_STR		"REPLICATION"
#define COMPILE_TIME_STR	"compile-time"
#define RUN_TIME_STR		"run-time"

#define SET_AND_APPEND_OPENSSL_ERROR(...)										\
{															\
	char	*errptr, *end;												\
	int	rv;													\
															\
	rv = gtm_tls_set_error(NULL, __VA_ARGS__);									\
	end = errptr = (char *)gtm_tls_get_error(NULL);									\
	end += MAX_GTMCRYPT_ERR_STRLEN;											\
	errptr += rv;													\
	if (end > errptr)												\
	{														\
		rv = snprintf(errptr, end - errptr, "%s", " Reason: ");							\
		if (0 <= rv)												\
			errptr += rv;											\
	}														\
	if (end > errptr)												\
	{														\
		rv = ERR_get_error();											\
		ERR_error_string_n(rv, errptr, end - errptr);								\
	}														\
}

struct gtm_ssl_options
{
	const char	*opt_str;
	long		opt_val;
};
STATICDEF struct gtm_ssl_options gtm_ssl_verify_level_list[]=
{
	{ "CHECK", GTMTLS_OP_VERIFY_LEVEL_CHECK },
	{NULL, 0}
};
STATICDEF struct gtm_ssl_options gtm_ssl_verify_mode_list[] =
{
#include "gen_tls_verify_options.h"
	{NULL, 0}
};
STATICDEF struct gtm_ssl_options gtm_ssl_options_list[] =
{
#include "gen_tls_options.h"
#ifndef	SSL_OP_NO_TLSv1_3
/*	Special case: define this option so that pre OpenSSL 1.1.1 recognize it */
#	define SSL_OP_NO_TLSv1_3	0x0
	DEFINE_SSL_OP(SSL_OP_NO_TLSv1_3),
#endif
	{NULL, 0}
};

#ifdef	SSL_OP_NO_TLSv1
	#define	GTM_NO_TLSv1	| SSL_OP_NO_TLSv1
#else
	#define	GTM_NO_TLSv1
#endif
#ifdef	SSL_OP_NO_TLSv1_1
	#define	GTM_NO_TLSv1_1	| SSL_OP_NO_TLSv1_1
#else
	#define	GTM_NO_TLSv1_1
#endif
/* Deprecate all SSL/TLS Protocols before TLS v1.2 */
#define	DEPRECATED_SSLTLS_PROTOCOLS (SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 GTM_NO_TLSv1 GTM_NO_TLSv1_1)

/* Static function definitions */
STATICFNDEF char *parse_SSL_options(struct gtm_ssl_options *opt_table, size_t opt_table_size, const char *options, long *current,
					long *clear);
STATICFNDEF int gtm_tls_set_error(gtm_tls_socket_t *tlssocket, const char *format, ...);
STATICFNDEF void gtm_tls_copy_gcerror(gtm_tls_socket_t *tlssocket);

STATICFNDEF char *parse_SSL_options(struct gtm_ssl_options *opt_table, size_t opt_table_size, const char *options, long *current,
					long *clear)
{
	int		negate;
	size_t		num_options, index, optionlen;
	long		bitmask;
	const char	*charptr, *optionend;

	if (NULL == options)
		return 0;
	negate = 0;
	bitmask = *current;
	num_options = opt_table_size/SIZEOF(struct gtm_ssl_options);
	for (charptr = options; *charptr; charptr = optionend)
	{
		if (OPTIONEND == *charptr)
			if ('\0' == *++charptr)
				break;
		optionend = strstr((const char *)charptr, OPTIONENDSTR);
		if (NULL == optionend)
			optionend = charptr + strlen(charptr);
		if (OPTIONNOT == *charptr)
		{
			negate = TRUE;
			charptr++;
		} else
			negate = FALSE;
		optionlen = optionend - charptr;
		for (index = 0; num_options > index ; index++)
		{
			if (NULL == opt_table[index].opt_str)
				break;
			if ((optionlen == strlen(opt_table[index].opt_str))
				&& (0 == strncmp(opt_table[index].opt_str, charptr, optionlen)))
			{
				if (negate)
				{
					bitmask &= ~opt_table[index].opt_val;
					if (NULL != clear)
						*clear |= opt_table[index].opt_val;
				} else
					bitmask |= opt_table[index].opt_val;
				break;
			}
		}
		if ((num_options - 1) <= index)		/* last option is NULL */
			return (char *)charptr;	/* option not known */
	}
	*current = bitmask;
	return NULL;
}
#ifdef DEBUG_SSL
#define SSL_DPRINT(FP, ...)		{fprintf(FP, __VA_ARGS__); fflush(FP);}	/* BYPASSOK -- cannot use FFLUSH. */
#else
#define SSL_DPRINT(FP, ...)
#endif

/* OpenSSL doesn't provide an easy way to convert an ASN1 time to a string format unless BIOs are used. So, use a memory BIO to
 * write the string representation of ASN1_TIME in the said buffer.
 */
STATICFNDEF int format_ASN1_TIME(ASN1_TIME *tm, char *buf, int maxlen)
{
	BIO	*memfp;
	int	len;

	if (NULL == (memfp = BIO_new(BIO_s_mem())))
		return -1;
	ASN1_TIME_print(memfp, tm);
	len = BIO_pending(memfp);
	len = len < maxlen ? len : maxlen;
	if (0 >= BIO_read(memfp, buf, len))
		return -1;
	if (0 >= BIO_free(memfp))
		return -1;
	buf[len] = '\0';
	return 0;
}

STATICFNDEF int ssl_error(gtm_tls_socket_t *tls_sock, int err, long verify_result)
{
	int		ssl_error_code, reason_code, err_lib;
	unsigned long	error_code;
	char		*base, *errptr, *end;
	SSL		*ssl;
#ifdef DEBUG
	int		is_wb = FALSE;
#endif

	ssl = tls_sock->ssl;
	ssl_error_code = SSL_get_error(ssl, err); /* generic error code */
	SSL_DPRINT(stderr, "Error code: %d\n", ssl_error_code);
#ifdef DEBUG
	/* Change the error code to induce error in case of
	 * WBTEST_REPL_TLS_RECONN white box.
	 */
	if ((1 == wbox_enable_val) && (WBTEST_REPL_TLS_RECONN == wbox_tls_check_val)
		&& (wbox_test_count_val == wbox_count_val))
	{
		SSL_DPRINT(stderr, "Prev code: %d setting code to: %d\n",
				ssl_error_code,SSL_ERROR_SSL);
		is_wb = TRUE;
		ssl_error_code = SSL_ERROR_SSL;
	}
#endif
	switch (ssl_error_code)
	{
		case SSL_ERROR_ZERO_RETURN:
			/* SSL/TLS connection has been closed gracefully. The underlying TCP/IP connection is not yet closed. The
			 * caller should take necessary action to close the underlying transport
			 */
			tls_errno = ECONNRESET;
			break;

		case SSL_ERROR_SYSCALL:
			tls_errno = errno;
			SSL_DPRINT(stderr, "SSL_ERROR_SYSCALL: %d\n", tls_errno);
			gtm_tls_set_error(tls_sock, "SSL_ERROR_SYSCALL: %s", strerror(tls_errno));
			if (0 == tls_errno)   /* If no error at underlying socket, consisder a connecton reset */
				tls_errno = ECONNRESET;
			tls_sock->flags |= GTMTLS_OP_NOSHUTDOWN;
			break;

		case SSL_ERROR_WANT_WRITE:
			return GTMTLS_WANT_WRITE;

		case SSL_ERROR_WANT_READ:
			return GTMTLS_WANT_READ;

		case SSL_ERROR_WANT_X509_LOOKUP:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_WANT_CONNECT:
			assert(FALSE);

		case SSL_ERROR_SSL:
		case SSL_ERROR_NONE:
			base = errptr = (char *)gtm_tls_get_error(tls_sock);
			assert(errptr != NULL);
			end = errptr + MAX_GTMCRYPT_ERR_STRLEN;
			tls_errno = -1;
			if ((GTMTLS_OP_VERIFY_LEVEL_CHECK & tls_sock->flags) && (X509_V_OK != verify_result))
			{
				gtm_tls_set_error(tls_sock, "certificate verification error: %s",
					X509_verify_cert_error_string(verify_result));
				return -1;
			} else if (SSL_ERROR_NONE == ssl_error_code)
			{	/* we are ignoring verify result and no other error */
				tls_errno = 0;
				/* Return the orginal return value from the prev SSL call,
				 * since SSL_ERROR_NONE is only returned when ret > 0
				 */
				return err;
			} else if (SSL_ERROR_SSL  == ssl_error_code)
			{
				SSL_DPRINT(stderr, "Resetting the err code\n");
				/* Check the first error in the queue */
				error_code = ERR_peek_error();
#ifdef DEBUG
				assert(error_code || ((1 == wbox_enable_val) && (WBTEST_REPL_TLS_RECONN
					== wbox_tls_check_val)));
#endif
				err_lib = ERR_GET_LIB(error_code);
				reason_code = ERR_GET_REASON(error_code);
#ifdef DEBUG
				if (is_wb)
				{
					reason_code = SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC;
					err_lib = ERR_LIB_SSL;
				}
#endif
				/*	Change the returned error only for replication
				 *	and if the error comes from SSL library
				 */
				if (!(tls_sock->flags & GTMTLS_OP_SOCKET_DEV) && !(tls_sock->flags & GTMTLS_OP_DM_AUDIT)
					&& (ERR_LIB_SSL == err_lib) &&
					((SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC == reason_code) ||
					(SSL_R_SSLV3_ALERT_BAD_RECORD_MAC == reason_code)))
					tls_errno = ECONNRESET;
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
				if ((TLS1_3_VERSION == SSL_version(tls_sock->ssl))
						&& (SSL_R_EXTENSION_NOT_RECEIVED == reason_code))
					tls_sock->flags |= GTMTLS_OP_PHA_EXT_NOT_RECEIVED;
#endif
				tls_sock->flags |= GTMTLS_OP_NOSHUTDOWN;
			}
			do
			{
				error_code = ERR_get_error();
				if (0 == error_code)
				{
					if (errptr == base)
					{
						/* Very first call to ERR_get_error returned 0. This is very unlikely. Nevertheless
						 * handle this by updating the error string with a generic error.
						 */
						gtm_tls_set_error(tls_sock, "Unknown SSL/TLS protocol error.");
						return -1;
					}
					break;
				} else if ((errptr < end) && (errptr != base))
					*errptr++ = ';';
				if (errptr >= end)
					continue;	/* We could break here, but we want to clear the OpenSSL error stack. */
				ERR_error_string_n(error_code, errptr, end - errptr);
				errptr += STRLEN(errptr);
			} while (TRUE);
			break;

		default:
			tls_errno = -1;
			gtm_tls_set_error(tls_sock, "Unknown error: %d returned by `SSL_get_error'", ssl_error_code);
			assert(FALSE);
			break;
	}
	return -1;
}

/* This callback function is called whenever OpenSSL wants to decrypt the private key (e.g., SSL_CTX_check_private_key()). More
 * importantly, when OpenSSL calls this callback function, it passes a userdata which provides the callback a hint as to which
 * private key this callback function corresponds to. In our case, this hint is equivalent to the TLSID passed to the plugin when
 * gtm_tls_socket is called.
 */
STATICFNDEF int	passwd_callback(char *buf, int size, int rwflag, void *userdata)
{
	passwd_entry_t		*pwent;
	int			len;

	pwent = (passwd_entry_t *)userdata;
	assert(NULL != pwent);
	assert(NULL != pwent->passwd);
	len = STRLEN(pwent->passwd);
	strncpy(buf, pwent->passwd, size);
	if (len >= size)
	{
		buf[size] = '\0';
		len = size - 1;
	}
	return len;
}

/* The below callback is invoked whenever OpenSSL creates a new session (either because the old session expired or because of a
 * renegotiation). Store the session back into the API's socket structure.
 */
STATICFNDEF int new_session_callback(SSL *ssl, SSL_SESSION *session)
{
	gtm_tls_socket_t	*socket;

	/* See SSL_set_app_data for details on why we need to use the compatibility interface: SSL_get_app_data/SSL_set_app_data. */
	socket = SSL_get_app_data(ssl);
	assert(NULL != socket);
	/* Free up the old session. */
	SSL_DPRINT(stderr, "new_session_callback: references=%d\n", session->references);
	if (socket->session)
		SSL_SESSION_free(socket->session);
	/* Add the new session to the `socket' structure. */
	socket->session = session;
	return 1;
}

#if OPENSSL_VERSION_NUMBER < 0x30000000L
STATICFNDEF DH *read_dhparams(const char *dh_fn)
{
	BIO		*bio;
	DH		*dh;

	if (NULL == (bio = BIO_new_file(dh_fn, "r")))
	{
		SET_AND_APPEND_OPENSSL_ERROR("Unable to load Diffie-Hellman parameter file: %s.", dh_fn);
		return NULL;
	}
	if (NULL == (dh = (PEM_read_bio_DHparams(bio, NULL, NULL, NULL))))
	{
		SET_AND_APPEND_OPENSSL_ERROR("Unable to load Diffie-Hellman parameter file: %s.", dh_fn);
		return NULL;
	}
	return dh;
}

STATICFNDEF int init_dhparams(void)
{
	int		rv1, rv2;
	const char	*dh512_fn, *dh1024_fn;

	if (dh1024)
		return 0;	/* already have */
	rv1 = config_lookup_string(&gtm_tls_cfg, "tls.dh512", &dh512_fn);
	rv2 = config_lookup_string(&gtm_tls_cfg, "tls.dh1024", &dh1024_fn);
	if (!rv1 && !rv2)
		return 0;	/* No Diffie-Hellman parameters specified in the config file. */
	if (!rv1)
	{
		gtm_tls_set_error(NULL, "Configuration parameter `tls.dh512' not specified.");
		return -1;
	}
	if (!rv2)
	{
		gtm_tls_set_error(NULL, "Configuration parameter `tls.dh1024' not specified.");
		return -1;
	}
	if (NULL == (dh512 = read_dhparams(dh512_fn)))
		return -1;
	if (NULL == (dh1024 = read_dhparams(dh1024_fn)))
		return -1;
	return 0;
}

STATICFNDEF DH *tmp_dh_callback(SSL *ssl, int is_export, int keylength)
{
	assert(dh512 && dh1024 && ((512 == keylength) || (1024 == keylength)));
	return (512 == keylength) ? dh512 : dh1024;
}
#endif

int gtm_tls_errno(void)
{
	return tls_errno;
}

/* Interlude to snprintf that uses the appropriate buffer. Function can return bytes written for callers that care */
STATICFNDEF int gtm_tls_set_error(gtm_tls_socket_t *tlssocket, const char *format, ...)
{
	char	*errstr = NULL;
	va_list	var;
	int	ret = 0;

	assert(NULL != gtmtls_err_string);
	if (NULL != tlssocket)
	{	/* Separate DB and network error strings */
		if (NULL == tlssocket->errstr)
		{
			errstr = tlssocket->errstr = malloc(MAX_GTMCRYPT_ERR_STRLEN);
			if (NULL != errstr)
				errstr[0] = '\0';
		}
		assert(NULL != tlssocket->errstr);
	}
	if (NULL == errstr)
		errstr = (NULL != gtmtls_err_string)? gtmtls_err_string : gtmcrypt_err_string;
	va_start(var, format);
	ret = vsnprintf(errstr, MAX_GTMCRYPT_ERR_STRLEN, format, var);
	va_end(var);
	return ret;
}

/* Copies error messages set by the DB encryption routines shared by the SSL/TLS reference implementation. */
STATICFNDEF void gtm_tls_copy_gcerror(gtm_tls_socket_t *tlssocket)
{
	assert(strlen(gtmcrypt_err_string));	/* If no error why copy? */
	if ((NULL != tlssocket) && (NULL != tlssocket->errstr)) /* socket specific error string present */
		memcpy(tlssocket->errstr, gtmcrypt_err_string, MAX_GTMCRYPT_ERR_STRLEN);
	else if (gtmtls_err_string) /* TLS implementation's general error string */
		memcpy(gtmtls_err_string, gtmcrypt_err_string, MAX_GTMCRYPT_ERR_STRLEN);
}

const char *gtm_tls_get_error(gtm_tls_socket_t *tlssocket)
{
	if ((NULL != tlssocket) && (NULL != tlssocket->errstr))
		return tlssocket->errstr; /* socket specific error string present */
	if (gtmtls_err_string)
		return gtmtls_err_string; /* TLS implementation's general error string */
	return gtmcrypt_err_string;	  /* DB encryption error string */
}

int gtm_tls_version(int caller_version)
{
	return GTM_TLS_API_VERSION;
}

gtm_tls_ctx_t *gtm_tls_init(int version, int flags)
{
	const char		*CAfile = NULL, *CApath = NULL, *crl, *CAptr, *cipher_list, *options_string, *verify_mode_string;
	const char		*verify_level_string;
	char			*config_env, *parse_ptr, *optionendptr;
	int			rv, rv1, rv2, fips_requested, fips_enabled, verify_mode, parse_len, verify_level;
	int	 		use_plaintext_fallback = 0, use_pha_fallback = 0;
#	if (((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 4)) || (LIBCONFIG_VER_MAJOR > 1))
	int			verify_depth, session_timeout;
#	else
	long int		verify_depth, session_timeout;
#	endif
	long			options_mask, options_current, options_clear, verify_long, level_long, level_clear;
	SSL_CTX			*ctx;
	X509_STORE		*store;
	X509_LOOKUP		*lookup;
	config_t		*cfg;

	if (NULL != gtm_tls_ctx) /* Non-null implies a repeat call which is possible when using the external call interface */
		return gtm_tls_ctx;

	assert(GTM_TLS_API_VERSION >= version); /* Make sure the caller is using the right API version */
	if (NULL == (gtmtls_err_string = malloc(MAX_GTMCRYPT_ERR_STRLEN +  1)))
	{
		gtm_tls_set_error(NULL, "Unable to allocate error buffer for libgtmtls.so plugin");
		return NULL;
	} else
		gtmtls_err_string[0] = '\0';

	if (GTM_TLS_API_VERSION < version)
	{
		gtm_tls_set_error(NULL, "Version of libgtmtls.so plugin (%d) older than needed by caller (%d).",
					GTM_TLS_API_VERSION, version);
		return NULL;
	}
	/* Setup function pointers to symbols exported by libgtmshr.so. */
	if (0 != gc_load_gtmshr_symbols())
		return NULL;
	/* Initialize OpenSSL library */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	/* The following options are initialzied by default:
	 * 	OPENSSL_INIT_LOAD_CRYPTO_STRINGS
	 * 	OPENSSL_INIT_ADD_ALL_CIPHERS
	 * 	OPENSSL_INIT_ADD_ALL_DIGESTS
	 *	OPENSSL_INIT_LOAD_CONFIG as of 1.1.1 - second parameter is path to file; NULL means OS default
	 *	OPENSSL_INIT_ASYNC
	 *
	 * The following are manually added:
	 * 	OPENSSL_INIT_NO_ATEXIT - suppress OpenSSL's "atexit" handler (new OpenSSL 3.0)
	 */
#	ifndef OPENSSL_INIT_NO_ATEXIT
#	define OPENSSL_INIT_NO_ATEXIT 0
#	endif
#	define GTMTLS_STARTUP_OPTIONS	(OPENSSL_INIT_NO_ATEXIT)
	if (!OPENSSL_init_ssl(GTMTLS_STARTUP_OPTIONS, NULL))
	{	/* Can't use SET_AND_APPEND_OPENSSL_ERROR which needs the above to initialize error reporting functions */
		gtm_tls_set_error(NULL, "OpenSSL library initialization failed");
		return NULL;
	}
#else
	/* Initialize the SSL/TLS library, the algorithms/cipher suite and error strings. */
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
#endif
	/* Turn on FIPS mode if requested. */
	fips_enabled = FALSE;	/* most common case. */
	IS_FIPS_MODE_REQUESTED(fips_requested);
	if (fips_requested)
	{
		ENABLE_FIPS_MODE(rv, fips_enabled);
		if (-1 == rv)
			return NULL; /* Relevant error detail populated in the above macro. */
	}
	/* Setup a SSL context that allows TLSv1.x but no SSLv[23] (which is deprecated due to a great number of security
	 * vulnerabilities).
	 */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	if (NULL == (ctx = SSL_CTX_new(TLS_method())))
#else
	if (NULL == (ctx = SSL_CTX_new(SSLv23_method())))
#endif
	{
		SET_AND_APPEND_OPENSSL_ERROR("Failed to create an SSL context.");
		return NULL;
	}
	SSL_CTX_set_options(ctx, DEPRECATED_SSLTLS_PROTOCOLS);
	/* Read the configuration file for more configuration parameters. */
	cfg = &gtm_tls_cfg;
	config_init(cfg);
	if (NULL == (config_env = getenv("gtmcrypt_config")))
	{
		if (!(GTMTLS_OP_INTERACTIVE_MODE & flags))
		{	/* allow no config file if interactive for simple client usage */
			gtm_tls_set_error(NULL, ENV_UNDEF_ERROR, "gtmcrypt_config");
			SSL_CTX_free(ctx);
			return NULL;
		} else
			flags |= GTMTLS_OP_ABSENT_CONFIG;
	} else if (!config_read_file(cfg, config_env))
	{
		gtm_tls_set_error(NULL, "Failed to read config file: %s. At line: %d, %s.", config_env, config_error_line(cfg),
						config_error_text(cfg));
		SSL_CTX_free(ctx);
		config_destroy(cfg);
		return NULL;
	}
	if (!(GTMTLS_OP_ABSENT_CONFIG & flags))
	{	/* check for tls section */
		if (NULL == config_lookup(cfg, "tls"))
		{
			if ((GTMTLS_OP_INTERACTIVE_MODE & flags))
				flags |= GTMTLS_OP_ABSENT_CONFIG;
			else
			{
				gtm_tls_set_error(NULL, "No tls: section in config file: %s", config_env);
				SSL_CTX_free(ctx);
				config_destroy(cfg);
				return NULL;
			}
		}
	}
	/* Get global SSL configuration parameters */
	if ((config_lookup_int(cfg, "tls.plaintext-fallback", &use_plaintext_fallback) && (0 < use_plaintext_fallback)))
		flags |= GTMTLS_OP_PLAINTEXT_FALLBACK;	/* Enable Plaintext fallback */
	if ((config_lookup_int(cfg, "tls.post-handshake-fallback", &use_pha_fallback) && (0 < use_pha_fallback)))
		flags |= GTMTLS_OP_PHA_EXT_FALLBACK;	/* Enable PHA fallback */
	if (config_lookup_int(cfg, "tls.verify-depth", &verify_depth))
		SSL_CTX_set_verify_depth(ctx, verify_depth);
	if (CONFIG_TRUE == config_lookup_string(cfg, "tls.verify-mode", &verify_mode_string))
	{
		verify_long = 0;
		parse_ptr = parse_SSL_options(&gtm_ssl_verify_mode_list[0], SIZEOF(gtm_ssl_verify_mode_list),
					verify_mode_string, &verify_long, NULL);
		if (NULL != parse_ptr)
		{
			optionendptr = strstr((const char *)parse_ptr, OPTIONENDSTR);
			if (NULL == optionendptr)
				parse_len = strlen(parse_ptr);
			else
				parse_len = optionendptr - parse_ptr;
			gtm_tls_set_error(NULL, "Unknown verify-mode option: %.*s", parse_len, parse_ptr);
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		if (0 == strlen(verify_mode_string))
		{	/* Do not treat empty strings as SSL_VERIFY_NONE */
			gtm_tls_set_error(NULL, "verify-mode string '%s' is the null string", verify_mode_string);
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		verify_mode = (int)verify_long;
		if ((SSL_VERIFY_NONE != verify_mode) && !(verify_mode & SSL_VERIFY_PEER))
		{
			gtm_tls_set_error(NULL, "verify-mode string '%s' needs SSL_VERIFY_PEER "
					"to enable other options", verify_mode_string);
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		SSL_CTX_set_verify(ctx, verify_mode, NULL);
	} else
		flags |= GTMTLS_OP_ABSENT_VERIFYMODE;
	if (CONFIG_TRUE == config_lookup_string(cfg, "tls.verify-level", &verify_level_string))
	{
		level_long = GTMTLS_OP_VERIFY_LEVEL_MASK & flags;
		level_clear = 0;
		parse_ptr = parse_SSL_options(&gtm_ssl_verify_level_list[0], SIZEOF(gtm_ssl_verify_level_list),
					verify_level_string, &level_long, &level_clear);
		if (NULL != parse_ptr)
		{
			optionendptr = strstr((const char *)parse_ptr, OPTIONENDSTR);
			if (NULL == optionendptr)
				parse_len = strlen(parse_ptr);
			else
				parse_len = optionendptr - parse_ptr;
			gtm_tls_set_error(NULL, "Unknown verify-level option: %.*s", parse_len, parse_ptr);
			return NULL;
		}
		if (0 != level_clear)
		{
			verify_level = (int)level_clear;
			flags &= ~verify_level;
		}
		verify_level = (int)level_long;
		flags = (GTMTLS_OP_VERIFY_LEVEL_CMPLMNT & flags) | verify_level;
	} else
		flags |= GTMTLS_OP_VERIFY_LEVEL_DEFAULT;
	rv1 = config_lookup_string(cfg, "tls.CAfile", &CAfile);
	rv2 = config_lookup_string(cfg, "tls.CApath", &CApath);
	/* Setup trust locations for peer verifications. This adds on to any trust locations that was previously loaded. */
	if ((rv1 || rv2) && !SSL_CTX_load_verify_locations(ctx, CAfile, CApath))
	{
		if (rv1 && rv2)
		{
			SET_AND_APPEND_OPENSSL_ERROR("Failed to load CA verification locations (CAfile = %s; CApath = %s).",
							CAfile, CApath);
		} else
		{
			CAptr = rv1 ? CAfile : CApath;
			SET_AND_APPEND_OPENSSL_ERROR("Failed to load CA verification location: %s.", CAptr);
		}
		SSL_CTX_free(ctx);
		config_destroy(cfg);
		return NULL;
	}
	if (rv1 || rv2)
		flags |= GTMTLS_OP_CA_LOADED;
	/* Load the default verification paths as well. On most Unix distributions, the default path is set to /etc/ssl/certs. */
	if (!SSL_CTX_set_default_verify_paths(ctx))
	{
		SET_AND_APPEND_OPENSSL_ERROR("Failed to load default CA verification locations.");
		SSL_CTX_free(ctx);
		config_destroy(cfg);
		return NULL;
	}
	/* If a CRL is specified in the configuration file, add it to the cert store. */
	if (config_lookup_string(cfg, "tls.crl", &crl))
	{
		if (NULL == (store = SSL_CTX_get_cert_store(ctx)))
		{
			SET_AND_APPEND_OPENSSL_ERROR("Failed to get handle to internal certificate store.");
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		if (NULL == (lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file())))
		{
			SET_AND_APPEND_OPENSSL_ERROR("Failed to get handle to internal certificate store.");
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		if (0 == X509_LOOKUP_load_file(lookup, (char *)crl, X509_FILETYPE_PEM))
		{
			SET_AND_APPEND_OPENSSL_ERROR("Failed to add Certificate Revocation List %s to internal certificate store.",
							crl);
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);
	}
	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_BOTH);
	/* Set callbacks to called whenever a SSL session is added. */
	SSL_CTX_sess_set_new_cb(ctx, new_session_callback);
	/* Set session timeout value (in seconds). If the current time is greater than the session creation time + session
	 * timeout, the session is not reused. This is useful only on the server.
	 */
	if (config_lookup_int(cfg, "tls.session-timeout", &session_timeout))
	{
		if (0 >= session_timeout)
			SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
		else
			SSL_CTX_set_timeout(ctx, session_timeout);
	} else
		SSL_CTX_set_timeout(ctx, DEFAULT_SESSION_TIMEOUT);
	if (CONFIG_FALSE == config_lookup_string(cfg, "tls.cipher-list", &cipher_list))
		cipher_list = NULL;
	else if (('\0' != cipher_list[0]) && (0 >= SSL_CTX_set_cipher_list(ctx, cipher_list))
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
			&& (0 >= SSL_CTX_set_ciphersuites(ctx, cipher_list))
#endif
		)
	{
		SET_AND_APPEND_OPENSSL_ERROR("Failed to add Cipher-List command string: %s.", cipher_list);
		SSL_CTX_free(ctx);
		config_destroy(cfg);
		return NULL;
	}	/* use OpenSSL default */
	if (CONFIG_TRUE == config_lookup_string(cfg, "tls.ssl-options", &options_string))
	{
		options_mask = options_current = SSL_CTX_get_options(ctx);
		options_clear = 0;
		parse_ptr = parse_SSL_options(&gtm_ssl_options_list[0], SIZEOF(gtm_ssl_options_list), options_string,
					&options_mask, &options_clear);
		if (NULL != parse_ptr)
		{
			optionendptr = strstr((const char *)parse_ptr, OPTIONENDSTR);
			if (NULL == optionendptr)
				parse_len = strlen(parse_ptr);
			else
				parse_len = optionendptr - parse_ptr;
			gtm_tls_set_error(NULL, "Unknown ssl-options option: %.*s", parse_len, parse_ptr);
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		if (options_current != options_mask)
			options_mask = SSL_CTX_set_options(ctx, options_mask);
		if (0 != options_clear)
			options_mask = SSL_CTX_clear_options(ctx, options_clear);
	}

	/* Support for ECDHE Ciphers was added in OpenSSL 1.0.2 with the below.
	 * OpenSSL 1.1.0 made this the default and made the function a no-op
	 */
	SSL_CTX_set_ecdh_auto(ctx, 1);

	gtm_tls_ctx = MALLOC(SIZEOF(gtm_tls_ctx_t));
	gtm_tls_ctx->ctx = ctx;
	if (NULL == cipher_list)
		flags |= GTMTLS_OP_ABSENT_CIPHER;
	else if ('\0' == cipher_list[0])
		flags |= GTMTLS_OP_DEFAULT_CIPHER;
	gtm_tls_ctx->flags = flags;
	gtm_tls_ctx->fips_mode = fips_enabled;
	gtm_tls_ctx->compile_time_version = OPENSSL_VERSION_NUMBER;
	gtm_tls_ctx->runtime_version = SSLeay();
	gtm_tls_ctx->version = version;		/* GTM_TLS_API_VERSION of caller */
	if (GTM_TLS_API_VERSION_NONBLOCK <= version)
		gtm_tls_ctx->plugin_version = GTM_TLS_API_VERSION;
	return gtm_tls_ctx;
}

STATICFNDEF gtmtls_passwd_list_t *gtm_tls_find_pwent(const char *input_env_name)
{
	gtmtls_passwd_list_t	*pwent_node;
	char			*env_name_ptr;
	int			len, inputlen;

	inputlen = STRLEN(input_env_name);
	for (pwent_node = gtmtls_passwd_listhead; NULL != pwent_node; pwent_node = pwent_node->next)
	{	/* Lookup to see if we already have a password for the tlsid. */
		env_name_ptr = pwent_node->pwent->env_name;
		len = STRLEN(env_name_ptr);
		assert(len < PASSPHRASE_ENVNAME_MAX);
		assert(len > SIZEOF(GTMTLS_PASSWD_ENV_PREFIX) - 1);
		if ((len == inputlen) && (0 == strcmp(input_env_name, env_name_ptr)))
			break;	/* We already have a password for the tlsid. */
	}
	return pwent_node;
}

int gtm_tls_store_passwd(gtm_tls_ctx_t *tls_ctx, const char *tlsid, const char *obs_passwd)
{
	char			env_name[PASSPHRASE_ENVNAME_MAX];
	size_t			env_name_idx;
	size_t			env_name_len, obs_len;
	gtmtls_passwd_list_t	*pwent_node;
	passwd_entry_t		*pwent;

	assert(tls_ctx);
	if (!(GTMTLS_OP_INTERACTIVE_MODE & tls_ctx->flags))
		return 0;	/* Not running in an interactive mode. */
	assert(NULL != tlsid);
	env_name_len = strnlen(tlsid, (PASSPHRASE_ENVNAME_MAX - sizeof(GTMTLS_PASSWD_ENV_PREFIX))); /* Includes null */
	env_name_idx = (sizeof(GTMTLS_PASSWD_ENV_PREFIX) - 1);
	memcpy(env_name, GTMTLS_PASSWD_ENV_PREFIX, env_name_idx);
	memcpy(&env_name[env_name_idx], tlsid, env_name_len);
	env_name_idx += env_name_len;
	assert(PASSPHRASE_ENVNAME_MAX > env_name_idx);
	env_name[env_name_idx] = '\0';
	obs_len = strlen(obs_passwd);
	pwent_node = gtm_tls_find_pwent(env_name);
	if (NULL != pwent_node)
	{
		pwent = pwent_node->pwent;
		if ((obs_len == (size_t)(pwent->passwd_len * 2)) && (0 == strncmp(obs_passwd, pwent->passwd, obs_len)))
			return 1;	/* already on the list */
	}
	/* Either no entry for tlsid or need to replace with new value */
	pwent = MALLOC(sizeof(passwd_entry_t));
	assert(NULL != pwent);
	strcpy(pwent->env_name, env_name);
	pwent->env_value = MALLOC(obs_len + 1);
	memcpy(pwent->env_value, obs_passwd, obs_len + 1);	/* include null */
	pwent->passwd = NULL;
	pwent->passwd_len = 0;
	if (0 == gc_update_passwd(pwent->env_name, &pwent, NULL, GTMTLS_OP_NOPWDENVVAR))
	{
		pwent_node = MALLOC(SIZEOF(gtmtls_passwd_list_t));
		pwent_node->next = gtmtls_passwd_listhead;
		pwent_node->pwent = pwent;
		gtmtls_passwd_listhead = pwent_node;
	} else
	{
		gtm_tls_copy_gcerror(NULL);
		return -1;		/* gc_update_passwd freed pwent */
	}
	return 1;
}

void gtm_tls_prefetch_passwd(gtm_tls_ctx_t *tls_ctx, char *env_name)
{
	char			*env_name_ptr, *env_value, prompt[GTM_PASSPHRASE_MAX_ASCII + 1];
	gtmtls_passwd_list_t	*pwent_node;
	passwd_entry_t		*pwent;

	assert((NULL != (env_value = getenv(env_name))) && (0 == STRLEN(env_value)));
	if (!(GTMTLS_OP_INTERACTIVE_MODE & tls_ctx->flags))
		return;	/* Not running in an interactive mode. Cannot prompt for password. */
	env_name_ptr = (char *)env_name;
	assert(PASSPHRASE_ENVNAME_MAX > STRLEN(env_name_ptr));
	assert(SIZEOF(GTMTLS_PASSWD_ENV_PREFIX) - 1 < STRLEN(env_name_ptr));
	env_name_ptr += (SIZEOF(GTMTLS_PASSWD_ENV_PREFIX) - 1);
	SNPRINTF(prompt, GTM_PASSPHRASE_MAX_ASCII, "Enter passphrase for TLSID %s: ", env_name_ptr);
	pwent = NULL;
	if (0 == gc_update_passwd(env_name, &pwent, prompt, TRUE))
	{
		pwent_node = MALLOC(SIZEOF(gtmtls_passwd_list_t));
		pwent_node->next = gtmtls_passwd_listhead;
		pwent_node->pwent = pwent;
		gtmtls_passwd_listhead = pwent_node;
	} else	/* Later, gtm_tls_socket makes another attempt to acquire the password. Keep the error just in case */
		gtm_tls_copy_gcerror(NULL);
}

static int copy_tlsid_elem(const config_t *tmpcfg, config_t *cfg, config_setting_t *tlsid, const char *idstr, const char *elemname,
				int type);
static int copy_tlsid_elem(const config_t *tmpcfg, config_t *cfg, config_setting_t *tlsid, const char *idstr, const char *elemname,
				int type)
{
	config_setting_t	*srcelem, *elem;
	char			cfg_path[MAX_CONFIG_LOOKUP_PATHLEN];

	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.%s", idstr, elemname);
	if ((srcelem = config_lookup(tmpcfg, cfg_path)))
	{
		elem = config_lookup(cfg, cfg_path);
		if (NULL == elem)
		{	/* option not currently in tls.idstr so create */
			elem = config_setting_add(tlsid, elemname, type);
			if (NULL == elem)
			{
				gtm_tls_set_error(NULL, "Failed to add TLSID: %s item %s to config file: %s",
						idstr, elemname, config_error_text(cfg));
				return -1;
			}
		}
		if (CONFIG_TYPE_STRING == type)
			config_setting_set_string(elem, config_setting_get_string(srcelem));
		else if (CONFIG_TYPE_INT == type)
		{
			config_setting_set_int(elem, config_setting_get_int(srcelem));
			config_setting_set_format(elem, config_setting_get_format(srcelem));
		} else
		{
			gtm_tls_set_error(NULL, "gtm_tls_impl.c/copy_tlsid_elem:  Unexpected CONFIG_TYPE %d for item %s",
				type, elemname);
			return -1;
		}
	}
	return 0;
}

int gtm_tls_add_config(gtm_tls_ctx_t *tls_ctx, const char *idstr, const char *configstr)
{
#	ifndef LIBCONFIG_VER_MAJOR
	gtm_tls_set_error(NULL, "TLSID: %s: libconfig 1.4.x is needed to support adding config information", idstr);
	return -1;
#	else
	config_t		*cfg, tmpcfg;
	config_setting_t	*tlsid, *tlssect;
	char			cfg_path[MAX_CONFIG_LOOKUP_PATHLEN];

	config_init(&tmpcfg);
	if (CONFIG_FALSE == config_read_string(&tmpcfg, configstr))
	{
		gtm_tls_set_error(NULL, "Failed to add config information: %s in line %d:\n%s",
			config_error_text(&tmpcfg), config_error_line(&tmpcfg), configstr);
		return -1;
	}
	cfg = &gtm_tls_cfg;
	strncpy(cfg_path, "tls", SIZEOF("tls"));
	tlssect = config_lookup(cfg, cfg_path);
	if (NULL == tlssect)
	{	/* need to add tls section */
		tlssect = config_setting_add(config_root_setting(cfg), "tls", CONFIG_TYPE_GROUP);
		if (NULL == tlssect)
		{
			gtm_tls_set_error(NULL, "Failed to add tls section to config file: %s",
				config_error_text(cfg));
			return -1;
		}
	}
	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s", idstr);
	tlsid = config_lookup(cfg, cfg_path);
	if (NULL == tlsid)
	{	/* add new section named tls.idstr */
		tlsid = config_setting_add(tlssect, idstr, CONFIG_TYPE_GROUP);
		if (NULL == tlsid)
		{
			gtm_tls_set_error(NULL, "Failed to add TLSID: %s section to config file: %s",
				idstr, config_error_text(cfg));
			return -1;
		}
	}
	/* add any new gtm_tls_socket level options below */
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "verify-mode", CONFIG_TYPE_STRING))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "cipher-list", CONFIG_TYPE_STRING))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "cert", CONFIG_TYPE_STRING))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "key", CONFIG_TYPE_STRING))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "format", CONFIG_TYPE_STRING))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "plaintext-fallback", CONFIG_TYPE_INT))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "post-handshake-fallback", CONFIG_TYPE_INT))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "ssl-options", CONFIG_TYPE_STRING))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "verify-depth", CONFIG_TYPE_INT))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "verify-level", CONFIG_TYPE_STRING))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "session-id-hex", CONFIG_TYPE_STRING))
		return -1;
	if (-1 == copy_tlsid_elem(&tmpcfg, cfg, tlsid, idstr, "CAfile", CONFIG_TYPE_STRING))
		return -1;
	config_destroy(&tmpcfg);
	return 0;
#	endif
}

gtm_tls_socket_t *gtm_tls_socket(gtm_tls_ctx_t *tls_ctx, gtm_tls_socket_t *prev_socket, int sockfd, char *id, int flags)
{
	int			len, verify_mode, verify_mode_set, nocert, nopkey, parse_len, verify_level, verify_level_set;
	int			tlscafile;
	int			session_id_len;
	int	 		use_plaintext_fallback, use_pha_fallback;
	long			options_mask, options_current, options_clear, verify_long, level_long, level_clear;
	char			*optionendptr, *parse_ptr;
#	if (((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 4)) || (LIBCONFIG_VER_MAJOR > 1))
	int			verify_depth, session_timeout;
#	else
	long int		verify_depth, session_timeout;
#	endif
	char			cfg_path[MAX_CONFIG_LOOKUP_PATHLEN], input_env_name[PASSPHRASE_ENVNAME_MAX + 1], *env_name_ptr;
	char			prompt[GTM_PASSPHRASE_MAX_ASCII + 1];
	const char		*cert, *private_key, *format, *cipher_list, *options_string, *verify_mode_string;
	const char		*verify_level_string, *session_id_hex, *CAfile = NULL;
	unsigned char		session_id_string[SSL_MAX_SSL_SESSION_ID_LENGTH];
	FILE			*fp;
	SSL			*ssl;
	SSL_CTX			*ctx;
	EVP_PKEY		*evp_pkey = NULL;
	config_t		*cfg;
	config_setting_t	*cfg_setting;
	gtmtls_passwd_list_t	*pwent_node;
	passwd_entry_t		*pwent;
	gtm_tls_socket_t	*socket;
	STACK_OF(X509_NAME)	*CAcerts;
#	ifndef SSL_OP_NO_COMPRESSION
	STACK_OF(SSL_COMP)*	compression;
#	endif

	ctx = tls_ctx->ctx;
	cfg = &gtm_tls_cfg;

	/* Create a SSL object. This object will be used for the actual I/O: recv/send */
	if (NULL == (ssl = SSL_new(ctx)))
	{
		SET_AND_APPEND_OPENSSL_ERROR("Failed to obtain a new SSL/TLS object.");
		return NULL;
	}

	if ('\0' != id[0])
	{
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s", id);
		cfg_setting = config_lookup(cfg, cfg_path);
		if (NULL == cfg_setting)
		{
			gtm_tls_set_error(NULL, "TLSID %s not found in configuration file.", id);
			SSL_free(ssl);
			return NULL;
		}
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.verify-mode", id);
		if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &verify_mode_string))
		{
			verify_long = 0;
			parse_ptr = parse_SSL_options(&gtm_ssl_verify_mode_list[0], SIZEOF(gtm_ssl_verify_mode_list),
						verify_mode_string, &verify_long, NULL);
			if (NULL != parse_ptr)
			{
				optionendptr = strstr((const char *)parse_ptr, OPTIONENDSTR);
				if (NULL == optionendptr)
					parse_len = strlen(parse_ptr);
				else
					parse_len = optionendptr - parse_ptr;
				gtm_tls_set_error(NULL, "In TLSID: %s - unknown verify-mode option: %.*s",
						id, parse_len, parse_ptr);
				SSL_free(ssl);
				return NULL;
			}
			if (0 == strlen(verify_mode_string))
			{	/* Do not treat empty strings as SSL_VERIFY_NONE */
				gtm_tls_set_error(NULL, "verify-mode string '%s' is the null string", verify_mode_string);
				SSL_CTX_free(ctx);
				config_destroy(cfg);
				return NULL;
			}
			verify_mode = (int)verify_long;
			if ((SSL_VERIFY_NONE != verify_mode) && !(verify_mode & SSL_VERIFY_PEER))
			{
				gtm_tls_set_error(NULL, "In TLSID: %s - verify-mode string '%s' needs SSL_VERIFY_PEER "
						"to enable other options", id, verify_mode_string);
				SSL_free(ssl);
				return NULL;
			}
			verify_mode_set = TRUE;
		} else if (GTMTLS_OP_ABSENT_VERIFYMODE & tls_ctx->flags)
		{
			verify_mode = SSL_VERIFY_PEER;
			if ((flags & GTMTLS_OP_FORCE_VERIFY_PEER) && (!CLIENT_MODE(flags)))
				verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
			verify_mode_set = TRUE;
		} else
		{
			verify_mode_set = FALSE;
			verify_mode = SSL_VERIFY_NONE;
		}
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.verify-level", id);
		if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &verify_level_string))
		{
			level_long = GTMTLS_OP_VERIFY_LEVEL_MASK & tls_ctx->flags;
			level_clear = 0;
			parse_ptr = parse_SSL_options(&gtm_ssl_verify_level_list[0], SIZEOF(gtm_ssl_verify_level_list),
						verify_level_string, &level_long, &level_clear);
			if (NULL != parse_ptr)
			{
				optionendptr = strstr((const char *)parse_ptr, OPTIONENDSTR);
				if (NULL == optionendptr)
					parse_len = strlen(parse_ptr);
				else
					parse_len = optionendptr - parse_ptr;
				gtm_tls_set_error(NULL, "In TLSID: %s - unknown verify-level option: %.*s",
					id, parse_len, parse_ptr);
				SSL_free(ssl);
				return NULL;
			}
			if (0 != level_clear)
				level_long &= ~level_clear;
			verify_level = (int)level_long;
			verify_level_set = TRUE;
		} else
			verify_level_set = FALSE;
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.cipher-list", id);
		if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &cipher_list))
		{
			if ('\0' == cipher_list[0]) /* use default instead of tls.cipher-list if empty string */
				cipher_list = (GTMTLS_OP_SOCKET_DEV & flags) ? GTM_DEFAULT_CIPHER_LIST : REPL_CIPHER_LIST;
		} else
			cipher_list = NULL;
	} else if (!CLIENT_MODE(flags))
	{	/* server mode needs certificate and thus tlsid */
		gtm_tls_set_error(NULL, "Server mode requires a certificate but no TLSID specified");
		SSL_free(ssl);
		return NULL;
	} else
	{	/* No TLS ID supplied */
		assert(GTMTLS_OP_SOCKET_DEV & flags);
		cipher_list = NULL;
		if (GTMTLS_OP_ABSENT_VERIFYMODE & tls_ctx->flags)
		{
			verify_mode = SSL_VERIFY_PEER;
			verify_mode_set = TRUE;
		} else
			verify_mode_set = FALSE;
		verify_level_set = FALSE;
	}
	verify_mode = (verify_mode_set) ? verify_mode : SSL_get_verify_mode(ssl); /* Use parent verify_mode if not set by TLSID */
	if (SSL_VERIFY_NONE != verify_mode)
	{
		assert((CLIENT_MODE(flags)) || (verify_mode & SSL_VERIFY_PEER));
		if (CLIENT_MODE(flags))
		{	/* In client mode, options other than SSL_VERIFY_PEER are supposed to be ignored, but may be treated like
			 * SSL_VERIFY_PEER. The man pages recommend removing all other options. Do this silently to easily share
			 * client and server settings
			 */
			verify_mode_set = (~SSL_VERIFY_PEER & verify_mode);
			verify_mode &= SSL_VERIFY_PEER;
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
			/* When SSL_VERIFY_PEER is set, enable PHA in client mode. Doing so just sends the PHA extension in the
			 * CLIENT_HELLO. It is up to the server to request and validate PHA. OpenSSL handles presenting the client
			 * ceritificate(s) without any explicit action. Enabling PHA is harmless for pre-TLSv1.3
			 */
			if (SSL_VERIFY_PEER | verify_mode)
				SSL_set_post_handshake_auth(ssl, 1);
#endif
		} else if (GTMTLS_OP_PHA_EXT_NOT_RECEIVED & flags)
		{	/* Receiver Server disabled PHA after an error */
			assert(!CLIENT_MODE(flags));
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
			verify_mode_set = (SSL_VERIFY_POST_HANDSHAKE | verify_mode);
			verify_mode &= ~SSL_VERIFY_POST_HANDSHAKE;
#else
			assert(FALSE); /* Should not get here with prior OpenSSL versions */
#endif
		}
	}
	if (verify_mode_set)
		SSL_set_verify(ssl, verify_mode, NULL);
	if (verify_level_set)
		flags = (GTMTLS_OP_VERIFY_LEVEL_CMPLMNT & flags) | verify_level;
	else
		flags = (GTMTLS_OP_VERIFY_LEVEL_CMPLMNT & flags) | (GTMTLS_OP_VERIFY_LEVEL_MASK & tls_ctx->flags);
	if (NULL == cipher_list)
	{	/* no cipher-list in labelled section or no section */
		if (0 != ((GTMTLS_OP_ABSENT_CIPHER | GTMTLS_OP_DEFAULT_CIPHER) & tls_ctx->flags))
		{	/* no or default cipher specified top level */
			cipher_list = (GTMTLS_OP_SOCKET_DEV & flags) ? GTM_DEFAULT_CIPHER_LIST : REPL_CIPHER_LIST;
		}
	}
	if ((NULL != cipher_list) && (0 >= SSL_set_cipher_list(ssl, cipher_list))
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
			&& (0 >= SSL_set_ciphersuites(ssl, cipher_list))
#endif
	   )
	{
		SET_AND_APPEND_OPENSSL_ERROR("Failed to add Cipher-List command string: %s.", cipher_list);
		SSL_free(ssl);
		return NULL;
	}
	if ('\0' != id[0])
	{
		/* First lookup the certificate and private key associated with the provided id in the configuration file. */
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.cert", id);
		if (!config_lookup_string(cfg, cfg_path, &cert))
		{
			if (!CLIENT_MODE(flags))
			{
				if (NULL == config_root_setting(cfg))
				{	/* not sure this is possible */
					gtm_tls_set_error(NULL, "Certificate required for TLSID: %s"
						" but no configuration information available.", id);
				} else
				{
					gtm_tls_set_error(NULL, "Certificate corresponding to TLSID: %s"
						" not found in configuration file.", id);
				}
				SSL_free(ssl);
				return NULL;
			} else
				nocert = TRUE;
		} else
			nocert = FALSE;
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.key", id);
		if (config_lookup_string(cfg, cfg_path, &private_key))
		{
			if (nocert)
			{
				gtm_tls_set_error(NULL, "Private key but no certificate corresponding to TLSID:"
					" %s in configuration file.", id);
				SSL_free(ssl);
				return NULL;
			}
		} else if (!nocert)
			private_key = cert;	/* assume both in one file */
		/* Verify that the format, if specified, is of PEM type as that's the only kind we support now. */
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.format", id);
		if (config_lookup_string(cfg, cfg_path, &format))
		{
			if (nocert)
			{
				gtm_tls_set_error(NULL, "Format but no certificate corresponding to TLSID: %s"
					" in configuration file.", id);
				SSL_free(ssl);
				return NULL;
			}
			if (((SIZEOF("PEM") - 1) != strlen(format))
				|| (format[0] != 'P') || (format[1] != 'E') || (format[2] != 'M'))
			{
				gtm_tls_set_error(NULL, "Unsupported format type %s found for TLSID: %s.",
					format, id);
				SSL_free(ssl);
				return NULL;
			}
		}
		if (!nocert)
		{
			/* Setup the certificate to be used for this connection */
			if (!SSL_use_certificate_file(ssl, cert, SSL_FILETYPE_PEM))
			{
				SET_AND_APPEND_OPENSSL_ERROR("Failed to add certificate %s.", cert);
				SSL_free(ssl);
				return NULL;
			}
			/* Before setting up the private key, check-up on the password for the private key. */
			SNPRINTF(input_env_name, PASSPHRASE_ENVNAME_MAX, GTMTLS_PASSWD_ENV_PREFIX "%s", id);
			/* Lookup to see if we have already prefetched the password. */
			pwent_node = gtm_tls_find_pwent(input_env_name);
			if (NULL == pwent_node)
			{	/* Lookup failed. Create a new entry for the given id. */
				pwent = NULL;
				SNPRINTF(prompt, GTM_PASSPHRASE_MAX_ASCII, "Enter passphrase for TLSID %s:", id);
				if (0 != gc_update_passwd(input_env_name, &pwent, prompt, 0))
				{
					gtm_tls_copy_gcerror(NULL);
					SSL_free(ssl);
					return NULL;
				}
				pwent_node = MALLOC(SIZEOF(gtmtls_passwd_list_t));
				pwent_node->next = gtmtls_passwd_listhead;
				pwent_node->pwent = pwent;
				gtmtls_passwd_listhead = pwent_node;
			} else
				pwent = pwent_node->pwent;
			assert((NULL != pwent) && (NULL != pwent_node));
			/* Setup the private key corresponding to the certificate and the callback function to obtain
		 	* the password for the key. We cannot use the much simpler SSL_use_PrivateKey file to load
		 	* the private key file because we want fine grained control on the password callback mechanism.
		 	* For this purpose, use the PEM_read_PrivateKey function which
		 	* supports callbacks for individual private keys.
		 	*/
			fp = fopen(private_key, "r");
			if (NULL != fp)
			{
				evp_pkey = PEM_read_PrivateKey(fp, &evp_pkey, &passwd_callback, pwent);
				fclose(fp);
			} else
				evp_pkey = NULL;
			if (NULL == evp_pkey)
			{
				if (NULL == fp)
				{
					gtm_tls_set_error(NULL, "Private Key corresponding to TLSID:"
						" %s - error opening file %s: %s.", id, private_key, strerror(errno));
				} else if (ERR_GET_REASON(ERR_peek_error()) == PEM_R_NO_START_LINE)
				{	/* give clearer error if only cert given but it doesn't have the key */
					gtm_tls_set_error(NULL, "Private Key corresponding to TLSID:"
						" %s not found in configuration file.", id);
				} else
				{
					SET_AND_APPEND_OPENSSL_ERROR("Failed to read private key %s.", private_key);
				}
				SSL_free(ssl);
				return NULL;
			}
			if (!SSL_use_PrivateKey(ssl, evp_pkey))
			{
				SET_AND_APPEND_OPENSSL_ERROR("Failed to use private key %s.", private_key);
				SSL_free(ssl);
				return NULL;
			}
			/* Verify that private key matches the certificate */
			if (!SSL_check_private_key(ssl))
			{
				SET_AND_APPEND_OPENSSL_ERROR("Consistency check failed for private key: %s and certificate: %s\n",
						private_key, cert);
				SSL_free(ssl);
				return NULL;
			}
		}
	}
#	ifdef SSL_OP_ALLOW_CLIENT_RENEGOTIATION
	/* The Receiver Server does this to let the Source Server initiate renegotiation. OpenSSL 3.0 requires the
	 * server side to explicitly allow client side initiated renegotiations. This was done to mitigate the
	 * possibility of a denial-of-service attack caused by a client repeatedly requesting renegotiation. This should
	 * not be a problem for replication since the Source and Receiver Servers negotiate a number of parameters
	 * before starting TLS (protection by protocol).
	 */
	if (flags & GTMTLS_OP_RENEGOTIATE_REQUESTED)
	{
		assert(!CLIENT_MODE(flags));
		SSL_set_options(ssl, SSL_OP_ALLOW_CLIENT_RENEGOTIATION);
	}
#	endif
	/* OpenSSL does not recommend enabling compression as the current state of the SSL/TLS protocol does not specify identifiers
	 * for compression libraries thereby allowing for incompatibilities when different SSL/TLS implementations are used in the
	 * client and the server. So, disable compression.
	 */
#	ifdef SSL_OP_NO_COMPRESSION
	SSL_set_options(ssl, SSL_OP_NO_COMPRESSION);
#	else
	/* OpenSSL versions <= 0.9.8 enabled compression by default and did not provide an easy way to turn-off compression. Below
	 * is a work-around that zeroes out all the compression methods explicitly in the compression structures effectively
	 * disabling compression.
	 */
	compression = SSL_COMP_get_compression_methods();
	sk_SSL_COMP_zero(compression);
#	endif
	/* When SSL_MODE_AUTO_RETRY is set, SSL_read, SSL_write, and others will not return when interrupted.  It can also cause
	 * hangs when used with select()/poll() (see SSL_set_mode man page.)
	 */
	/* While SSL_clear_mode was't documented until OpenSSL 1.1.1, it has been defined in openssl/ssl.h since at least 1.0.1d */
#	ifdef SSL_clear_mode
		SSL_clear_mode(ssl, SSL_MODE_AUTO_RETRY);	/* on by default since 1.1.1 */
#	endif
	if (GTMTLS_OP_NONBLOCK_WRITE & flags)
	{
		SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
	}
	/* Carry forward the main configuration fallback settings */
	use_plaintext_fallback = (GTMTLS_OP_PLAINTEXT_FALLBACK & tls_ctx->flags);
	use_pha_fallback = (GTMTLS_OP_PHA_EXT_FALLBACK & tls_ctx->flags);
	if ('\0' != id[0])
	{
		/* use_(plaintext|pha)_fallback are unchanged if not present */
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.plaintext-fallback", id);
		(void)config_lookup_int(cfg, cfg_path, &use_plaintext_fallback);
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.post-handshake-fallback", id);
		(void)config_lookup_int(cfg, cfg_path, &use_pha_fallback);
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.ssl-options", id);
		if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &options_string))
		{
			options_mask = options_current = SSL_get_options(ssl);
			options_clear = 0;
			parse_ptr = parse_SSL_options(&gtm_ssl_options_list[0], SIZEOF(gtm_ssl_options_list), options_string,
					&options_mask, &options_clear);
			if (NULL != parse_ptr)
			{
				optionendptr = strstr((const char *)parse_ptr, OPTIONENDSTR);
				if (NULL == optionendptr)
					parse_len = strlen(parse_ptr);
				else
					parse_len = optionendptr - parse_ptr;
				gtm_tls_set_error(NULL, "In TLSID: %s - unknown ssl-options option: %.*s",
						id, parse_len, parse_ptr);
				SSL_free(ssl);
				return NULL;
			}
			if (options_current != options_mask)
				options_mask = SSL_set_options(ssl, options_mask);
			if (0 != options_clear)
				options_mask = SSL_clear_options(ssl, options_clear);

		}
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.verify-depth", id);
		if (CONFIG_TRUE == config_lookup_int(cfg, cfg_path, &verify_depth))
			SSL_set_verify_depth(ssl, verify_depth);
	}
	if (0 < use_plaintext_fallback)
		flags |= GTMTLS_OP_PLAINTEXT_FALLBACK;	/* Enable Plaintext fallback */
	else
		flags &= ~GTMTLS_OP_PLAINTEXT_FALLBACK;	/* Disable Plaintext fallback */
	if (0 < use_pha_fallback)
		flags |= GTMTLS_OP_PHA_EXT_FALLBACK;	/* Enable PHA fallback */
	else
		flags &= ~GTMTLS_OP_PHA_EXT_FALLBACK;	/* Disable PHA fallback */
	if (!CLIENT_MODE(flags))
	{	/* Socket created for server mode operation. Set a session ID context for session resumption at the time of
		 * reconnection.
		 */
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.session-id-hex", id);
		if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &session_id_hex))
		{	/* convert hex to char and set len */
			session_id_len = (int)strnlen(session_id_hex, (size_t)MAX_SESSION_ID_LEN);
			assert((0 < session_id_len) && (MAX_SESSION_ID_LEN >= session_id_len));
			GC_UNHEX(session_id_hex, session_id_string, session_id_len);
			if (-1 == session_id_len)
			{
				gtm_tls_set_error(NULL, "In TLSID: %s - invalid session-id-hex value: %s",
					id, session_id_hex);
				tls_errno = -1;
				SSL_free(ssl);
				return NULL;
			}
			session_id_len = session_id_len / 2;		/* bytes */
		} else
		{
			session_id_len = (int)strnlen(id, (size_t)(SSL_MAX_SSL_SESSION_ID_LENGTH - 1));
			assert((0 < session_id_len) && (SSL_MAX_SSL_SESSION_ID_LENGTH > session_id_len));
			memcpy((char *)session_id_string, id, (size_t)session_id_len);		/* default to tlsid */
			session_id_string[session_id_len] = '\0';
		}
		if (0 >= SSL_set_session_id_context(ssl, (const unsigned char *)session_id_string, (unsigned int)session_id_len))
		{
			SET_AND_APPEND_OPENSSL_ERROR("Failed to set Session-ID context to enable session resumption.");
			SSL_free(ssl);
			return NULL;
		}
#if OPENSSL_VERSION_NUMBER < 0x30000000L
		/* Set up Ephemeral Diffie-Hellman key exchange callback. This callback is invoked whenever, during the connection
	 	* time, OpenSSL requires Diffie-Hellman key parameters. The SSL_OP_SINGLE_DH_USE is turned on so that the same
	 	* private key is not used for each session. This means a little extra computation during the time of handshake, but
	 	* is recommended by the OpenSSL community.
	 	*/
		if (-1 == init_dhparams())
		{
			SSL_free(ssl);
			return NULL;
		}
		if (dh512 && dh1024)
		{
			SSL_set_options(ssl, SSL_OP_SINGLE_DH_USE);
			SSL_set_tmp_dh_callback(ssl, tmp_dh_callback);
		}
#else
		SSL_set_dh_auto(ssl, 1);
#endif
	}
	tlscafile = config_lookup_string(cfg, "tls.CAfile", &CAfile);
	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.CAfile", id);
	config_lookup_string(cfg, cfg_path, &CAfile);	/* if absent CAfile retains value */
	if (NULL != CAfile)
	{
		if (!(GTMTLS_OP_CA_LOADED & tls_ctx->flags))
		{	/* no CAfile or CApath before so do now */
			if (!SSL_CTX_load_verify_locations(tls_ctx->ctx, CAfile, NULL))
			{
				SET_AND_APPEND_OPENSSL_ERROR("Failed to load CA verification location: %s.", CAfile);
				SSL_free(ssl);
				return NULL;
			}
			tls_ctx->flags |= GTMTLS_OP_CA_LOADED;
		}
		if (!CLIENT_MODE(flags))
		{	/* these SSL calls are server side only */
			CAcerts = SSL_load_client_CA_file(CAfile);
			if (NULL == CAcerts)
			{
				SET_AND_APPEND_OPENSSL_ERROR("Failed to load client CA file %s", CAfile);
				SSL_free(ssl);
				return NULL;
			}
			SSL_set_client_CA_list(ssl, CAcerts);
			flags |= GTMTLS_OP_CLIENT_CA;
		}
	}
	/* Finally, wrap the Unix TCP/IP socket into SSL/TLS object */
	if (0 >= SSL_set_fd(ssl, sockfd))
	{
		SET_AND_APPEND_OPENSSL_ERROR("Failed to associate TCP/IP socket descriptor %d with an SSL/TLS descriptor", sockfd);
		SSL_free(ssl);
		return NULL;
	}
	if (NULL == prev_socket)
	{
		socket = MALLOC(SIZEOF(gtm_tls_socket_t));
		socket->session = NULL;
	} else
		socket = prev_socket;
	socket->flags = flags;
	socket->ssl = ssl;
	socket->gtm_ctx = tls_ctx;
	session_id_len = (int)strnlen(id, (size_t)MAX_TLSID_LEN);
	strncpy(socket->tlsid, (const char *)id, MAX_TLSID_LEN);
	socket->tlsid[session_id_len] = '\0';
	socket->errstr = NULL;
	/* Now, store the `socket' structure in the `SSL' structure so that we can get it back in a callback that receives an
	 * `SSL' structure. Ideally, we should be using SSL_set_ex_data/SSL_get_ex_data family of functions. But, these functions
	 * operate on a specific index (obtained by calling SSL_get_ex_new_index). But, since the library should potentially
	 * support multiple sockets, we need to be able to associate the index returned by OpenSSL with a structure which is not
	 * straightforward. So, use OpenSSL's compatibility interface which lets the application store *one* piece of application
	 * data in the SSL structure for later retrieval. It does so by reserving index '0' for the compatibility interface.
	 */
	SSL_set_app_data(ssl, socket);	/* This way, we can get back the `socket' structure in a callback that receives `SSL'. */
	return socket;
}

int gtm_tls_connect(gtm_tls_socket_t *socket)
{
	int		rv;
	long		verify_result;

	assert(CLIENT_MODE(socket->flags));
	if (NULL != socket->session)
	{	/* Old session available. Reuse it. */
		SSL_DPRINT(stderr, "gtm_tls_connect(1): references=%d\n", ((SSL_SESSION *)(socket->session))->references);
		if (0 >= (rv = SSL_set_session(socket->ssl, socket->session)))
			return ssl_error(socket, rv, X509_V_OK);
		SSL_DPRINT(stderr, "gtm_tls_connect(2): references=%d\n", ((SSL_SESSION *)(socket->session))->references);
	}
	if (0 < (rv = SSL_connect(socket->ssl)))
	{
		if (NULL != socket->session)
			SSL_DPRINT(stderr, "gtm_tls_connect(3): references=%d\n", ((SSL_SESSION *)(socket->session))->references);
		verify_result = SSL_get_verify_result(socket->ssl);
		if (X509_V_OK == verify_result)
			return 0;
	} else
		verify_result = SSL_get_verify_result(socket->ssl);
	return ssl_error(socket, rv, verify_result);
}

int gtm_tls_accept(gtm_tls_socket_t *socket)
{
	int		rv;
	long		verify_result;

	assert(!CLIENT_MODE(socket->flags));
	rv = SSL_accept(socket->ssl);
	verify_result = SSL_get_verify_result(socket->ssl);
	if ((0 < rv) && (X509_V_OK == verify_result))
			return 0;
	return ssl_error(socket, rv, verify_result);
}

/* gtm_tls_did_post_hand_shake returns TRUE if the post handshake complete and the client certificate is available */
int gtm_tls_did_post_hand_shake(gtm_tls_socket_t *socket)
{
	X509			*peer;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	return (NULL != SSL_get0_peer_certificate(socket->ssl));
#elif OPENSSL_VERSION_NUMBER >= 0x10101000L
	peer = SSL_get_peer_certificate(socket->ssl);
	X509_free(peer);
	return (NULL != peer);
#else
	return FALSE;
#endif
}

/* Call SSL_verify_client_post_handshake() when the connection uses TLSv1.3 and PHA was requested in the server's configuration.
 * This function embeds a call to perform a handshake which might need a retry if the socket is non-blocking.
 * NOTE: When TLSv1.3 and PHA do not apply, the function returns a success status as the embedded handshake returns with an
 * error status only when the connection needs an extra READ/WRITE.
 */
int gtm_tls_do_post_hand_shake(gtm_tls_socket_t *socket)
{
	int		rv;
	long		verify_result;

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	/* Neither TLSv1.3 nor SSL_VERIFY_{PEER,POST_HANDSHAKE} combo requested. Skip PHA */
	if ((TLS1_3_VERSION != SSL_version(socket->ssl))
			|| (PHA_MACROS_ENABLED != (PHA_MACROS_ENABLED & SSL_get_verify_mode(socket->ssl))))
		return 0;
	SSL_DPRINT(stderr, "gtm_tls_do_post_hand_shake: implemented\n");
	/* Post Handshake Auth (PHA) requested for TLSv1.3 */
	if (1 != SSL_verify_client_post_handshake(socket->ssl))
	{
		SSL_DPRINT(stderr, "gtm_tls_do_post_hand_shake: return ssl_error\n");
		return ssl_error(socket, 0, 0);
	}
	SSL_DPRINT(stderr, "gtm_tls_do_post_hand_shake: return gtm_tls_repeat_hand_shake\n");
	return gtm_tls_repeat_hand_shake(socket);
#else
	SSL_DPRINT(stderr, "gtm_tls_do_post_hand_shake: not implemented\n");
	return 0;
#endif
}

/* gtm_tls_has_post_hand_shake returns TRUE if post handshake authentication is enabled */
int gtm_tls_has_post_hand_shake(gtm_tls_socket_t *socket)
{
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	return (PHA_MACROS_ENABLED == (PHA_MACROS_ENABLED & SSL_get_verify_mode(socket->ssl)));
#else
	return FALSE;
#endif
}

/* Perform a handshake on demand. This function was added to perform handshake(s) after the server side initiates PHA */
int gtm_tls_repeat_hand_shake(gtm_tls_socket_t *socket)
{
	int		rv;
	long		verify_result;
	X509		*cert;

	/* WARNING: one-shot attempt at handshake, caller needs to loop */
	SSL_DPRINT(stderr, "gtm_tls_repeat_hand_shake: enter\n");
	rv = SSL_do_handshake(socket->ssl);
	SSL_DPRINT(stderr, "gtm_tls_repeat_hand_shake: SSL_do_handshake: %d\n", rv);
	verify_result = SSL_get_verify_result(socket->ssl);
	SSL_DPRINT(stderr, "gtm_tls_repeat_hand_shake: verify_result: %d\n", verify_result);
	if ((0 < rv) && (X509_V_OK == verify_result))
		return 0;	/* Success */
	/* else error occurred */
	rv = ssl_error(socket, rv, verify_result);
	SSL_DPRINT(stderr, "gtm_tls_repeat_hand_shake: ssl_error: %d\n", rv);
	return rv;
}

int gtm_tls_does_renegotiate(gtm_tls_socket_t *socket)
{
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	return (TLS1_3_VERSION > SSL_version(socket->ssl));
#endif
	return 1;
}

int gtm_tls_renegotiate(gtm_tls_socket_t *socket)
{
	int		rv;
	long		verify_result;

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	if (TLS1_3_VERSION == SSL_version(socket->ssl))
	{
		if (0 >= (rv = SSL_key_update(socket->ssl, SSL_KEY_UPDATE_REQUESTED)))
			return ssl_error(socket, rv, SSL_get_verify_result(socket->ssl));
	} else /* TLS1_2_VERSION and before */
#endif
	{
		if (0 >= (rv = SSL_renegotiate(socket->ssl)))
			return ssl_error(socket, rv, SSL_get_verify_result(socket->ssl));
	}
	do
	{
		rv = SSL_do_handshake(socket->ssl);
		verify_result = SSL_get_verify_result(socket->ssl);
		if ((0 < rv) && (X509_V_OK == verify_result))
			return 0;
		/* On a blocking socket, SSL_do_handshake returns ONLY after successful completion. However, if the system call
		 * is interrupted (say, by a SIGALRM), it can return with a WANT_READ or WANT_WRITE. Handle it by retrying.
		 * Ideally, we should return back to the caller and let it handle WANT_READ/WANT_WRITE and call us again, but
		 * since renegotiation is done seldomly and returning the control back to the caller causes interface issues, we
		 * handle GTMTLS_WANT_READ or GTMTLS_WANT_WRITE by retrying.
		 */
		rv = ssl_error(socket, rv, verify_result);
	} while ((GTMTLS_WANT_READ == rv) || (GTMTLS_WANT_WRITE == rv));
	return rv;
}

#	if (((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 4)) || (LIBCONFIG_VER_MAJOR > 1))
#define VERIFY_DEPTH_TYPE	int
#	else
#define VERIFY_DEPTH_TYPE	long int
#endif

STATICFNDEF int gtm_tls_renegotiate_options_config(gtm_tls_socket_t *socket, char *idstr, int flags, config_t *cfg,
		VERIFY_DEPTH_TYPE *verify_depth, int *verify_depth_set, int *verify_mode, int *verify_mode_set, int *verify_level,
		int *verify_level_set, int *session_id_len, unsigned char *session_id_string, const char **CAfile);

STATICFNDEF int gtm_tls_renegotiate_options_config(gtm_tls_socket_t *socket, char *idstr, int flags, config_t *cfg,
		VERIFY_DEPTH_TYPE *verify_depth, int *verify_depth_set, int *verify_mode, int *verify_mode_set, int *verify_level,
		int *verify_level_set, int *session_id_len, unsigned char *session_id_string, const char **CAfile)
{
	int			rv, parse_len;
	config_setting_t	*tlsid, *tlssect, *cfg_setting;
	char			cfg_path[MAX_CONFIG_LOOKUP_PATHLEN];
	long			options_mask, options_current, options_clear, verify_long, level_long, level_clear;
	char			*optionendptr, *parse_ptr;
	const char		*verify_mode_string, *verify_level_string, *session_id_hex;
	STACK_OF(X509_NAME)	*CAcerts;

	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.verify-depth", idstr);
	if (CONFIG_TRUE == config_lookup_int(cfg, cfg_path, verify_depth))
		*verify_depth_set = TRUE;
	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.verify-mode", idstr);
	if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &verify_mode_string))
	{
		verify_long = 0;
		parse_ptr = parse_SSL_options(&gtm_ssl_verify_mode_list[0], SIZEOF(gtm_ssl_verify_mode_list),
				verify_mode_string, &verify_long, NULL);
		if (NULL != parse_ptr)
		{
			optionendptr = strstr((const char *)parse_ptr, OPTIONENDSTR);
			if (NULL == optionendptr)
				parse_len = strlen(parse_ptr);
			else
				parse_len = optionendptr - parse_ptr;
			gtm_tls_set_error(socket, "In TLSID: %s - unknown verify-mode option: %.*s",
				idstr, parse_len, parse_ptr);
			tls_errno = -1;
			return -1;
		}
		*verify_mode = (int)verify_long;
		*verify_mode_set = TRUE;
	}
	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.verify-level", idstr);
	if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &verify_level_string))
	{
		level_long = GTMTLS_OP_VERIFY_LEVEL_MASK & flags;
		level_clear = 0;
		parse_ptr = parse_SSL_options(&gtm_ssl_verify_level_list[0], SIZEOF(gtm_ssl_verify_level_list),
				verify_level_string, &level_long, &level_clear);
		if (NULL != parse_ptr)
		{
			optionendptr = strstr((const char *)parse_ptr, OPTIONENDSTR);
			if (NULL == optionendptr)
				parse_len = strlen(parse_ptr);
			else
				parse_len = optionendptr - parse_ptr;
			gtm_tls_set_error(socket, "In TLSID: %s - unknown verify-level option: %.*s",
				idstr, parse_len, parse_ptr);
			tls_errno = -1;
			return -1;
		}
		if (0 != level_clear)
			level_long &= ~level_clear;
		*verify_level = (int)level_long;
		*verify_level_set = TRUE;
	}
	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.CAfile", idstr);
	rv = config_lookup_string(cfg, cfg_path, CAfile);
	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.session-id-hex", idstr);
	if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &session_id_hex))
	{	/* convert hex to char and set len */
		*session_id_len = STRLEN(session_id_hex);
		if (MAX_SESSION_ID_LEN < *session_id_len)
			*session_id_len = MAX_SESSION_ID_LEN;	/* avoid overrun */
		GC_UNHEX(session_id_hex, session_id_string, *session_id_len);
		if (-1 == *session_id_len)
		{
			gtm_tls_set_error(socket, "In TLSID: %s - invalid session-id-hex value: %s",
				idstr, session_id_hex);
			tls_errno = -1;
			return -1;
		}
		*session_id_len = *session_id_len / 2;		/* bytes */
	}
	return 0;
}

int gtm_tls_renegotiate_options(gtm_tls_socket_t *socket, int msec_timeout, char *idstr, char *configstr, int tlsid_present)
{
	int			rv;
	config_t		*cfg, tmpcfg;
	config_setting_t	*tlsid, *tlssect, *cfg_setting;
	char			cfg_path[MAX_CONFIG_LOOKUP_PATHLEN];
	int			verify_mode, parse_len, verify_level;
	int			verify_mode_set, verify_level_set, verify_depth_set, flags;
	int			session_id_len;
	long			options_mask, options_current, options_clear, verify_long, level_long, level_clear;
	char			*optionendptr, *parse_ptr;
	const char		*verify_mode_string, *verify_level_string;
	const char		*CAfile = NULL, *session_id_hex;
	unsigned char		session_id_string[SSL_MAX_SSL_SESSION_ID_LENGTH];
#	if (((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 4)) || (LIBCONFIG_VER_MAJOR > 1))
	int			verify_depth;
#	else
	long int		verify_depth;
#	endif
	SSL			*ssl;
	STACK_OF(X509_NAME)	*CAcerts;

	ssl = socket->ssl;
	flags = socket->flags;
	gtm_tls_get_error(socket);
	verify_mode_set = verify_level_set = verify_depth_set = FALSE;
	if ('\0' != idstr[0])
	{	/* process options from config file and/or options */
		cfg = &gtm_tls_cfg;
		session_id_len = 0;
		if (tlsid_present)
		{	/* process config file first if real tlsid */
			SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s", idstr);
			cfg_setting = config_lookup(cfg, cfg_path);
			if (NULL == cfg_setting)
			{
				gtm_tls_set_error(socket, "TLSID %s not found in configuration file.", idstr);
				tls_errno = -1;
				return -1;
			}
			if (0 != gtm_tls_renegotiate_options_config(socket, idstr, flags, cfg, &verify_depth, &verify_depth_set,
					&verify_mode, &verify_mode_set, &verify_level, &verify_level_set, &session_id_len,
					session_id_string, &CAfile))
				return -1;
		}
		if (NULL != configstr)
		{	/* now process any options given on WRITE /TLS */
#			ifndef LIBCONFIG_VER_MAJOR
			gtm_tls_set_error(socket, "TLSID: %s: libconfig 1.4.x is needed to support providing options on WRITE /TLS",
				idstr);
			tls_errno = -1;
			return -1;
#			else
			config_init(&tmpcfg);
			if (CONFIG_FALSE == config_read_string(&tmpcfg, configstr))
			{
				gtm_tls_set_error(socket, "Failed to process options: %s in line %d:\n%s",
					config_error_text(&tmpcfg),
					config_error_line(&tmpcfg), configstr);
				tls_errno = -1;
				return -1;
			}
			if (0 != gtm_tls_renegotiate_options_config(socket, idstr, flags, &tmpcfg, &verify_depth, &verify_depth_set,
					&verify_mode, &verify_mode_set, &verify_level, &verify_level_set, &session_id_len,
					session_id_string, &CAfile))
				return -1;
#			endif
		}
		/* now really process verify-* and CAfile options */
		if (verify_depth_set)
			SSL_set_verify_depth(ssl, verify_depth);
		if (verify_mode_set)
			SSL_set_verify(ssl, verify_mode, NULL);
		if (verify_level_set)
			flags = (GTMTLS_OP_VERIFY_LEVEL_CMPLMNT & flags) | verify_level;
		if ((NULL == CAfile) && !(GTMTLS_OP_CLIENT_CA & flags))
		{	/* check for tlsid.CAfile or tls.CAfile */
			SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.CAfile", socket->tlsid);
			rv = config_lookup_string(cfg, cfg_path, &CAfile);
			if (CONFIG_FALSE == rv)
			{
				rv = config_lookup_string(cfg, "tls.CAfile", &CAfile);
			}
		}
		if (NULL != CAfile)
		{
			CAcerts = SSL_load_client_CA_file(CAfile);
			if (NULL == CAcerts)
			{
				SET_AND_APPEND_OPENSSL_ERROR("Failed to load client CA file %s", CAfile);
				tls_errno = -1;
				return -1;
			}
			SSL_set_client_CA_list(ssl, CAcerts);
			flags |= GTMTLS_OP_CLIENT_CA;
		}
		if ((0 < session_id_len)
			&& (0 >= SSL_set_session_id_context(ssl, (const unsigned char *)session_id_string,
						(unsigned int)session_id_len)))
		{
			SET_AND_APPEND_OPENSSL_ERROR("Failed to set Session-ID context to enable session resumption.");
			tls_errno = -1;
			return -1;
		}
	}
	socket->flags = flags;
	rv = gtm_tls_renegotiate(socket);
	return rv;
}

int gtm_tls_get_conn_info(gtm_tls_socket_t *socket, gtm_tls_conn_info *conn_info)
{
	long			verify_result, timeout, creation_time;
	int			session_id_length;
	unsigned int		ssl_version;
	const SSL_CIPHER	*cipher;
	const COMP_METHOD	*compression_method;
	char			*ssl_version_ptr, *session_id_ptr;
	gtm_tls_ctx_t		*tls_ctx;
	X509			*peer;
	SSL			*ssl;
	EVP_PKEY		*pubkey;
	SSL_SESSION		*session;

	ssl = socket->ssl;
	tls_ctx = socket->gtm_ctx;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	peer = SSL_get0_peer_certificate(ssl);
#else
	peer = SSL_get_peer_certificate(ssl);
#endif
	if ((NULL != peer) || (GTMTLS_OP_SOCKET_DEV & socket->flags))
	{	/* if socket device and no certificate from peer still provide info */
		verify_result = SSL_get_verify_result(ssl);
		if ((X509_V_OK == verify_result) || (GTMTLS_OP_ABSENT_CONFIG & tls_ctx->flags))
		{	/* return information for Socket clients even without a config file */
			/* SSL-Session Protocol */
			switch (ssl_version = SSL_version(ssl))
			{
				case SSL2_VERSION:
					ssl_version_ptr = "SSLv2";
					break;

				case SSL3_VERSION:
					ssl_version_ptr = "SSLv3";
					break;

				case TLS1_VERSION:
					ssl_version_ptr = "TLSv1";
					break;
				case TLS1_1_VERSION:
					ssl_version_ptr = "TLSv1.1";
					break;
				case TLS1_2_VERSION:
					ssl_version_ptr = "TLSv1.2";
					break;
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
				case TLS1_3_VERSION:
					ssl_version_ptr = "TLSv1.3";
					break;
#endif
				default:
					ssl_version_ptr = NULL;
					SNPRINTF(conn_info->protocol, SIZEOF(conn_info->protocol), "unknown protocol 0x%X",
							ssl_version);
					break;
			}
			if (NULL != ssl_version_ptr)
				strncpy(conn_info->protocol, ssl_version_ptr, MAX_ALGORITHM_LEN);
			/* SSL-Session Cipher Algorithm */
			cipher = SSL_get_current_cipher(ssl);
			SNPRINTF(conn_info->session_algo, SIZEOF(conn_info->session_algo), "%s", SSL_CIPHER_get_name(cipher));
			/* Remote Certificate Asymmetric Algorithm */
			if (NULL != peer)
			{
				pubkey = X509_get_pubkey(peer);
#				if OPENSSL_VERSION_NUMBER >= 0x10000001L
				SNPRINTF(conn_info->cert_algo, SIZEOF(conn_info->cert_algo), "%s",
						OBJ_nid2ln(EVP_PKEY_base_id(pubkey)));
#				else
				SNPRINTF(conn_info->cert_algo, SIZEOF(conn_info->cert_algo), "%s",
						OBJ_nid2ln(pubkey->type));
#				endif
			} else
				conn_info->cert_algo[0] = '\0';
			/* Is Secure Renegotiation Supported? */
			/* SSL_get_secure_renegotiation_support function was introduced in OpenSSL version >= "0.9.8m". */
			conn_info->secure_renegotiation = SSL_get_secure_renegotiation_support(ssl);
			/* Is the session reused? */
			conn_info->reused = SSL_session_reused(ssl);
			/* Negotiated Session-ID. */
			if (NULL == (session = SSL_get1_session(ssl)))	/* `get1' version is used to increment reference count. */
			{
				gtm_tls_set_error(socket, "Failed to obtain the handle to negotiated SSL/TLS session");
				return -1;
			}
			session_id_ptr = (char *)SSL_SESSION_get_id(session, (unsigned int *)&session_id_length);
			assert(session_id_length <= (MAX_SESSION_ID_LEN / 2));
			assert(MAX_SESSION_ID_LEN >= (SSL_MAX_SSL_SESSION_ID_LENGTH * 2));
			if ((MAX_SESSION_ID_LEN / 2) < session_id_length)
				session_id_length = MAX_SESSION_ID_LEN / 2;	/* avoid overrun */
			GC_HEX(session_id_ptr, conn_info->session_id, session_id_length * 2);
			conn_info->session_id[session_id_length * 2] = '\0';
			/* Session expiry timeout. */
			if (0 >= (timeout = SSL_SESSION_get_timeout(session)))
				conn_info->session_expiry_timeout = -1;
			else
			{
				creation_time = SSL_SESSION_get_time(session);
				if (0 == creation_time)
					conn_info->session_expiry_timeout = -1;
				else
					conn_info->session_expiry_timeout = creation_time + timeout;
			}
			SSL_SESSION_free(session);
			/* Is compression supported? */
#			ifndef OPENSSL_NO_COMP
			compression_method = SSL_get_current_compression(ssl);
			conn_info->compression = compression_method ? (char *)SSL_COMP_get_name(compression_method) : "NONE";
#			else
			conn_info->compression = "NONE";
#			endif
			if (NULL != peer)
			{
				/* Remote Certificate Asymmetric Algorithm Strength */
				conn_info->cert_nbits = EVP_PKEY_bits(pubkey);
				/* Remote Certificate Subject */
				X509_NAME_oneline(X509_get_subject_name(peer), conn_info->subject, MAX_X509_LEN);
				/* Remote Certificate Issuer */
				X509_NAME_oneline(X509_get_issuer_name(peer), conn_info->issuer, MAX_X509_LEN);
				/* Certificate Expiry */
				if (-1 == format_ASN1_TIME(X509_get_notBefore(peer), conn_info->not_before, MAX_TIME_STRLEN))
					SNPRINTF(conn_info->not_before, MAX_TIME_STRLEN, "Bad certificate date");
				if (-1 == format_ASN1_TIME(X509_get_notAfter(peer), conn_info->not_after, MAX_TIME_STRLEN))
					SNPRINTF(conn_info->not_after, MAX_TIME_STRLEN, "Bad certificate date");
#if OPENSSL_VERSION_NUMBER < 0x30000000L
				X509_free(peer);
#endif
			} else
			{
				conn_info->cert_nbits = 0;
				conn_info->subject[0] = conn_info->issuer[0] = '\0';
				conn_info->not_before[0] = conn_info->not_after[0] = '\0';
			}
			if (GTM_TLS_API_VERSION_SOCK <= socket->gtm_ctx->version)
				conn_info->options = SSL_get_options(ssl);
			if (GTM_TLS_API_VERSION_RENEGOPT <= socket->gtm_ctx->version)
			{
				conn_info->total_renegotiations = SSL_total_renegotiations(ssl);
				conn_info->verify_mode = SSL_get_verify_mode(ssl);
			}
			return 0;
		} else
		{
			gtm_tls_set_error(socket, "Peer certificate invalid: %s",
					X509_verify_cert_error_string(verify_result));
#if OPENSSL_VERSION_NUMBER < 0x30000000L
			X509_free(peer);
#endif
			return -1;
		}
	} else
		gtm_tls_set_error(socket, "No certificate sent from the remote side");
	return -1;
}

int gtm_tls_send(gtm_tls_socket_t *socket, char *buf, int send_len)
{
	int		rv;
	long		verify_result;

	if (0 < (rv = SSL_write(socket->ssl, buf, send_len)))
	{
		assert(SSL_ERROR_NONE == SSL_get_error(socket->ssl, rv));
		verify_result = SSL_get_verify_result(socket->ssl);
		if (X509_V_OK == verify_result)
			return rv;
	} else
		verify_result = SSL_get_verify_result(socket->ssl);
	return ssl_error(socket, rv, verify_result);
}

int gtm_tls_recv(gtm_tls_socket_t * socket, char *buf, int recv_len)
{
	int		rv;
	long		verify_result;

	rv = SSL_read(socket->ssl, buf, recv_len);
#ifdef DEBUG
	/* Emulate an error condition in case of WBTEST_REPL_TLS_RECONN white box*/
	if ((NULL != (wbox_enable = getenv("gtm_white_box_test_case_enable"))) &&
		(NULL != (wbox_tls_check = getenv("gtm_white_box_test_case_number"))) &&
		(NULL != (wbox_count = getenv("wbox_count"))) &&
		(NULL != (wbox_test_count= getenv("gtm_white_box_test_case_count"))) &&
		(1 == (wbox_enable_val = atoi(wbox_enable))) && (WBTEST_REPL_TLS_RECONN ==
			(wbox_tls_check_val = atoi(wbox_tls_check))) && (wbox_test_count_val = atoi(wbox_test_count))
			&& (wbox_count_val = atoi(wbox_count)) && (wbox_test_count_val == wbox_count_val))
			rv = -1;
#endif
	if (0 < rv)
	{
		assert(SSL_ERROR_NONE == SSL_get_error(socket->ssl, rv));
		verify_result = SSL_get_verify_result(socket->ssl);
		if (X509_V_OK == verify_result)
			return rv;
	} else
		verify_result = SSL_get_verify_result(socket->ssl);
	return ssl_error(socket, rv, verify_result);
}

int gtm_tls_cachedbytes(gtm_tls_socket_t *socket)
{
	return SSL_pending(socket->ssl);
}

void gtm_tls_socket_close(gtm_tls_socket_t *socket)
{
	if ((NULL == socket) || (NULL == socket->ssl))
	{
		tls_errno = 0;
		return;
	}
	/* Invoke SSL_shutdown to close the SSL/TLS connection. Although the protocol (and the OpenSSL library) supports
	 * bidirectional shutdown (which waits for the peer's "close notify" alert as well), we intend to only send the
	 * "close notify" alert and be done with it. This is because the process is done with the connection when it calls
	 * this function and we don't want to consume additional time waiting for a "close notify" acknowledge signal from the
	 * other side.
	 */
	if (!(GTMTLS_OP_NOSHUTDOWN & socket->flags))
	{
		tls_errno = 0;
		SSL_shutdown(socket->ssl);
	}
	tls_errno = 0;
	SSL_free(socket->ssl);
	socket->ssl = NULL;
}

void gtm_tls_session_close(gtm_tls_socket_t **socket)
{
	SSL_SESSION		*session;
	gtm_tls_socket_t	*sock;

	sock = *socket;
	if (NULL == sock)
		return;
	if (NULL != sock->ssl)
		gtm_tls_socket_close(sock);
	if (NULL != (session = sock->session))
	{
		SSL_DPRINT(stderr, "gtm_tls_session_close: references=%d\n", session->references);
		SSL_SESSION_free(session);
	}
	sock->session = NULL;
	FREE(*socket);
	*socket = NULL;
}

void gtm_tls_fini(gtm_tls_ctx_t **tls_ctx)
{
	gtmtls_passwd_list_t	*node, *prev_node;

	/* Free up the SSL/TLS context */
	assert(*tls_ctx);
	SSL_CTX_free((*tls_ctx)->ctx);
	FREE(*tls_ctx);
	*tls_ctx = NULL;
	/* Free up the libconfig context */
	config_destroy(&gtm_tls_cfg);	/* Relinquish all the memory allocated by libconfig */
	/* Free up the gtmtls_passwd_list_t linked list */
	node = gtmtls_passwd_listhead;
	while (NULL != node)
	{
		gc_freeup_pwent(node->pwent);
		prev_node = node;
		node = node->next;
		FREE(prev_node);
	}
}

/***** EXTERNAL CALLs Section *****/
#define FREE_TLS_SOCKET_OBJECT(LCL_GTM_TLS_SOCK)		\
{								\
	SSL_free(LCL_GTM_TLS_SOCK->ssl);			\
	gtm_free(LCL_GTM_TLS_SOCK);				\
}

/* Report the version of the gtmtls plugin library. The TLS plugin is not portable across GT.M versions. GT.M uses this
 * version number to determine if the TLS version supports functionality that GT.M expects.
 *
 * Input:
 * 	count	- number of arguments in the external call
 *
 * Output:
 *	version	- 2048 byte external call interface allocated buffer containing the gtmtls plugin library version with
 *		  relevant details
 */
long gtm_tls_get_version(int count, char *version)
{
	(void)snprintf(version, ERR_STR_SIZE, "libgtmtls.so(%s) %x %s",
			PLUGIN_LIBRARY_OSNAME, GTM_TLS_API_VERSION, PLUGIN_LIBRARY_RELEASE_TYPE);
	return GTM_TLS_API_VERSION;
}

/* Report the run-time time version of the 3rd party TLS library. OpenSSL functionality is often version dependent.
 * Use this to determine what is available. For instance, one needs at least OpenSSL 1.1.0 for TLSv1.3 support.
 *
 * Input:
 * 	count	 - number of arguments in the external call
 *	libmode  - string of "run-time" for the run-time library version or "compile-time" for the library
 *		   version at compile time
 *
 * Output:
 *	version	- 2048 byte external call interface allocated buffer containing the 3rd party TLS library version
 *	errstr	- 2048 byte external call allocated buffer containing an error message
 *
 * Return:
 * 	Integer representation of the 3rd party TLS version (use hexadecimal printing to compare with source code),
 * 	-1 on errror
 */
long gtm_tls_get_TLS_lib_version(int count, char *version, char *libmode, char *errstr)
{
	/* Load the TLS context if needed */
	if ((NULL == gtm_tls_ctx) && (NULL == gtm_tls_init(GTM_TLS_API_VERSION, GTMTLS_OP_INTERACTIVE_MODE)))
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "%s", gtm_tls_get_error(NULL));
		return -1;
	}
	if ((NULL == libmode)
			|| (strncasecmp(libmode, LIT_AND_LEN(RUN_TIME_STR))
				&& strncasecmp(libmode, LIT_AND_LEN(COMPILE_TIME_STR))))
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "'%s' is not a valid. Please use either 'run-time' or 'compile-time'",
				(NULL == libmode)? "": libmode);
		return -1;
	}
	if (0 == strncasecmp(libmode, LIT_AND_LEN(COMPILE_TIME_STR)))
	{
		(void)snprintf(version, ERR_STR_SIZE, "%s", OPENSSL_VERSION_TEXT);
		return OPENSSL_VERSION_NUMBER;
	} /* else RUN_TIME */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	(void)snprintf(version, ERR_STR_SIZE, "%s", OpenSSL_version(OPENSSL_VERSION));
	return OpenSSL_version_num();
#else
	(void)snprintf(version, ERR_STR_SIZE, "%s", SSLeay_version(SSLEAY_VERSION));
	return SSLeay();
#endif
}

/* Report the 3rd party TLS library's default set of ciphers
 *
 * Input:
 * 	count	- number of arguments in the external call
 *	tlsver	- string of either "tls1_3" or "tls1_2"
 *
 * Output:
 *	tlscipher	- 4096 byte external call allocated buffer containing the colon delimited list of default set of
 *			  pre-TLSv1.3 ciphers from 3rd party TLS library
 *	errstr		- 2048 byte external call allocated buffer containing an error message
 *
 * Return:
 * 	Positive integer (Success) for the number of pieces in the string, -1 (Error) check error message
 *
 */
long gtm_tls_get_defaultciphers(int count, char *tlsciphers, char *tlsver, char *errstr)
{
	char	*cptr;
	int	i;

	/* Load the TLS context if needed */
	if ((NULL == gtm_tls_ctx) && (NULL == gtm_tls_init(GTM_TLS_API_VERSION, GTMTLS_OP_INTERACTIVE_MODE)))
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "%s", gtm_tls_get_error(NULL));
		return -1;
	}
	if ((NULL == tlsver) || (strncasecmp(tlsver, LIT_AND_LEN(TLS1_2_STR)) && strncasecmp(tlsver, LIT_AND_LEN(TLS1_3_STR))))
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "'%s' is not a valid TLS version. Please use either 'tls1_2' or 'tls1_3'",
				(NULL == tlsver)? "": tlsver);
		return -1;
	}
	if (0 == strncasecmp(tlsver, LIT_AND_LEN(TLS1_3_STR)))
	{
#if OPENSSL_VERSION_NUMBER > 0x30000000L
		snprintf(tlsciphers, CIPHER_LIST_SIZE, "%s", OSSL_default_ciphersuites());
#elif OPENSSL_VERSION_NUMBER >= 0x10100000L
		snprintf(tlsciphers, CIPHER_LIST_SIZE, "%s", TLS_DEFAULT_CIPHERSUITES);
#else
		(void)snprintf(errstr, ERR_STR_SIZE, "tls1_3 ciphers are no supported with %s", SSLeay_version(SSLEAY_VERSION));
		tlsciphers[0] = '\0';
		return -1;
#endif
	} else
	{	/* Defaults to TLSv1.2 */
#if OPENSSL_VERSION_NUMBER > 0x30000000L
		snprintf(tlsciphers, CIPHER_LIST_SIZE, "%s", OSSL_default_cipher_list());
#else
		snprintf(tlsciphers, CIPHER_LIST_SIZE, "%s", SSL_DEFAULT_CIPHER_LIST);
#endif
	}

	/* Count the number of pieces */
	for (i = 1, cptr = strchr(tlsciphers, ':'); NULL != cptr; cptr = strchr(cptr, ':'))
		i++, cptr++;
	return i;
}

/* Given a list of ciphers, return the resulting string of supported ciphers
 *
 * Input:
 * 	count	- number of arguments in the external call
 *	tlsver	- string which if not "tls1_3" or null is treated as tls1_2
 *	mode	- string which if not REPLICATION or null is treated as SOCKETDEVICE
 *	tlsid	- string TLS configuration identifier to use; If null/not present, the $gtmcrypt_config is NOT used
 *
 * Output:
 * 	Available set of ciphers
 *	tlscipher	- 4096 byte external call allocated buffer containing the colon delimited list of supported ciphersuites
 *	errstr		- 2048 byte external call allocated buffer containing an error message
 *
 * Return:
 * 	Positive integer (Success) for the number of pieces in the string, -1 (Error) check error message
 */
long gtm_tls_get_ciphers(int count, char *tlsciphers, char *tlsver, char *mode, char *tlsid, char *ciphersuites, char *errstr)
{
	const char		*ciphername;
	char			*cptr, *endptr;
	char			idstr[MAX_TLSID_LEN];
	gtm_tls_ctx_t		*lcl_tls_ctx = NULL;
	gtm_tls_socket_t	*lcl_gtm_tls_sock;
	int			i, flags = GTMTLS_OP_SOCKET_DEV		/* Treat like a SOCKET device and not replication */
					| GTMTLS_OP_INTERACTIVE_MODE;	/* Allows for no config file but will prompt for password */
	long			status;
	const SSL_CIPHER	*c;
	STACK_OF(SSL_CIPHER)	*sk = NULL;

	/* Load the TLS context if needed */
	if ((NULL == gtm_tls_ctx) && (NULL == (lcl_tls_ctx = (gtm_tls_init(GTM_TLS_API_VERSION, GTMTLS_OP_INTERACTIVE_MODE)))))
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "Error initializing OpenSSL context: %s", gtm_tls_get_error(NULL));
		return -1;
	}
	lcl_tls_ctx = gtm_tls_ctx;

	/* Use supplied TLSID if requested. Not asking for one makes checking $gtmcrypt_config optional and implies client mode */
	if ((NULL != tlsid) && (0 < strlen(tlsid)))
		(void)snprintf(idstr, MAX_TLSID_LEN, "%s", tlsid);
	else
	{
		idstr[0] = '\0';
		flags |= GTMTLS_OP_CLIENT_MODE;
	}
	/* TLS version */
	if ((NULL == tlsver) || (strncasecmp(tlsver, LIT_AND_LEN(TLS1_2_STR)) && strncasecmp(tlsver, LIT_AND_LEN(TLS1_3_STR))))
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "'%s' is not a valid TLS version. Please use either 'tls1_2' or 'tls1_3'",
				(NULL == tlsver)? "": tlsver);
		return -1;
	}

	/* Use SOCKET device or Replication server defaults? */
	if ((NULL == mode) || (strncasecmp(mode, LIT_AND_LEN(REPLICATION_STR)) && strncasecmp(mode, LIT_AND_LEN(SOCKET_STR))))
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "'%s' is not valid. Please use either 'SOCKET' for SOCKET devices or "
				"'REPLICATION' for replication servers", (NULL == mode)? "": mode);
		return -1;
	}

	if (0 == strncasecmp(mode, LIT_AND_LEN(REPLICATION_STR)))
		flags ^= GTMTLS_OP_INTERACTIVE_MODE;	/* Replication uses a different default ciphersuite */

	/* Create the SSL object */
	lcl_gtm_tls_sock = gtm_tls_socket(lcl_tls_ctx, NULL, 0, idstr, flags);
	if (NULL == lcl_gtm_tls_sock)
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "Error establishing SSL/TLS context: %s", gtm_tls_get_error(NULL));
		return -1;
	}

	/* Apply the protocol restrictions to SSL object. Note: gtm_tls_init() limits the lowest TLS version to TLSv1.2 */
	if (0 == strncasecmp(tlsver, LIT_AND_LEN(TLS1_3_STR)))
	{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
		SSL_set_min_proto_version(lcl_gtm_tls_sock->ssl, TLS1_3_VERSION);
		SSL_set_max_proto_version(lcl_gtm_tls_sock->ssl, TLS1_3_VERSION);
	} else
	{
		SSL_set_min_proto_version(lcl_gtm_tls_sock->ssl, TLS1_2_VERSION);
		SSL_set_max_proto_version(lcl_gtm_tls_sock->ssl, TLS1_2_VERSION);
#else
		(void)snprintf(errstr, ERR_STR_SIZE, "tls1_3 ciphers are not supported with %s", SSLeay_version(SSLEAY_VERSION));
		tlsciphers[0] = '\0';
		return -1;
#endif
	}

	/* Update cipher suites using the SAME string for TLSv1.3 and pre-TLSv1.3 */
	if ((NULL != ciphersuites) && (0 < strlen(ciphersuites))
			&& (0 >= SSL_set_cipher_list(lcl_gtm_tls_sock->ssl, ciphersuites))	/* TLSv1.2 and before */
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
			&& (0 >= SSL_set_ciphersuites(lcl_gtm_tls_sock->ssl, ciphersuites))	/* TLSv1.3 since OpenSSL 1.1.0 */
#endif
	   )
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "Cipher Suite error: %s", gtm_tls_get_error(NULL));
		FREE_TLS_SOCKET_OBJECT(lcl_gtm_tls_sock);
		return -1;
	}

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	sk = SSL_get1_supported_ciphers((SSL *)lcl_gtm_tls_sock->ssl);
#else
	sk = SSL_get_ciphers((SSL *)lcl_gtm_tls_sock->ssl);
#endif
	cptr = tlsciphers;
	endptr = cptr + CIPHER_LIST_SIZE;
	for (i = 0; (i < sk_SSL_CIPHER_num(sk)) && (cptr < endptr); i++)
	{
		c = sk_SSL_CIPHER_value(sk, i);
		ciphername = SSL_CIPHER_get_name(c);
		if (ciphername == NULL)
			break;
		cptr += snprintf(cptr, CIPHER_LIST_SIZE - (cptr - tlsciphers), "%s%s", (i)?":":"", ciphername);
	}
	if (cptr > endptr)
	{
		(void)snprintf(errstr, ERR_STR_SIZE, "truncated cipher list, %d of %d", i, sk_SSL_CIPHER_num(sk));
		status = 0;
	} else
		status = sk_SSL_CIPHER_num(sk);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	sk_SSL_CIPHER_free(sk);
#endif
	FREE_TLS_SOCKET_OBJECT(lcl_gtm_tls_sock);

	return status;
}
