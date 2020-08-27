/****************************************************************
 *								*
 * Copyright (c) 2013-2019 Fidelity National Information	*
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

#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
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

#include "ydb_tls_interface.h"
#include "gtm_tls_impl.h"
#include "ydb_getenv.h"

#ifdef DEBUG
#include <assert.h>	/* Use "assert" macro to check assertions in DEBUG builds */
#endif

GBLDEF	int			tls_errno;
GBLDEF	gtmtls_passwd_list_t	*gtmtls_passwd_listhead;

STATICDEF	config_t	gtm_tls_cfg;
STATICDEF DH			*dh512, *dh1024;	/* Diffie-Hellman structures for Ephemeral Diffie-Hellman key exchange. */

#define MAX_CONFIG_LOOKUP_PATHLEN	64

/* Older, but still commonly used, OpenSSL versions don't have macros for TLSv1.1, TLSv1.2 and TLSv1.3 versions.
 * They are hard coded. So, define them here for use in gtm_tls_get_conn_info.
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

/* Below template translates to: Arrange ciphers in increasing order of strength after excluding the following:
 * ADH: Anonymous Diffie-Hellman Key Exchange (Since we want both encryption and authentication and ADH provides only former).
 * LOW: Low strength ciphers.
 * EXP: Export Ciphers.
 * MD5 : MD5 message digest.
 */
#define REPL_CIPHER_LIST			"ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"
#define GTM_DEFAULT_CIPHER_LIST			"DEFAULT:!SRP"

