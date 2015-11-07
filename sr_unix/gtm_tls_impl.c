/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
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

GBLDEF	int			tls_errno;
GBLDEF	config_t		gtm_tls_cfg;
GBLDEF	gtmtls_passwd_list_t	*gtmtls_passwd_listhead;

STATICDEF DH			*dh512, *dh1024;	/* Diffie-Hellman structures for Ephemeral Diffie-Hellman key exchange. */

#define MAX_CONFIG_LOOKUP_PATHLEN	64

/* Below template translates to: Arrange ciphers in increasing order of strength after excluding the following:
 * ADH: Anonymous Diffie-Hellman Key Exchange (Since we want both encryption and authentication and ADH provides only former).
 * LOW: Low strength ciphers.
 * EXP: Export Ciphers.
 * MD5 : MD5 message digest.
 */
#define CIPHER_LIST			"ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"

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

STATICFNDEF int ssl_error(SSL *ssl, int err)
{
	int		error_code;
	char		*errptr, *end;

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

		case SSL_ERROR_WANT_X509_LOOKUP:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_WANT_CONNECT:
			assert(FALSE);

		case SSL_ERROR_SSL:
			errptr = gtmcrypt_err_string;
			end = errptr + MAX_GTMCRYPT_ERR_STRLEN;
			tls_errno = -1;
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
	assert(dh512 && dh1024 && (512 <= keylength));
	return (512 == keylength) ? dh512 : dh1024;
}

_GTM_APIDEF int		gtm_tls_errno(void)
{
	return tls_errno;
}

_GTM_APIDEF const char	*gtm_tls_get_error(void)
{
	return gtmcrypt_err_string;
}

_GTM_APIDEF gtm_tls_ctx_t	*gtm_tls_init(int version, int flags)
{
	const char		*CAfile = NULL, *CApath = NULL, *crl, *CAptr;
	char			*config_env;
	int			rv, rv1, rv2, fips_requested, fips_enabled;
#	if (((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 4)) || (LIBCONFIG_VER_MAJOR > 1))
	int			verify_depth, session_timeout;
#	else
	long int		verify_depth, session_timeout;
#	endif
	SSL_CTX			*ctx;
	X509_STORE		*store;
	X509_LOOKUP		*lookup;
	config_t		*cfg;
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
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	/* Setup function pointers to symbols exported by libgtmshr.so. */
	if (0 != gc_load_gtmshr_symbols())
		return NULL;
	/* Setup a SSL context that allows SSLv3 and TLSv1.x but no SSLv2 (which is deprecated due to a great number of security
	 * vulnerabilities).
	 */
	if (NULL == (ctx = SSL_CTX_new(SSLv23_method())))
	{
		GC_APPEND_OPENSSL_ERROR("Failed to create an SSL context.");
		return NULL;
	}
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	/* Read the configuration file for more configuration parameters. */
	if (NULL == (config_env = getenv("gtmcrypt_config")))
	{
		UPDATE_ERROR_STRING(ENV_UNDEF_ERROR, "gtmcrypt_config");
		SSL_CTX_free(ctx);
		return NULL;
	}
	cfg = &gtm_tls_cfg;
	config_init(cfg);
	if (!config_read_file(cfg, config_env))
	{
		UPDATE_ERROR_STRING("Failed to read config file: %s. At line: %d, %s.", config_env, config_error_line(cfg),
						config_error_text(cfg));
		SSL_CTX_free(ctx);
		config_destroy(cfg);
		return NULL;
	}
	/* Get global SSL configuration parameters */
	if (config_lookup_int(cfg, "tls.verify-depth", &verify_depth))
		SSL_CTX_set_verify_depth(ctx, verify_depth);
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
	if (0 >= SSL_CTX_set_cipher_list(ctx, CIPHER_LIST))
	{
		GC_APPEND_OPENSSL_ERROR("Failed to add Cipher-List command string: %s.", CIPHER_LIST);
		SSL_CTX_free(ctx);
		config_destroy(cfg);
		return NULL;
	}
	gtm_tls_ctx = MALLOC(SIZEOF(gtm_tls_ctx_t));
	gtm_tls_ctx->ctx = ctx;
	gtm_tls_ctx->flags = flags;
	gtm_tls_ctx->fips_mode = fips_enabled;
	gtm_tls_ctx->compile_time_version = OPENSSL_VERSION_NUMBER;
	gtm_tls_ctx->runtime_version = SSLeay();
	return gtm_tls_ctx;
}