#define OPTIONEND ':'
#define OPTIONENDSTR ":"
#define OPTIONNOT '!'
#define DEFINE_SSL_OP(OP_DEF)   { #OP_DEF , OP_DEF }
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
	DEFINE_SSL_OP(SSL_VERIFY_PEER),
	DEFINE_SSL_OP(SSL_VERIFY_NONE),
	DEFINE_SSL_OP(SSL_VERIFY_FAIL_IF_NO_PEER_CERT),
	DEFINE_SSL_OP(SSL_VERIFY_CLIENT_ONCE),
	{NULL, 0}
};
STATICDEF struct gtm_ssl_options gtm_ssl_options_list[] =
{
#ifdef	SSL_OP_MICROSOFT_SESS_ID_BUG
	DEFINE_SSL_OP(SSL_OP_MICROSOFT_SESS_ID_BUG),
#endif
#ifdef	SSL_OP_NETSCAPE_CHALLENGE_BUG
	DEFINE_SSL_OP(SSL_OP_NETSCAPE_CHALLENGE_BUG),
#endif
#ifdef	SSL_OP_LEGACY_SERVER_CONNECT
	DEFINE_SSL_OP(SSL_OP_LEGACY_SERVER_CONNECT),
#endif
#ifdef	SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG
	DEFINE_SSL_OP(SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG),
#endif
#ifdef	SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG
	DEFINE_SSL_OP(SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG),
#endif
#ifdef	SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER
	DEFINE_SSL_OP(SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER),
#endif
#ifdef	SSL_OP_SAFARI_ECDHE_ECDSA_BUG
	DEFINE_SSL_OP(SSL_OP_SAFARI_ECDHE_ECDSA_BUG),
#endif
#ifdef	SSL_OP_SSLEAY_080_CLIENT_DH_BUG
	DEFINE_SSL_OP(SSL_OP_SSLEAY_080_CLIENT_DH_BUG),
#endif
#ifdef	SSL_OP_TLS_D5_BUG
	DEFINE_SSL_OP(SSL_OP_TLS_D5_BUG),
#endif
#ifdef	SSL_OP_TLS_BLOCK_PADDING_BUG
	DEFINE_SSL_OP(SSL_OP_TLS_BLOCK_PADDING_BUG),
#endif
#ifdef	SSL_OP_MSIE_SSLV2_RSA_PADDING
	DEFINE_SSL_OP(SSL_OP_MSIE_SSLV2_RSA_PADDING),
#endif
#ifdef	SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
	DEFINE_SSL_OP(SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS),
#endif
#ifdef	SSL_OP_ALL
	DEFINE_SSL_OP(SSL_OP_ALL),
#endif
#ifdef	SSL_OP_NO_QUERY_MTU
	DEFINE_SSL_OP(SSL_OP_NO_QUERY_MTU),
#endif
#ifdef	SSL_OP_COOKIE_EXCHANGE
	DEFINE_SSL_OP(SSL_OP_COOKIE_EXCHANGE),
#endif
#ifdef	SSL_OP_NO_TICKET
	DEFINE_SSL_OP(SSL_OP_NO_TICKET),
#endif
#ifdef	SSL_OP_CISCO_ANYCONNECT
	DEFINE_SSL_OP(SSL_OP_CISCO_ANYCONNECT),
#endif
#ifdef	SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
	DEFINE_SSL_OP(SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION),
#endif
#ifdef	SSL_OP_NO_COMPRESSION
	DEFINE_SSL_OP(SSL_OP_NO_COMPRESSION),
#endif
#ifdef	SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
	DEFINE_SSL_OP(SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION),
#endif
#ifdef	SSL_OP_SINGLE_ECDH_USE
	DEFINE_SSL_OP(SSL_OP_SINGLE_ECDH_USE),
#endif
#ifdef	SSL_OP_SINGLE_DH_USE
	DEFINE_SSL_OP(SSL_OP_SINGLE_DH_USE),
#endif
#ifdef	SSL_OP_EPHEMERAL_RSA
	DEFINE_SSL_OP(SSL_OP_EPHEMERAL_RSA),
#endif
#ifdef	SSL_OP_CIPHER_SERVER_PREFERENCE
	DEFINE_SSL_OP(SSL_OP_CIPHER_SERVER_PREFERENCE),
#endif
#ifdef	SSL_OP_TLS_ROLLBACK_BUG
	DEFINE_SSL_OP(SSL_OP_TLS_ROLLBACK_BUG),
#endif
#ifdef	SSL_OP_NO_SSLv2
	DEFINE_SSL_OP(SSL_OP_NO_SSLv2),
#endif
#ifdef	SSL_OP_NO_SSLv3
	DEFINE_SSL_OP(SSL_OP_NO_SSLv3),
#endif
#ifdef	SSL_OP_NO_TLSv1
	DEFINE_SSL_OP(SSL_OP_NO_TLSv1),
#endif
#ifdef	SSL_OP_NO_TLSv1_2
	DEFINE_SSL_OP(SSL_OP_NO_TLSv1_2),
#endif
/*	Special case for NO_TLSv1_3 so defined for pre OpenSSL 1.1.1 */
#ifndef	SSL_OP_NO_TLSv1_3
#	define SSL_OP_NO_TLSv1_3	0x0
#endif
	DEFINE_SSL_OP(SSL_OP_NO_TLSv1_3),
#ifdef	SSL_OP_NO_TLSv1_1
	DEFINE_SSL_OP(SSL_OP_NO_TLSv1_1),
#endif
#ifdef	SSL_OP_PKCS1_CHECK_1
	DEFINE_SSL_OP(SSL_OP_PKCS1_CHECK_1),
#endif
#ifdef	SSL_OP_PKCS1_CHECK_2
	DEFINE_SSL_OP(SSL_OP_PKCS1_CHECK_2),
#endif
#ifdef	SSL_OP_NETSCAPE_CA_DN_BUG
	DEFINE_SSL_OP(SSL_OP_NETSCAPE_CA_DN_BUG),
#endif
#ifdef	SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG
	DEFINE_SSL_OP(SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG),
#endif
#ifdef	SSL_OP_CRYPTOPRO_TLSEXT_BUG
	DEFINE_SSL_OP(SSL_OP_CRYPTOPRO_TLSEXT_BUG),
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

STATICFNDEF char *parse_SSL_options(struct gtm_ssl_options *opt_table, size_t opt_table_size, const char *options, long *current,
					long *clear);
STATICFNDEF char *parse_SSL_options(struct gtm_ssl_options *opt_table, size_t opt_table_size, const char *options, long *current,
					long *clear)
{
	size_t		num_options, index, optionlen;
	long		bitmask;
	const char	*charptr, *optionend;

	if (NULL == options)
		return 0;
	bitmask = *current;
	num_options = opt_table_size/SIZEOF(struct gtm_ssl_options);
	for (charptr = options; *charptr; charptr = optionend)
	{
		int negate;
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

/* In PRO builds (where DEBUG macro is not defined), define assert(x) to be no-op.
 * In DEBUG builds, use system assert defined by <assert.h>.
 */
#ifndef DEBUG
#define assert(X)
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
	int		error_code;
	char		*errptr, *end;
	SSL		*ssl;

	ssl = tls_sock->ssl;
	error_code = SSL_get_error(ssl, err); /* generic error code */
	switch (error_code)
	{
		case SSL_ERROR_ZERO_RETURN:
			/* SSL/TLS connection has been closed gracefully. The underlying TCP/IP connection is not yet closed. The
			 * caller should take necessary action to close the underlying transport
			 */
			tls_errno = ECONNRESET;
			break;

		case SSL_ERROR_SYSCALL:
			tls_errno = errno;
			if (0 == tls_errno)
				tls_errno = ECONNRESET;
			break;

		case SSL_ERROR_WANT_WRITE:
			return GTMTLS_WANT_WRITE;

		case SSL_ERROR_WANT_READ:
			return GTMTLS_WANT_READ;

		case SSL_ERROR_SSL:
		case SSL_ERROR_NONE:
			errptr = gtmcrypt_err_string;
			end = errptr + MAX_GTMCRYPT_ERR_STRLEN;
			tls_errno = -1;
			if ((GTMTLS_OP_VERIFY_LEVEL_CHECK & tls_sock->flags) && (X509_V_OK != verify_result))
			{
				UPDATE_ERROR_STRING("certificate verification error: %s",
					X509_verify_cert_error_string(verify_result));
				return -1;
			} else if (SSL_ERROR_NONE == error_code)
			{	/* we are ignoring verify result and no other error */
				tls_errno = 0;
				return 0;
			}
			do
			{
				error_code = ERR_get_error();
				if (0 == error_code)
				{
					if (errptr == gtmcrypt_err_string)
					{
						/* Very first call to ERR_get_error returned 0. This is very unlikely. Nevertheless
						 * handle this by updating the error string with a generic error.
						 */
						UPDATE_ERROR_STRING("Unknown SSL/TLS protocol error.");
						return -1;
					}
					break;
				} else if ((errptr < end) && (errptr != gtmcrypt_err_string))
					*errptr++ = ';';
				if (errptr >= end)
					continue;	/* We could break here, but we want to clear the OpenSSL error stack. */
				ERR_error_string_n(error_code, errptr, end - errptr);
				errptr += STRLEN(errptr);
			} while (TRUE);
			break;

		case SSL_ERROR_WANT_X509_LOOKUP:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_WANT_CONNECT:
		default:
			tls_errno = -1;
			UPDATE_ERROR_STRING("Unknown error: %d returned by `SSL_get_error'", error_code);
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
	SSL_DPRINT(stdout, "new_session_callback: references=%d\n", session->references);
	if (socket->session)
		SSL_SESSION_free(socket->session);
	/* Add the new session to the `socket' structure. */
	socket->session = session;
	return 1;
}

STATICFNDEF DH *read_dhparams(const char *dh_fn)
{
	BIO		*bio;
	DH		*dh;

	if (NULL == (bio = BIO_new_file(dh_fn, "r")))
	{
		GC_APPEND_OPENSSL_ERROR("Unable to load Diffie-Hellman parameter file: %s.", dh_fn);
		return NULL;
	}
	if (NULL == (dh = (PEM_read_bio_DHparams(bio, NULL, NULL, NULL))))
	{
		GC_APPEND_OPENSSL_ERROR("Unable to load Diffie-Hellman parameter file: %s.", dh_fn);
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
		UPDATE_ERROR_STRING("Configuration parameter `tls.dh512' not specified.");
		return -1;
	}
	if (!rv2)
	{
		UPDATE_ERROR_STRING("Configuration parameter `tls.dh1024' not specified.");
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

int gtm_tls_errno(void)
{
	return tls_errno;
}

const char *gtm_tls_get_error(void)
{
	return gtmcrypt_err_string;
}

gtm_tls_ctx_t *gtm_tls_init(int version, int flags)
{
	const char		*CAfile = NULL, *CApath = NULL, *crl, *CAptr, *cipher_list, *options_string, *verify_mode_string;
	const char		*verify_level_string;
	char			*config_env, *parse_ptr, *optionendptr;
	int			rv, rv1, rv2, fips_requested, fips_enabled, verify_mode, parse_len, verify_level;
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
#	if OPENSSL_VERSION_NUMBER < 0x009080dfL
	unsigned int            cfg_line;
	const char              *cfg_file;
	config_setting_t        *cfg_setting;
#	endif
	gtm_tls_ctx_t		*gtm_tls_ctx;

	assert(GTM_TLS_API_VERSION >= version); /* Make sure the caller is using the right API version */
	/* Initialize the SSL/TLS library, the algorithms/cipher suite and error strings. */
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	/* Turn on FIPS mode if requested. */
	fips_enabled = FALSE;	/* most common case. */
	IS_FIPS_MODE_REQUESTED(fips_requested);
	if (fips_requested)
	{
		ENABLE_FIPS_MODE(rv, fips_enabled);
		if (-1 == rv)
			return NULL; /* Relevant error detail populated in the above macro. */
	}
	OpenSSL_add_all_algorithms();
	/* Setup function pointers to symbols exported by libyottadb.so. */
	if (0 != gc_load_yottadb_symbols())
		return NULL;
	/* Setup a SSL context that allows TLSv1.x but no SSLv[23] (which is deprecated due to a great number of security
	 * vulnerabilities).
	 */
	if (NULL == (ctx = SSL_CTX_new(SSLv23_method())))
	{
		GC_APPEND_OPENSSL_ERROR("Failed to create an SSL context.");
		return NULL;
	}
	SSL_CTX_set_options(ctx, DEPRECATED_SSLTLS_PROTOCOLS);
#	ifdef GTM_ONLY
	/* Note: The below code is inherited from GT.M V6.3-008
	 * (http://tinco.pair.com/bhaskar/gtm/doc/articles/GTM_V6.3-008_Release_Notes.html#GTM-9084).
	 * And sets the connection to not use TLS 1.3 by default. This is because GT.M does not yet
	 * support TLS 1.3. But YottaDB does support it (fixed as part of YDB#376). Therefore
	 * keep the below code commented out.
	 */
	if (0 != SSL_OP_NO_TLSv1_3)
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_3);	/* until changes made to handle TLSv1.3 ciphers */
#	endif
	/* Read the configuration file for more configuration parameters. */
	cfg = &gtm_tls_cfg;
	config_init(cfg);
	if (NULL == (config_env = ydb_getenv(YDBENVINDX_CRYPT_CONFIG, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH)))
	{
		if (!(GTMTLS_OP_INTERACTIVE_MODE & flags))
		{	/* allow no config file if interactive for simple client usage */
			UPDATE_ERROR_STRING(ENV_UNDEF_ERROR, "ydb_crypt_config/gtmcrypt_config");
			SSL_CTX_free(ctx);
			return NULL;
		} else
			flags |= GTMTLS_OP_ABSENT_CONFIG;
	} else if (!config_read_file(cfg, config_env))
	{
		UPDATE_ERROR_STRING("Failed to read config file: %s. At line: %d, %s.", config_env, config_error_line(cfg),
						config_error_text(cfg));
		SSL_CTX_free(ctx);
		config_destroy(cfg);
		return NULL;
	}
	if (!(GTMTLS_OP_ABSENT_CONFIG & flags))
	{	/* check for tls section */
		if (NULL == config_lookup(cfg, "tls"))
			if ((GTMTLS_OP_INTERACTIVE_MODE & flags))
				flags |= GTMTLS_OP_ABSENT_CONFIG;
			else
			{
				UPDATE_ERROR_STRING("No tls: section in config file: %s", config_env);
				SSL_CTX_free(ctx);
				config_destroy(cfg);
				return NULL;
			}
	}
	/* Get global SSL configuration parameters */
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
			UPDATE_ERROR_STRING("Unknown verify-mode option: %.*s", parse_len, parse_ptr);
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		verify_mode = (int)verify_long;
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
			UPDATE_ERROR_STRING("Unknown verify-level option: %.*s",
				parse_len, parse_ptr);
			return NULL;
		}
		if (0 != level_clear)
		{
			verify_level = (int)level_clear;
			flags &= ~verify_level;
		}
		verify_level = (int)level_long;
		flags = (~GTMTLS_OP_VERIFY_LEVEL_MASK & flags) | verify_level;
	} else
		flags |= GTMTLS_OP_VERIFY_LEVEL_DEFAULT;
	rv1 = config_lookup_string(cfg, "tls.CAfile", &CAfile);
	rv2 = config_lookup_string(cfg, "tls.CApath", &CApath);
	/* Setup trust locations for peer verifications. This adds on to any trust locations that was previously loaded. */
	if ((rv1 || rv2) && !SSL_CTX_load_verify_locations(ctx, CAfile, CApath))
	{
		if (rv1 && rv2)
		{
			GC_APPEND_OPENSSL_ERROR("Failed to load CA verification locations (CAfile = %s; CApath = %s).",
							CAfile, CApath);
		} else
		{
			CAptr = rv1 ? CAfile : CApath;
			GC_APPEND_OPENSSL_ERROR("Failed to load CA verification location: %s.", CAptr);
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
		GC_APPEND_OPENSSL_ERROR("Failed to load default CA verification locations.");
		SSL_CTX_free(ctx);
		config_destroy(cfg);
		return NULL;
	}
	/* If a CRL is specified in the configuration file, add it to the cert store. */
	if (config_lookup_string(cfg, "tls.crl", &crl))
	{
		if (NULL == (store = SSL_CTX_get_cert_store(ctx)))
		{
			GC_APPEND_OPENSSL_ERROR("Failed to get handle to internal certificate store.");
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		if (NULL == (lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file())))
		{
			GC_APPEND_OPENSSL_ERROR("Failed to get handle to internal certificate store.");
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		if (0 == X509_LOOKUP_load_file(lookup, (char *)crl, X509_FILETYPE_PEM))
		{
			GC_APPEND_OPENSSL_ERROR("Failed to add Certificate Revocation List %s to internal certificate store.",
							crl);
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL);
	}
	SSL_CTX_clear_mode(ctx, SSL_MODE_AUTO_RETRY);
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
	else if (('\0' != cipher_list[0]) && (0 >= SSL_CTX_set_cipher_list(ctx, cipher_list)))
	{
		GC_APPEND_OPENSSL_ERROR("Failed to add Cipher-List command string: %s.", cipher_list);
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
			UPDATE_ERROR_STRING("Unknown ssl-options option: %.*s", parse_len, parse_ptr);
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
		if (options_current != options_mask)
			options_mask = SSL_CTX_set_options(ctx, options_mask);
		if (0 != options_clear)
#		if OPENSSL_VERSION_NUMBER >= 0x009080dfL
			options_mask = SSL_CTX_clear_options(ctx, options_clear);
#		else
		{	/* clear_options first in OpenSSL 0.9.8m */
			cfg_setting = config_lookup(cfg, "tls.ssl-options");
			cfg_file = config_setting_source_file(cfg_setting);
			cfg_line = config_setting_source_line(cfg_setting);
			UPDATE_ERROR_STRING("Unable to negate values in %s - need OpenSSL 0.9.8m or newer in %s line %d",
				"tls.ssl-options", cfg_file, cfg_line);
			SSL_CTX_free(ctx);
			config_destroy(cfg);
			return NULL;
		}
#		endif
	}
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
	gtm_tls_ctx->version = version;
	return gtm_tls_ctx;
}

STATICFNDEF gtmtls_passwd_list_t *gtm_tls_find_pwent(ydbenvindx_t envindx, char *input_suffix)
{
	gtmtls_passwd_list_t	*pwent_node;
	char			*suffix;
	int			suffixlen, input_suffixlen;

	input_suffixlen = (NULL != input_suffix) ? STRLEN(input_suffix) : 0;
	for (pwent_node = gtmtls_passwd_listhead; NULL != pwent_node; pwent_node = pwent_node->next)
	{	/* Lookup to see if we already have a password for the tlsid. */
		suffix = pwent_node->pwent->suffix;
		suffixlen = STRLEN(suffix);
		assert(PASSPHRASE_ENVNAME_MAX > (STRLEN(ydbenvname[envindx]) + suffixlen));
		if ((suffixlen == input_suffixlen) && (0 == strcmp(input_suffix, suffix)))
			break;	/* We already have a password for the tlsid. */
	}
	return pwent_node;
}

int gtm_tls_store_passwd(gtm_tls_ctx_t *tls_ctx, const char *tlsid, const char *obs_passwd)
{
	int			obs_len;
	gtmtls_passwd_list_t	*pwent_node;
	passwd_entry_t		*pwent;

	assert(tls_ctx);
	if (!(GTMTLS_OP_INTERACTIVE_MODE & tls_ctx->flags))
		return 0;	/* Not running in an interactive mode. */
	assert(NULL != tlsid);
	assert(0 < STRLEN(tlsid));
	assert(PASSPHRASE_ENVNAME_MAX > ((STRLEN(ydbenvname[YDBENVINDX_TLS_PASSWD_PREFIX]) + STRLEN(tlsid))));
	obs_len = STRLEN(obs_passwd);
	pwent_node = gtm_tls_find_pwent(YDBENVINDX_TLS_PASSWD_PREFIX, (char *)tlsid);
	if (NULL != pwent_node)
	{
		pwent = pwent_node->pwent;
		if ((obs_len == (pwent->passwd_len * 2)) && (0 == strncmp(obs_passwd, pwent->passwd, obs_len)))
			return 1;	/* already on the list */
	}
	/* Either no entry for tlsid or need to replace with new value */
	pwent = MALLOC(SIZEOF(passwd_entry_t));
	pwent->envindx = YDBENVINDX_TLS_PASSWD_PREFIX;
	SNPRINTF(pwent->suffix, SIZEOF(pwent->suffix), "%s", tlsid);
	pwent->env_value = MALLOC(obs_len + 1);
	memcpy(pwent->env_value, obs_passwd, obs_len + 1);	/* include null */
	pwent->passwd = NULL;
	pwent->passwd_len = 0;
	if (0 == gc_update_passwd(YDBENVINDX_TLS_PASSWD_PREFIX, (char *)tlsid, &pwent, NULL, GTMTLS_OP_NOPWDENVVAR))
	{
		pwent_node = MALLOC(SIZEOF(gtmtls_passwd_list_t));
		pwent_node->next = gtmtls_passwd_listhead;
		pwent_node->pwent = pwent;
		gtmtls_passwd_listhead = pwent_node;
	} else
		return -1;		/* gc_update_passwd freed pwent */
	return 1;
}

void gtm_tls_prefetch_passwd(gtm_tls_ctx_t *tls_ctx, char *env_name)
{
	char			*env_name_ptr, *env_value, prompt[GTM_PASSPHRASE_MAX_ASCII + 1];
	gtmtls_passwd_list_t	*pwent_node;
	passwd_entry_t		*pwent;

	assert(FALSE);	/* This assert replaces the below assert which is commented since this code is currently
			 * not invoked by anyone (which is what the new assert checks) but if/when this code
			 * is invoked, the above "getenv" call needs to be converted to an appropriate "ydb_getenv" call.
			 */
#	ifdef GTM_SOCKET_SSL_SUPPORT
	assert((NULL != (env_value = getenv(env_name))) && (0 == STRLEN(env_value)));
	if (!(GTMTLS_OP_INTERACTIVE_MODE & tls_ctx->flags))
		return;	/* Not running in an interactive mode. Cannot prompt for password. */
	env_name_ptr = (char *)env_name;
	assert(PASSPHRASE_ENVNAME_MAX > STRLEN(env_name_ptr));
	assert(SIZEOF(GTMTLS_PASSWD_ENV_PREFIX) - 1 < STRLEN(env_name_ptr));
	env_name_ptr += (SIZEOF(GTMTLS_PASSWD_ENV_PREFIX) - 1);
	SNPRINTF(prompt, GTM_PASSPHRASE_MAX_ASCII, "Enter passphrase for TLSID %s: ", env_name_ptr);
	pwent = NULL;
	assert(FALSE);	/* this is for the NULL use in the 2nd parameter below. It needs to be a non-null suffix string
			 * but not sure what that is now since this is unused code. Fix it if/when this assert fails.
			 */
	if (0 == gc_update_passwd(YDBENVINDX_TLS_PASSWD_PREFIX, NULL, &pwent, prompt, TRUE))
	{
		pwent_node = MALLOC(SIZEOF(gtmtls_passwd_list_t));
		pwent_node->next = gtmtls_passwd_listhead;
		pwent_node->pwent = pwent;
		gtmtls_passwd_listhead = pwent_node;
	}
	/* else, something went wrong while acquiring the password. Don't report it now. Later, `gtm_tls_socket' makes another
	 * attempt to acquire the password.
	 */
#	endif
}

static int copy_tlsid_elem(const config_t *tmpcfg, config_t *cfg, config_setting_t *tlsid, const char *idstr, const char *elemname,
				int type);
static int copy_tlsid_elem(const config_t *tmpcfg, config_t *cfg, config_setting_t *tlsid, const char *idstr, const char *elemname,
				int type)
{
	config_setting_t	*srcelem, *elem;
	char			cfg_path[MAX_CONFIG_LOOKUP_PATHLEN];

	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.%s", idstr, elemname);
	if (srcelem = config_lookup(tmpcfg, cfg_path))
	{
		elem = config_lookup(cfg, cfg_path);
		if (NULL == elem)
		{	/* option not currently in tls.idstr so create */
			elem = config_setting_add(tlsid, elemname, type);
			if (NULL == elem)
			{
				UPDATE_ERROR_STRING("Failed to add TLSID: %s item %s to config file: %s", idstr, elemname,
					config_error_text(cfg));
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
			UPDATE_ERROR_STRING("gtm_tls_impl.c/copy_tlsid_elem:  Unexpected CONFIG_TYPE %d for item %s", type,
				elemname);
			return -1;
		}
	}
	return 0;
}

int gtm_tls_add_config(gtm_tls_ctx_t *tls_ctx, const char *idstr, const char *configstr)
{
#	ifndef LIBCONFIG_VER_MAJOR
	UPDATE_ERROR_STRING("TLSID: %s: libconfig 1.4.x is needed to support adding config information", idstr);
	return -1;
#	else
	config_t		*cfg, tmpcfg;
	config_setting_t	*tlsid, *tlssect;
	char			cfg_path[MAX_CONFIG_LOOKUP_PATHLEN];

	config_init(&tmpcfg);
	if (CONFIG_FALSE == config_read_string(&tmpcfg, configstr))
	{
		UPDATE_ERROR_STRING("Failed to add config information: %s in line %d:\n%s", config_error_text(&tmpcfg),
			config_error_line(&tmpcfg), configstr);
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
			UPDATE_ERROR_STRING("Failed to add tls section to config file: %s", config_error_text(cfg));
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
			UPDATE_ERROR_STRING("Failed to add TLSID: %s section to config file: %s", idstr, config_error_text(cfg));
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
	long			options_mask, options_current, options_clear, verify_long, level_long, level_clear;
	char			*optionendptr, *parse_ptr;
#	if (((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 4)) || (LIBCONFIG_VER_MAJOR > 1))
	int			verify_depth, session_timeout;
#	else
	long int		verify_depth, session_timeout;
#	endif
	char			cfg_path[MAX_CONFIG_LOOKUP_PATHLEN], *env_name_ptr;
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
#	if OPENSSL_VERSION_NUMBER < 0x009080dfL
	unsigned int		cfg_line;
	const char		*cfg_file;
#	endif
	gtmtls_passwd_list_t	*pwent_node;
	passwd_entry_t		*pwent;
	gtm_tls_socket_t	*socket;
	STACK_OF(X509_NAME)	*CAcerts;
#	ifndef SSL_OP_NO_COMPRESSION
	STACK_OF(SSL_COMP)*	compression;
#	endif

	DBG_VERIFY_SOCK_IS_BLOCKING(sockfd);
	ctx = tls_ctx->ctx;
	cfg = &gtm_tls_cfg;

	/* Create a SSL object. This object will be used for the actual I/O: recv/send */
	if (NULL == (ssl = SSL_new(ctx)))
	{
		GC_APPEND_OPENSSL_ERROR("Failed to obtain a new SSL/TLS object.");
		return NULL;
	}

	if ('\0' != id[0])
	{
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s", id);
		cfg_setting = config_lookup(cfg, cfg_path);
		if (NULL == cfg_setting)
		{
			UPDATE_ERROR_STRING("TLSID %s not found in configuration file.", id);
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
				UPDATE_ERROR_STRING("In TLSID: %s - unknown verify-mode option: %.*s", id, parse_len, parse_ptr);
				SSL_free(ssl);
				return NULL;
			}
			verify_mode = (int)verify_long;
			if (SSL_VERIFY_PEER & verify_mode)
				flags |= GTMTLS_OP_VERIFY_PEER;
			verify_mode_set = TRUE;
		} else if (GTMTLS_OP_ABSENT_VERIFYMODE & tls_ctx->flags)
		{
			verify_mode = SSL_VERIFY_PEER;
			verify_mode_set = TRUE;
		} else
			verify_mode_set = FALSE;
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
				UPDATE_ERROR_STRING("In TLSID: %s - unknown verify-level option: %.*s",
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
		UPDATE_ERROR_STRING("Server mode requires a certificate but no TLSID specified");
		SSL_free(ssl);
		return NULL;
	} else
	{
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
	if (verify_mode_set)
		SSL_set_verify(ssl, verify_mode, NULL);
	if (verify_level_set)
		flags = (~GTMTLS_OP_VERIFY_LEVEL_MASK & flags) | verify_level;
	else
		flags = (~GTMTLS_OP_VERIFY_LEVEL_MASK & flags) | (GTMTLS_OP_VERIFY_LEVEL_MASK & tls_ctx->flags);
	if (NULL == cipher_list)
	{	/* no cipher-list in labelled section or no section */
		if (0 != ((GTMTLS_OP_ABSENT_CIPHER | GTMTLS_OP_DEFAULT_CIPHER) & tls_ctx->flags))
		{	/* no or default cipher specified top level */
			cipher_list = (GTMTLS_OP_SOCKET_DEV & flags) ? GTM_DEFAULT_CIPHER_LIST : REPL_CIPHER_LIST;
		}
	}
	if ((NULL != cipher_list) && (0 >= SSL_set_cipher_list(ssl, cipher_list)))
	{
		GC_APPEND_OPENSSL_ERROR("Failed to add Cipher-List command string: %s.", cipher_list);
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
					UPDATE_ERROR_STRING("Certificate required for TLSID: %s"
						" but no configuration information available.", id);
				} else
				{
					UPDATE_ERROR_STRING("Certificate corresponding to TLSID: %s"
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
				UPDATE_ERROR_STRING("Private key but no certificate corresponding to TLSID:"
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
				UPDATE_ERROR_STRING("Format but no certificate corresponding to TLSID: %s in configuration file.",
					id);
				SSL_free(ssl);
				return NULL;
			}
			if (((SIZEOF("PEM") - 1) != strlen(format))
				|| (format[0] != 'P') || (format[1] != 'E') || (format[2] != 'M'))
			{
				UPDATE_ERROR_STRING("Unsupported format type %s found for TLSID: %s.", format, id);
				SSL_free(ssl);
				return NULL;
			}
		}
		if (!nocert)
		{
			/* Setup the certificate to be used for this connection */
			if (!SSL_use_certificate_file(ssl, cert, SSL_FILETYPE_PEM))
			{
				GC_APPEND_OPENSSL_ERROR("Failed to add certificate %s.", cert);
				SSL_free(ssl);
				return NULL;
			}
			/* Before setting up the private key, check-up on the password for the private key. */
			/* Lookup to see if we have already prefetched the password. */
			pwent_node = gtm_tls_find_pwent(YDBENVINDX_TLS_PASSWD_PREFIX, id);
			if (NULL == pwent_node)
			{	/* Lookup failed. Create a new entry for the given id. */
				pwent = NULL;
				SNPRINTF(prompt, GTM_PASSPHRASE_MAX_ASCII, "Enter passphrase for TLSID %s:", id);
				if (0 != gc_update_passwd(YDBENVINDX_TLS_PASSWD_PREFIX, id, &pwent, prompt, 0))
				{
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
				/* If fclose(fp) fails, ignore it particularly because we opened the file in "r" mode only
				 * so there is no danger of loss of updates. If evp_pkey is non-NULL, we definitely want to
				 * proceed. If it is NULL, we will error out a few lines later. Hence the (void) below.
				 */
				(void)fclose(fp);
			} else
				evp_pkey = NULL;
			if (NULL == evp_pkey)
			{
				if (NULL == fp)
				{
					UPDATE_ERROR_STRING("Private Key corresponding to TLSID:"
						" %s - error opening file %s: %s.", id, private_key, strerror(errno));
				} else if (ERR_GET_REASON(ERR_peek_error()) == PEM_R_NO_START_LINE)
				{	/* give clearer error if only cert given but it doesn't have the key */
					UPDATE_ERROR_STRING("Private Key corresponding to TLSID:"
						" %s not found in configuration file.", id);
				} else
				{
					GC_APPEND_OPENSSL_ERROR("Failed to read private key %s.", private_key);
				}
				SSL_free(ssl);
				return NULL;
			}
			if (!SSL_use_PrivateKey(ssl, evp_pkey))
			{
				GC_APPEND_OPENSSL_ERROR("Failed to use private key %s.", private_key);
				SSL_free(ssl);
				return NULL;
			}
			/* Verify that private key matches the certificate */
			if (!SSL_check_private_key(ssl))
			{
				GC_APPEND_OPENSSL_ERROR("Consistency check failed for private key: %s and certificate: %s\n",
						private_key, cert);
				SSL_free(ssl);
				return NULL;
			}
		}
	}
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
	if ('\0' != id[0])
	{
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
				UPDATE_ERROR_STRING("In TLSID: %s - unknown ssl-options option: %.*s", id, parse_len, parse_ptr);
				SSL_free(ssl);
				return NULL;
			}
			if (options_current != options_mask)
				options_mask = SSL_set_options(ssl, options_mask);
			if (0 != options_clear)
#			if OPENSSL_VERSION_NUMBER >= 0x009080dfL
				options_mask = SSL_clear_options(ssl, options_clear);
#			else
			{       /* clear_options first in OpenSSL 0.9.8m */
				cfg_setting = config_lookup(cfg, cfg_path);
				cfg_file = config_setting_source_file(cfg_setting);
				cfg_line = config_setting_source_line(cfg_setting);
				UPDATE_ERROR_STRING("Unable to negate values in %s - need OpenSSL 0.9.8m or newer in %s line %d",
					cfg_path, cfg_file, cfg_line);
				SSL_free(ssl);
				return NULL;
			}
#			endif

		}
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.verify-depth", id);
		if (CONFIG_TRUE == config_lookup_int(cfg, cfg_path, &verify_depth))
			SSL_set_verify_depth(ssl, verify_depth);
	}
	if (!CLIENT_MODE(flags))
	{	/* Socket created for server mode operation. Set a session ID context for session resumption at the time of
		 * reconnection.
		 */
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.session-id-hex", id);
		if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &session_id_hex))
		{	/* convert hex to char and set len */
			session_id_len = STRLEN(session_id_hex);
			if (MAX_SESSION_ID_LEN < session_id_len)
				session_id_len = MAX_SESSION_ID_LEN;	/* avoid overrun */
			GC_UNHEX(session_id_hex, session_id_string, session_id_len);
			if (-1 == session_id_len)
			{
				UPDATE_ERROR_STRING("In TLSID: %s - invalid session-id-hex value: %s", id, session_id_hex);
				tls_errno = -1;
				SSL_free(ssl);
				return NULL;
			}
			session_id_len = session_id_len / 2;		/* bytes */
		} else
		{
			session_id_len = STRLEN(id);
			if (SIZEOF(session_id_string) <= session_id_len)
				session_id_len = SIZEOF(session_id_string) - 1;
			memcpy((char *)session_id_string, id, session_id_len);		/* default to tlsid */
			session_id_string[session_id_len] = '\0';
		}
		if (0 >= SSL_set_session_id_context(ssl, (const unsigned char *)session_id_string, (unsigned int)session_id_len))
		{
			GC_APPEND_OPENSSL_ERROR("Failed to set Session-ID context to enable session resumption.");
			SSL_free(ssl);
			return NULL;
		}
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
	}
	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.CAfile", id);
	tlscafile = config_lookup_string(cfg, cfg_path, &CAfile);	/* if absent CAfile retains value */
	if (CONFIG_FALSE != tlscafile)
	{
		assert(NULL != CAfile);
		if (!(GTMTLS_OP_CA_LOADED & tls_ctx->flags))
		{	/* no CAfile or CApath before so do now */
			if (!SSL_CTX_load_verify_locations(tls_ctx->ctx, CAfile, NULL))
			{
				GC_APPEND_OPENSSL_ERROR("Failed to load CA verification location: %s.", CAfile);
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
				GC_APPEND_OPENSSL_ERROR("Failed to load client CA file %s", CAfile);
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
		GC_APPEND_OPENSSL_ERROR("Failed to associate TCP/IP socket descriptor %d with an SSL/TLS descriptor", sockfd);
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
	SNPRINTF(socket->tlsid, SIZEOF(socket->tlsid), "%s", (const char *)id);
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

STATICFNDEF int ydb_tls_is_supported(SSL *ssl);

/* Function to check if the negotiated TLS version of the connection is a supported version.
 * Returns  0 if supported.
 *         -1 if not supported.
 */
STATICFNDEF int ydb_tls_is_supported(SSL *ssl)
{
	int	ssl_version;

	ssl_version = SSL_version(ssl);
	switch (ssl_version)
	{
		case TLS1_1_VERSION:
		case TLS1_2_VERSION:
		case TLS1_3_VERSION:
			/* The above are the supported TLS versions */
			return 0;
			break;
		default:
			/* Anything else is currently unsupported */
			UPDATE_ERROR_STRING("%s version unsupported by the TLS plug-in", SSL_get_version(ssl));
			tls_errno = -1;
			break;
	}
	return -1;
}

int gtm_tls_connect(gtm_tls_socket_t *socket)
{
	int		rv;

	assert(CLIENT_MODE(socket->flags));
	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	if (NULL != socket->session)
	{	/* Old session available. Reuse it. */
		SSL_DPRINT(stdout, "gtm_tls_connect(1): references=%d\n", ((SSL_SESSION *)(socket->session))->references);
		if (0 >= (rv = SSL_set_session(socket->ssl, socket->session)))
			return ssl_error(socket, rv, X509_V_OK);
		SSL_DPRINT(stdout, "gtm_tls_connect(2): references=%d\n", ((SSL_SESSION *)(socket->session))->references);
	}
	if (0 >= (rv = SSL_connect(socket->ssl)))
		return ssl_error(socket, rv, X509_V_OK);
	if (NULL != socket->session)
		SSL_DPRINT(stdout, "gtm_tls_connect(3): references=%d\n", ((SSL_SESSION *)(socket->session))->references);
	return ydb_tls_is_supported(socket->ssl);
}

int gtm_tls_accept(gtm_tls_socket_t *socket)
{
	int	rv;

	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	rv = SSL_accept(socket->ssl);
	if (0 >= rv)
		return ssl_error(socket, rv, X509_V_OK);
	return ydb_tls_is_supported(socket->ssl);
}

int gtm_tls_renegotiate(gtm_tls_socket_t *socket)
{
	int		rv, ssl_version;
	long		vresult;
	SSL		*ssl;

	ssl = socket->ssl;
	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(ssl));
	/* TLSv1.3 does not have renegotiation so calls to "SSL_renegotiate" will immediately fail if invoked on a
	 * connection that has negotiated TLSv1.3. Since the purpose of "gtm_tls_renegotiate" is to update the connection
	 * keys, use "SSL_key_update" for that purpose in TLS v1.3.
	 */
	ssl_version = SSL_version(ssl);
	rv = 1;	/* set "rv" at start to success return value of "SSL_renegotiate"/"SSL_key_update" */
	if (TLS1_3_VERSION > ssl_version)
		rv = SSL_renegotiate(ssl);	/* Schedule a renegotiation */
	/* Note: "SSL_key_update" function is available only from OpenSSL 1.1.1 onwards hence the OPENSSL_VERSION_NUMBER check
	 * below to avoid a compile error if this plugin is compiled on a system which has an older OpenSSL version installed.
	 * See "man OPENSSL_VERSION_NUMBER" for details on its format.
	 */
#	if OPENSSL_VERSION_NUMBER >= 0x1010100fL
	else
		rv = SSL_key_update(ssl, SSL_KEY_UPDATE_REQUESTED);
#	endif
	if (0 >= rv)
		return ssl_error(socket, rv, SSL_get_verify_result(ssl));
	do
	{
		rv = SSL_do_handshake(ssl);
		vresult = SSL_get_verify_result(ssl);
		if ((0 < rv) && (X509_V_OK == vresult))
			return 0;
		/* On a blocking socket, SSL_do_handshake returns ONLY after successful completion. However, if the system call
		 * is interrupted (say, by a SIGALRM), it can return with a WANT_READ or WANT_WRITE. Handle it by retrying.
		 * Ideally, we should return back to the caller and let it handle WANT_READ/WANT_WRITE and call us again, but
		 * since renegotiation is done seldomly and returning the control back to the caller causes interface issues, we
		 * handle GTMTLS_WANT_READ or GTMTLS_WANT_WRITE by retrying.
		 */
		rv = ssl_error(socket, rv, vresult);
	} while ((GTMTLS_WANT_READ == rv) || (GTMTLS_WANT_WRITE == rv));
	return rv;
}

#	if (((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 4)) || (LIBCONFIG_VER_MAJOR > 1))
#define VERIFY_DEPTH_TYPE	int
#	else
#define VERIFY_DEPTH_TYPE	long int
#endif

STATICFNDEF int gtm_tls_renegotiate_options_config(char *idstr, int flags, config_t *cfg, VERIFY_DEPTH_TYPE *verify_depth,
			int *verify_depth_set, int *verify_mode, int *verify_mode_set, int *verify_level, int *verify_level_set,
			int *session_id_len, unsigned char *session_id_string, const char **CAfile);

STATICFNDEF int gtm_tls_renegotiate_options_config(char *idstr, int flags, config_t *cfg, VERIFY_DEPTH_TYPE *verify_depth,
			int *verify_depth_set, int *verify_mode, int *verify_mode_set, int *verify_level, int *verify_level_set,
			int *session_id_len, unsigned char *session_id_string, const char **CAfile)
{
	int			parse_len;
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
			UPDATE_ERROR_STRING("In TLSID: %s - unknown verify-mode option: %.*s",
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
			UPDATE_ERROR_STRING("In TLSID: %s - unknown verify-level option: %.*s",
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
	if (CONFIG_TRUE != config_lookup_string(cfg, cfg_path, CAfile)) {
		UPDATE_ERROR_STRING("In TLSID: %s - CAfile not found or invalid type", idstr);
		tls_errno = -1;
		return -1;
	}
	SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.session-id-hex", idstr);
	if (CONFIG_TRUE == config_lookup_string(cfg, cfg_path, &session_id_hex))
	{	/* convert hex to char and set len */
		*session_id_len = STRLEN(session_id_hex);
		if (MAX_SESSION_ID_LEN < *session_id_len)
			*session_id_len = MAX_SESSION_ID_LEN;	/* avoid overrun */
		GC_UNHEX(session_id_hex, session_id_string, *session_id_len);
		if (-1 == *session_id_len)
		{
			UPDATE_ERROR_STRING("In TLSID: %s - invalid session-id-hex value: %s",
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
				UPDATE_ERROR_STRING("TLSID %s not found in configuration file.", idstr);
				tls_errno = -1;
				return -1;
			}
			if (0 != gtm_tls_renegotiate_options_config(idstr, flags, cfg, &verify_depth, &verify_depth_set,
					&verify_mode, &verify_mode_set, &verify_level, &verify_level_set, &session_id_len,
					session_id_string, &CAfile))
				return -1;
		}
		if (NULL != configstr)
		{	/* now process any options given on WRITE /TLS */
#			ifndef LIBCONFIG_VER_MAJOR
			UPDATE_ERROR_STRING("TLSID: %s: libconfig 1.4.x is needed to support providing options on WRITE /TLS",
				idstr);
			tls_errno = -1;
			return -1;
#			else
			config_init(&tmpcfg);
			if (CONFIG_FALSE == config_read_string(&tmpcfg, configstr))
			{
				UPDATE_ERROR_STRING("Failed to process options: %s in line %d:\n%s",
					config_error_text(&tmpcfg),
					config_error_line(&tmpcfg), configstr);
				tls_errno = -1;
				return -1;
			}
			if (0 != gtm_tls_renegotiate_options_config(idstr, flags, &tmpcfg, &verify_depth, &verify_depth_set,
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
			flags = (~GTMTLS_OP_VERIFY_LEVEL_MASK & flags) | verify_level;
		if ((NULL == CAfile) && !(GTMTLS_OP_CLIENT_CA & flags))
		{	/* check for tlsid.CAfile or tls.CAfile */
			SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.CAfile", socket->tlsid);
			rv = config_lookup_string(cfg, cfg_path, &CAfile);
			if (CONFIG_FALSE == rv)
			{	/* We intentionally do not check the return value of the below call. If it returns CONFIG_FALSE,
				 * we are guaranteed CAfile is NULL. In that case, we will anyways skip the later "if" block
				 * of code that handles a non-NULL CAfile. No error is needed in that case.
				 */
				assert(NULL == CAfile);
				(void)config_lookup_string(cfg, "tls.CAfile", &CAfile);
			}
		}
		if (NULL != CAfile)
		{
			CAcerts = SSL_load_client_CA_file(CAfile);
			if (NULL == CAcerts)
			{
				GC_APPEND_OPENSSL_ERROR("Failed to load client CA file %s", CAfile);
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
			GC_APPEND_OPENSSL_ERROR("Failed to set Session-ID context to enable session resumption.");
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
	const SSL_CIPHER	*cipher;
	const COMP_METHOD	*compression_method;
	char			*session_id_ptr;
	gtm_tls_ctx_t		*tls_ctx;
	X509			*peer;
	SSL			*ssl;
	EVP_PKEY		*pubkey;
	SSL_SESSION		*session;

	ssl = socket->ssl;
	tls_ctx = socket->gtm_ctx;
	peer = SSL_get_peer_certificate(ssl);
	if ((NULL != peer) || (GTMTLS_OP_SOCKET_DEV & socket->flags))
	{	/* if socket device and no certificate from peer still provide info */
		verify_result = SSL_get_verify_result(ssl);
		if ((X509_V_OK == verify_result) || (GTMTLS_OP_ABSENT_CONFIG & tls_ctx->flags))
		{	/* return information for Socket clients even without a config file */
			/* SSL-Session Protocol */
			SNPRINTF(conn_info->protocol, SIZEOF(conn_info->protocol), "%s", SSL_get_version(ssl));
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
#			if OPENSSL_VERSION_NUMBER >= 0x009080dfL
			/* SSL_get_secure_renegotiation_support function was introduced in OpenSSL version >= "0.9.8m". */
			conn_info->secure_renegotiation = SSL_get_secure_renegotiation_support(ssl);
#			else
			conn_info->secure_renegotiation = FALSE;
#			endif
			/* Is the session reused? */
			conn_info->reused = SSL_session_reused(ssl);
			/* Negotiated Session-ID. */
			if (NULL == (session = SSL_get1_session(ssl)))	/* `get1' version is used to increment reference count. */
			{
				UPDATE_ERROR_STRING("Failed to obtain the handle to negotiated SSL/TLS session");
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
				X509_free(peer);
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
			UPDATE_ERROR_STRING("Peer certificate invalid: %s", X509_verify_cert_error_string(verify_result));
			X509_free(peer);
			return -1;
		}
	} else
		UPDATE_ERROR_STRING("No certificate sent from the remote side");
	return -1;
}

int gtm_tls_send(gtm_tls_socket_t *socket, char *buf, int send_len)
{
	int		rv;

	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	rv = SSL_write(socket->ssl, buf, send_len);
	if (0 >= rv)
		return ssl_error(socket, rv, X509_V_OK);
	return rv;
}

int gtm_tls_recv(gtm_tls_socket_t * socket, char *buf, int recv_len)
{
	int		rv;

	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	rv = SSL_read(socket->ssl, buf, recv_len);
	if (0 >= rv)
		return ssl_error(socket, rv, X509_V_OK);
	return rv;
}

int gtm_tls_cachedbytes(gtm_tls_socket_t *socket)
{
	return SSL_pending(socket->ssl);
}

void gtm_tls_socket_close(gtm_tls_socket_t *socket)
{
	tls_errno = 0;
	if ((NULL == socket) || (NULL == socket->ssl))
		return;
	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	/* Invoke SSL_shutdown to close the SSL/TLS connection. Although the protocol (and the OpenSSL library) supports
	 * bidirectional shutdown (which waits for the peer's "close notify" alert as well), we intend to only send the
	 * "close notify" alert and be done with it. This is because the process is done with the connection when it calls
	 * this function and we don't want to consume additional time waiting for a "close notify" acknowledge signal from the
	 * other side.
	 */
	SSL_shutdown(socket->ssl);
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
		SSL_DPRINT(stdout, "gtm_tls_session_close: references=%d\n", session->references);
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