_GTM_APIDEF void gtm_tls_prefetch_passwd(gtm_tls_ctx_t *tls_ctx, char *env_name)
{
	char			*env_name_ptr, *env_value, prompt[256];
	gtmtls_passwd_list_t	*pwent_node;
	passwd_entry_t		*pwent;

	assert((NULL != (env_value = getenv(env_name))) && (0 == STRLEN(env_value)));
	if (!(GTMTLS_OP_INTERACTIVE_MODE & tls_ctx->flags))
		return;	/* Not running in an interactive mode. Cannot prompt for password. */
	env_name_ptr = (char *)env_name;
	assert(PASSPHRASE_ENVNAME_MAX > STRLEN(env_name_ptr));
	assert(SIZEOF(GTMTLS_PASSWD_ENV_PREFIX) - 1 < STRLEN(env_name_ptr));
	env_name_ptr += (SIZEOF(GTMTLS_PASSWD_ENV_PREFIX) - 1);
	SNPRINTF(prompt, 256, "Enter passphrase for TLSID %s: ", env_name_ptr);
	pwent = NULL;
	if (0 == gc_update_passwd(env_name, &pwent, prompt, TRUE))
	{
		pwent_node = MALLOC(SIZEOF(gtmtls_passwd_list_t));
		pwent_node->next = gtmtls_passwd_listhead;
		pwent_node->pwent = pwent;
		gtmtls_passwd_listhead = pwent_node;
	}
	/* else, something went wrong while acquiring the password. Don't report it now. Later, `gtm_tls_socket' makes another
	 * attempt to acquire the password.
	 */
}

_GTM_APIDEF gtm_tls_socket_t *gtm_tls_socket(gtm_tls_ctx_t *tls_ctx, gtm_tls_socket_t *prev_socket, int sockfd, char *id, int flags)
{
	int			len;
	char			cfg_path[MAX_CONFIG_LOOKUP_PATHLEN], input_env_name[PASSPHRASE_ENVNAME_MAX], *env_name_ptr;
	char			prompt[256];
	const char		*cert, *private_key, *format;
	FILE			*fp;
	SSL			*ssl;
	SSL_CTX			*ctx;
	EVP_PKEY		*evp_pkey = NULL;
	config_t		*cfg;
	gtmtls_passwd_list_t	*pwent_node;
	passwd_entry_t		*pwent;
	gtm_tls_socket_t	*socket;
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

	if (VERIFY_PEER(flags))
	{
		/* First lookup the certificate and private key associated with the provided id in the configuration file. */
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.cert", id);
		if (!config_lookup_string(cfg, cfg_path, &cert))
		{
			UPDATE_ERROR_STRING("Certificate corresponding to TLSID: %s not found in configuration file.", id);
			SSL_free(ssl);
			return NULL;
		}

		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.key", id);
		if (!config_lookup_string(cfg, cfg_path, &private_key))
		{
			UPDATE_ERROR_STRING("Private Key corresponding to TLSID: %s not found in configuration file.", id);
			SSL_free(ssl);
			return NULL;
		}

		/* Verify that the format, if specified, is of PEM type as that's the only kind we support now. */
		SNPRINTF(cfg_path, MAX_CONFIG_LOOKUP_PATHLEN, "tls.%s.format", id);
		if (config_lookup_string(cfg, cfg_path, &format))
		{
			if (((SIZEOF("PEM") - 1) != strlen(format))
					|| (format[0] != 'P') || (format[1] != 'E') || (format[2] != 'M'))
			{
				UPDATE_ERROR_STRING("Unsupported format type %s found for TLSID: %s.", format, id);
				SSL_free(ssl);
				return NULL;
			}
		}
		/* Setup the certificate to be used for this connection */
		if (!SSL_use_certificate_file(ssl, cert, SSL_FILETYPE_PEM))
		{
			GC_APPEND_OPENSSL_ERROR("Failed to add certificate %s.", cert);
			SSL_free(ssl);
			return NULL;
		}

		/* Before setting up the private key, check-up on the password for the private key. */
		SNPRINTF(input_env_name, PASSPHRASE_ENVNAME_MAX, GTMTLS_PASSWD_ENV_PREFIX "%s", id);
		if (NULL != (pwent_node = gtmtls_passwd_listhead))
		{	/* Lookup to see if we have already prefetched the password. */
			while (NULL != pwent_node)
			{
				env_name_ptr = pwent_node->pwent->env_name;
				len = STRLEN(env_name_ptr);
				assert(len < PASSPHRASE_ENVNAME_MAX);
				assert(len > SIZEOF(GTMTLS_PASSWD_ENV_PREFIX) - 1);
				if ((len == STRLEN(input_env_name)) && (0 == strncmp(input_env_name, env_name_ptr, len)))
					break;
				pwent_node = pwent_node->next;
			}
		}
		if (NULL == pwent_node)
		{	/* Lookup failed. Create a new entry for the given id. */
			pwent = NULL;
			SNPRINTF(prompt, 256, "Enter passphrase for TLSID %s:", id);
			if (0 != gc_update_passwd(input_env_name, &pwent, prompt, GTMTLS_OP_INTERACTIVE_MODE & tls_ctx->flags))
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
		/* Setup the private key corresponding to the certificate and the callback function to obtain the password for the
		 * key. We cannot use the much simpler SSL_use_PrivateKey file to load the private key file because we want fine
		 * grained control on the password callback mechanism. For this purpose, use the PEM_read_PrivateKey function which
		 * supports callbacks for individual private keys.
		 */
		fp = fopen(private_key, "r");
		evp_pkey = PEM_read_PrivateKey(fp, &evp_pkey, &passwd_callback, pwent);
		fclose(fp);	/* Close the file irrespective of whether the above function succeeded or failed. */
		if (NULL == evp_pkey)
		{
			GC_APPEND_OPENSSL_ERROR("Failed to read private key %s.", private_key);
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
		SSL_set_verify(ssl, SSL_VERIFY_PEER, NULL);
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
	if (0 == (GTMTLS_OP_CLIENT_MODE & flags))
	{	/* Socket created for server mode operation. Set a session ID context for session resumption at the time of
		 * reconnection.
		 */
		if (0 >= SSL_set_session_id_context(ssl, (const unsigned char *)id, STRLEN(id)))
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

_GTM_APIDEF int		gtm_tls_connect(gtm_tls_socket_t *socket)
{
	int		rv;

	assert(socket->flags & GTMTLS_OP_CLIENT_MODE);
	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	if (NULL != socket->session)
	{	/* Old session available. Reuse it. */
		SSL_DPRINT(stdout, "gtm_tls_connect(1): references=%d\n", ((SSL_SESSION *)(socket->session))->references);
		if (0 >= (rv = SSL_set_session(socket->ssl, socket->session)))
			return ssl_error(socket->ssl, rv);
		SSL_DPRINT(stdout, "gtm_tls_connect(2): references=%d\n", ((SSL_SESSION *)(socket->session))->references);
	}
	if (0 < (rv = SSL_connect(socket->ssl)))
	{
		if (NULL != socket->session)
			SSL_DPRINT(stdout, "gtm_tls_connect(3): references=%d\n", ((SSL_SESSION *)(socket->session))->references);
		return 0;
	}
	return ssl_error(socket->ssl, rv);
}

_GTM_APIDEF int		gtm_tls_accept(gtm_tls_socket_t *socket)
{
	int		rv;

	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	if (0 < (rv = SSL_accept(socket->ssl)))
		return 0;
	return ssl_error(socket->ssl, rv);
}

_GTM_APIDEF int gtm_tls_renegotiate(gtm_tls_socket_t *socket)
{
	int		rv;

	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	if (0 >= (rv = SSL_renegotiate(socket->ssl)))
		return ssl_error(socket->ssl, rv);
	do
	{
		if (0 < (rv = SSL_do_handshake(socket->ssl)))
			return 0;
		/* On a blocking socket, SSL_do_handshake returns ONLY after successful completion. However, if the system call
		 * is interrupted (say, by a SIGALRM), it can return with a WANT_READ or WANT_WRITE. Handle it by retrying.
		 * Ideally, we should return back to the caller and let it handle WANT_READ/WANT_WRITE and call us again, but
		 * since renegotiation is done seldomly and returning the control back to the caller causes interface issues, we
		 * handle GTMTLS_WANT_READ or GTMTLS_WANT_WRITE by retrying.
		 */
		rv = ssl_error(socket->ssl, rv);
	} while ((GTMTLS_WANT_READ == rv) || (GTMTLS_WANT_WRITE == rv));
	return rv;
}

_GTM_APIDEF int		gtm_tls_get_conn_info(gtm_tls_socket_t *socket, gtm_tls_conn_info *conn_info)
{
	long			verify_result, timeout, creation_time;
	unsigned int		session_id_length, ssl_version;
	const SSL_CIPHER	*cipher;
	const COMP_METHOD	*compression_method;
	char			*ssl_version_ptr, *session_id_ptr;
	X509			*peer;
	SSL			*ssl;
	EVP_PKEY		*pubkey;
	SSL_SESSION		*session;

	ssl = socket->ssl;
	if (NULL != (peer = SSL_get_peer_certificate(ssl)))
	{
		verify_result = SSL_get_verify_result(ssl);
		if (X509_V_OK == verify_result)
		{
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
				/* Older, but still commonly used, OpenSSL versions don't have macros for TLSv1.1 and TLSv1.2
				 * versions. They are hard coded to 0x0302 and 0x0303 respectively. So, use them as-is here.
				 */
				case 0x0302:
					ssl_version_ptr = "TLSv1.1";
					break;
				case 0x0303:
					ssl_version_ptr = "TLSv1.2";
					break;
				default:
					assert(FALSE && ssl_version);
					break;
			}
			strncpy(conn_info->protocol, ssl_version_ptr, MAX_ALGORITHM_LEN);
			/* SSL-Session Cipher Algorithm */
			cipher = SSL_get_current_cipher(ssl);
			SNPRINTF(conn_info->session_algo, SIZEOF(conn_info->session_algo), "%s", SSL_CIPHER_get_name(cipher));
			/* Remote Certificate Asymmetric Algorithm */
			pubkey = X509_get_pubkey(peer);
			SNPRINTF(conn_info->cert_algo, SIZEOF(conn_info->cert_algo), "%s", OBJ_nid2ln(pubkey->type));
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
			session_id_ptr = (char *)SSL_SESSION_get_id(session, &session_id_length);
			assert(session_id_length <= MAX_SESSION_ID_LEN / 2);
			GC_HEX(session_id_ptr, conn_info->session_id, session_id_length * 2);
			conn_info->session_id[session_id_length * 2] = '\0';
			/* Session expiry timeout. */
			if (0 >= (timeout = SSL_SESSION_get_timeout(session)))
				conn_info->session_expiry_timeout = -1;
			else
			{
				creation_time = SSL_SESSION_get_time(session);
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
			return 0;
		} else
		{
			UPDATE_ERROR_STRING("Peer certificate invalid");
			X509_free(peer);
			return -1;
		}
	} else
		UPDATE_ERROR_STRING("No certificate sent from the remote side");
	return -1;
}

_GTM_APIDEF int		gtm_tls_send(gtm_tls_socket_t *socket, char *buf, int send_len)
{
	int		rv;

	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	if (0 < (rv = SSL_write(socket->ssl, buf, send_len)))
	{
		assert(SSL_ERROR_NONE == SSL_get_error(socket->ssl, rv));
		return rv;
	}
	return ssl_error(socket->ssl, rv);
}

_GTM_APIDEF int		gtm_tls_recv(gtm_tls_socket_t * socket, char *buf, int recv_len)
{
	int		rv;

	DBG_VERIFY_SOCK_IS_BLOCKING(GET_SOCKFD(socket->ssl));
	if (0 < (rv = SSL_read(socket->ssl, buf, recv_len)))
	{
		assert(SSL_ERROR_NONE == SSL_get_error(socket->ssl, rv));
		return rv;
	}
	return ssl_error(socket->ssl, rv);
}

_GTM_APIDEF int		gtm_tls_cachedbytes(gtm_tls_socket_t *socket)
{
	return SSL_pending(socket->ssl);
}

_GTM_APIDEF void	gtm_tls_socket_close(gtm_tls_socket_t *socket)
{
	assert(socket);
	tls_errno = 0;
	if (NULL == socket->ssl)
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

_GTM_APIDEF void	gtm_tls_session_close(gtm_tls_socket_t **socket)
{
	SSL_SESSION		*session;
	gtm_tls_socket_t	*sock;

	sock = *socket;
	assert(sock);
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

_GTM_APIDEF void	gtm_tls_fini(gtm_tls_ctx_t **tls_ctx)
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
