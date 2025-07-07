/****************************************************************
 *								*
 * Copyright (c) 2009-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define _FILE_OFFSET_BITS	64	/* Needed to compile gpgme client progs also with large file support */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>			/* BYPASSOK -- see above */
#include <sys/types.h>
#include <gpgme.h>			/* gpgme functions */
#include <gpg-error.h>			/* gcry*_err_t */
#include <libconfig.h>

#include <openssl/bio.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/decoder.h>
#endif
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

#include "gtmxc_types.h"		/* xc_string, xc_status_t and other callin interfaces xc_fileid */

#include "gtmcrypt_util.h"
#include "gtmcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */

#include "gtmcrypt_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_sym_ref.h"
#include "gtmcrypt_pk_ref.h"

GBLDEF	passwd_entry_t	*gtmcrypt_pwent;
GBLDEF	gpgme_ctx_t	pk_crypt_ctx;
GBLDEF	EVP_PKEY	*evp_pkey = NULL;

GBLREF	int		gtmcrypt_init_flags;
STATICDEF config_t	gtmcrypt_cfg;				/* Encryption configuration. */
STATICDEF char		path_array[GTM_PATH_MAX];		/* Array for temporary storage of keys or
								* DBs' real path information. */

/* This callback functions are called whenever OpenSSL wants to decrypt the private key using the OSSL_DECODER_* functions.
 * See https://docs.openssl.org/master/man3/OSSL_CALLBACK/ for more information. The callback function setup passes the
 * password entry to this callback function to provide OpenSSL with the private key password
 */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
STATICFNDEF int	decoder_passwd_callback(char *buf, size_t size, size_t *pass_len, const OSSL_PARAM params[], void *userdata)
{
	int			len;
	passwd_entry_t		*pwent;

	pwent = (passwd_entry_t *)userdata;
	assert(NULL != pwent);
	assert(NULL != pwent->passwd);
	if (NULL == pwent)
		return 0;	/* Cannot move forward */
	len = (int)strlen(pwent->passwd);
	strncpy(buf, pwent->passwd, size);
	if (len >= size)
	{
		buf[size] = '\0';
		len = size - 1;
	}
	*pass_len = (size_t)len;
	return len;
}
#endif

/* Function lifted from gtm_tls_impl.c */
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

/* Function reads encrypted database symmetric key
 *
 * key_path		- (input) database symmetric key file path
 * plain_text_length	- (in/out) decrypted symmetric key length
 *
 * Returns:		pointer to memory allocated (must be freed) holding encrypted symmetric key
 */
unsigned char *gc_pk_get_symmetric_key(const char *key_path, size_t *rawkey_length)
{
	int		errno, fd, save_errno, status, toread;
	unsigned char	*ptr, *buff;
	size_t		size;
	struct stat	stat_info;

	while (-1 == (fd = open(key_path, O_RDONLY)))
	{
		if (EINTR == errno)
			continue;
		UPDATE_ERROR_STRING("Symmetric key " STR_ARG " open:%s", ELLIPSIZE(key_path), strerror(errno));
		return NULL;
	}
	while (-1 == fstat(fd, &stat_info))
	{
		if (EINTR == errno)
			continue;
		UPDATE_ERROR_STRING("Symmetric key " STR_ARG " fstat:%s", ELLIPSIZE(key_path), strerror(errno));
		return NULL;
	}
	if (!(S_IFREG && stat_info.st_mode))
	{
		UPDATE_ERROR_STRING("Symmetric key " STR_ARG " not regular file", ELLIPSIZE(key_path));
		return NULL;
	}
	size = stat_info.st_size;
	buff = ptr = malloc(size);
	for (toread = (int)size; 0 < toread ; )
	{
		while ((-1 == (status = read(fd, ptr, toread))) && (EINTR == errno))
			;
		if (-1 == status)
		{
			save_errno = errno;
			UPDATE_ERROR_STRING("Failed to read key" STR_ARG ". %s",
					ELLIPSIZE(key_path), strerror(save_errno));
			return NULL;
		} else if (0 == status)
		{
			UPDATE_ERROR_STRING("Failed to read key" STR_ARG ". Encountered premature EOF while reading",
					ELLIPSIZE(key_path));
			free(buff);
			return NULL;
		}
		toread -= status;
		ptr += status;
	}
	//assert(size == (ptr - buff));
	*rawkey_length = size;
	close(fd);

	return buff;
}

/* Function that decrypts the private key encrypted database symmetric key using OpenSSL
 *
 * key_path		- (input) database symmetric key file path
 * plain_text		- (in/out) decrypted symmetric key
 * plain_text_length	- (in/out) decrypted symmetric key length (should be 32 bytes/SYMMETRIC_KEY_MAX)
 */
gpgme_error_t gc_pk_get_decrypted_key_pkcs(const char *key_path, unsigned char *plain_text, int *plain_text_length)
{
	FILE			*fp;
	EVP_PKEY_CTX		*ctx;
	unsigned char		*key_buff = NULL, *temp = NULL;
	size_t			key_buff_len = 0, len = 0, olen = 0;
	unsigned long 		oerr;
	char			oerrstr[2048];

	/* Acquire (encrypted) symmetric key in an allocated buffer */
	if (NULL == (key_buff = gc_pk_get_symmetric_key(key_path, &key_buff_len)))
		return -1;	/* Error message set in called function */
	/* Establish context to decrypt symmetric key. Below are two separate functions called back to back */
	if ((NULL == (ctx = EVP_PKEY_CTX_new(evp_pkey, NULL)))
			|| (0 >= EVP_PKEY_decrypt_init(ctx)))
	{
		oerr = ERR_get_error();
		ERR_error_string_n(oerr, oerrstr, sizeof(oerrstr));
		UPDATE_ERROR_STRING("Could not create decryption context for " STR_ARG " reason %s", ELLIPSIZE(key_path), oerrstr);
		free(key_buff);
		if (NULL != ctx)
			EVP_PKEY_CTX_free(ctx);
		return -1;
	}
	/* RSA default padding is "pkcs1" aka RSA_PKCS1_PADDING. No need for RSA_PKCS1_OAEP_PADDING since an oracle padding attack
	 * is not possible here. */
	/* Recommended decrypted buffer size check even though plaintext length has known SYMMETRIC_KEY_MAX bytes length.
	 * OpenSSL rounds up to the nearest modulus size, 512 */
	if (EVP_PKEY_decrypt(ctx, NULL, &olen, key_buff, key_buff_len) <= 0)
	{
		oerr = ERR_get_error();
		ERR_error_string_n(oerr, oerrstr, sizeof(oerrstr));
		UPDATE_ERROR_STRING("Failure to decrypt " STR_ARG " for reason %s", ELLIPSIZE(key_path), oerrstr);
		free(key_buff);
		EVP_PKEY_CTX_free(ctx);
		return -1;
	}
	len = olen;	/* GnuPG can refuse to produce the encrypted key which results in a zero length decryption. Catch that */
	temp = calloc(len, sizeof(char));
	if ((0 == olen) || (NULL == temp))
	{
		UPDATE_ERROR_STRING("Failure to allocate memory to decrypt " STR_ARG " for reason %s",
				ELLIPSIZE(key_path), strerror(errno));
		free(key_buff);
		EVP_PKEY_CTX_free(ctx);
		return -1;
	}
	/* Finally decrypt symmetric key */
	if (EVP_PKEY_decrypt(ctx, temp, &len, key_buff, key_buff_len) <= 0)
	{
		oerr = ERR_get_error();
		ERR_error_string_n(oerr, oerrstr, sizeof(oerrstr));
		UPDATE_ERROR_STRING("Failure to decrypt " STR_ARG " for reason %s", ELLIPSIZE(key_path), oerrstr);
		free(key_buff);
		EVP_PKEY_CTX_free(ctx);
		return -1;
	}
	if (SYMMETRIC_KEY_MAX != len)
	{	/* In testing GnuPG often failed to produce a decrypted key tripping this. Issue an error instead of assert */
		oerr = ERR_get_error();
		ERR_error_string_n(oerr, oerrstr, sizeof(oerrstr));
		UPDATE_ERROR_STRING("Failure to decrypt " STR_ARG " for reason %s", ELLIPSIZE(key_path), oerrstr);
		free(key_buff);
		EVP_PKEY_CTX_free(ctx);
		return -1;
	}
	memcpy((void *)plain_text, (void *)temp, (size_t)len);
	*plain_text_length = len;
	EVP_PKEY_CTX_free(ctx);
	free(key_buff);
	return 0;
}

int gc_pk_establish_pkcs_cfg(config_setting_t *parent, char *config_fn)
{
	int			name_length;
	const char		*cert_path, *key_format, *key_path, *key_type;
	int			cfg_enabled = 0, cfg_version = 1, envvar_len;
	FILE			*fp;
	struct stat		stat_info;
	unsigned long 		oerr;
	char			oerrstr[2048];
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	OSSL_DECODER_CTX	*dctx = NULL;
#else
#endif

	if ((CONFIG_TRUE != config_setting_lookup_string(parent, "format", &key_format))
			|| ((0 != strncmp("PEM", key_format, sizeof("PEM") - 1))
				&& (0 != strncmp("DER", key_format, sizeof("DER") - 1))))
		key_format = "PEM";	/* Default to PEM, allowing PEM and DER */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	/* OpenSSL 3+ allows RSA and EC keys */
	if ((CONFIG_TRUE != config_setting_lookup_string(parent, "type", &key_type))
			|| ((0 != strncmp("RSA", key_type, sizeof("RSA") - 1))
				&& (0 != strncmp("EC", key_type, sizeof("EC") - 1))))
		key_type = "RSA";	/* Default to RSA */
#else
		key_type = "RSA";	/* Pre-OpenSSL 3 forces RSA private key type */
#endif
	if (CONFIG_TRUE == config_setting_lookup_string(parent, "key", &key_path))
	{	/* Key path needs to be fully resolved before we can reliably use it, hence realpath-ing. */
		if (NULL == realpath(key_path, path_array))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", could not obtain the real path of 'plugins"
				".openssl-pkcs8.key' %s. %s", ELLIPSIZE(config_fn), key_path, strerror(errno));
			return -1;
		}
		if (0 != stat(key_path, &stat_info))
		{
			UPDATE_ERROR_STRING("Cannot stat key file: " STR_ARG ". %s", ELLIPSIZE(key_path),
				strerror(errno));
			return -1;
		}
		if (!S_ISREG(stat_info.st_mode))
		{
			UPDATE_ERROR_STRING("Key file " STR_ARG " is not a regular file", ELLIPSIZE(key_path));
			return -1;
		}
		fp = fopen(key_path, "r"); // Could have also done BIO* b = BIO_new_file(key_path, "r");
		if (NULL != fp)
		{	/* Open private key file */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
			/* Create the key appropriate decoder handling PEM/DER and RSA/EC. Requires OpenSSL 3+ */
			dctx = OSSL_DECODER_CTX_new_for_pkey(&evp_pkey, key_format, NULL, key_type,
					EVP_PKEY_KEYPAIR | EVP_PKEY_KEY_PARAMETERS, NULL, NULL);
			/* OSSL_KEYMGMT_SELECT_KEYPAIR - https://docs.openssl.org/master/man7/provider-keymgmt/#key-objects */
			if (dctx == NULL)
			{
				UPDATE_ERROR_STRING("Configuration file " STR_ARG " no decoders for %s %s",
						ELLIPSIZE(key_path), key_format, key_type);
				return -1;
			}
			/* Establish passphrase callback function */
			OSSL_DECODER_CTX_set_passphrase_cb(dctx, &decoder_passwd_callback, gtmcrypt_pwent);
			if (!OSSL_DECODER_from_fp(dctx, fp))
			{	/* decoding failure */
				oerr = ERR_get_error();
				ERR_error_string_n(oerr, oerrstr, sizeof(oerrstr));
				UPDATE_ERROR_STRING("Configuration file " STR_ARG " '%s' for %s %s %s", ELLIPSIZE(key_path),
						oerrstr, key_path, key_format, key_type);
				return -1;
			}
			OSSL_DECODER_CTX_free(dctx);
#else
			/* Read PEM/DER private key */
			if ('P' == key_format[0]) /* Cheap PEM ver DER check */
				evp_pkey = PEM_read_PrivateKey(fp, &evp_pkey, &passwd_callback, (void *)gtmcrypt_pwent);
			else
				evp_pkey = d2i_PKCS8PrivateKey_fp(fp, &evp_pkey, &passwd_callback, (void *)gtmcrypt_pwent);
#endif
			fclose(fp);
		} else
			evp_pkey = NULL;
		if (NULL == evp_pkey)
		{
			if (NULL == fp)
			{
				UPDATE_ERROR_STRING("Error opening file %s: %s.", key_path, strerror(errno));
			} else if (ERR_GET_REASON(ERR_peek_error()) == PEM_R_NO_START_LINE)
			{	/* give clearer error if only cert given but it doesn't have the key */
				UPDATE_ERROR_STRING("Private key missing from file %s.", key_path);
			} else
			{
				UPDATE_ERROR_STRING("Failed to read private key %s.", key_path);
			}
			return -1;
		}
		/* evp_pkey available for all decryption operations */
	}
	if (CONFIG_TRUE == config_setting_lookup_string(parent, "cert", &cert_path))
	{	/* Cert path needs to be fully resolved before we can reliably use it, hence realpath-ing. */
		if (NULL == realpath(cert_path, path_array))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", could not obtain the real path of 'plugins"
				".openssl-pkcs8.cert' %s. %s", ELLIPSIZE(config_fn), cert_path, strerror(errno));
			return -1;
		}
		if (0 != stat(cert_path, &stat_info))
		{
			UPDATE_ERROR_STRING("Cannot stat certificate file: " STR_ARG ". %s", ELLIPSIZE(cert_path),
				strerror(errno));
			return -1;
		}
		if (!S_ISREG(stat_info.st_mode))
		{
			UPDATE_ERROR_STRING("Certificate file " STR_ARG " is not a regular file", ELLIPSIZE(cert_path));
			return -1;
		}
	}
	return 0;
}

/* Public-private key cryptography using GPGME and GnuPG to manage encrypted database symmetric keys */
void gc_pk_scrub_passwd()
{
	/* Nullify the key strings, so that any generated cores will not contain the unencrypted keys */
	if (NULL != gtmcrypt_pwent)
		gc_freeup_pwent(gtmcrypt_pwent);
	/* Finally release the gpgme context */
	if (NULL != pk_crypt_ctx)
		gpgme_release(pk_crypt_ctx);
}

/* This function is called whenever gpg needs the passphrase with which the secret key is encrypted. In this case, the passphrase
 * is obtained from the ENVIRONMENT VARIABLE - $gtm_passwd or by invoking the mumps engine during the "gtmcrypt_init()".
 * In either ways, it's guaranteed that when this function is called, the passphrase is already set in the global variable.
 */
int gc_pk_crypt_passphrase_callback(void *opaque, const char *uid_hint, const char *passphrase_info, int last_was_bad, int fd)
{
	int 	write_ret, len;

	assert(0 != fd);
	assert(NULL != gtmcrypt_pwent->passwd);
	len = STRLEN(gtmcrypt_pwent->passwd);
	write_ret = write(fd, gtmcrypt_pwent->passwd, len);
	if (len == write_ret)
	{
		write_ret = write(fd, "\n", 1);
		if (1 == write_ret)
			return 0;
	}
	/* Problem with one of the writes so let gpgme know */
	return -1;
}

/* Given the structure that holds the plain data, this function reads through the structure and retrieves the plain text. We
 * also return the number of bytes actually read from the structure.
 */
int gc_pk_crypt_retrieve_plain_text(gpgme_data_t plain_data, unsigned char *plain_text)
{
	int	ret;

	assert(NULL != plain_text);

	/* Clear the temporary buffer */
	memset(plain_text, 0, SYMMETRIC_KEY_MAX);
	gpgme_data_seek(plain_data, 0, SEEK_SET);
	ret = (int)gpgme_data_read(plain_data, plain_text, SYMMETRIC_KEY_MAX);
	assert(ret || (NULL != getenv("gtm_white_box_test_case_enable"))); /* || needed for "encryption/key_file_enc" subtest */
	return ret;
}

/* This is currently necessary to work around what seems to be a gpgme issue in not clearing the plaintext keys
 * from the C stack (shows up in a core dump otherwise). When gpgme is fixed, this code can be removed.
 * The size of lclarray (8K) is determined purely from experimentation on all platforms.
 */
int gc_pk_scrub_plaintext_keys_from_c_stack()
{
	char lclarray[8192];

	memset(lclarray, 0, SIZEOF(lclarray));
	return 0;
}

/* This function tries to decrypt the cipher file (the file containing the symmetric key with which the database is encrypted).
 * It's assumed that the context is initialized and is set with the appropriate passphrase callback. The cipher_file
 * should contain the fully qualified path of the encrypted database key file. Also, plain text is supposed to be allocated with
 * sufficient space to hold the decrypted text.
 */
gpgme_error_t gc_pk_get_decrypted_key(const char *cipher_file, unsigned char *plain_text, int *plain_text_length)
{
	gpgme_error_t	err;
	gpgme_data_t	cipher_data = NULL, plain_data = NULL;
	gpg_err_code_t	ecode;
	char		null_buffer[SYMMETRIC_KEY_MAX];

	assert(NULL != cipher_file);
	assert(NULL != plain_text);
	assert(NULL != pk_crypt_ctx);
	assert(0 != STRLEN(cipher_file));

	/* Convert the cipher content in the cipher file into in-memory content.
	 * This in-memory content is stored in gpgme_data_t structure.
	 */
	err = gpgme_data_new_from_file(&cipher_data, cipher_file, 1);
	if (!err)
	{
		err = gpgme_data_new(&plain_data);
		if (!err)
		{	/* Try decrypting the cipher content with the context.
			 * The decrypted content will also be stored in gpgme_data_t structure.
			 */
			err = gpgme_op_decrypt(pk_crypt_ctx, cipher_data, plain_data);
			if (!err)	/* Once decrypted, the plain text has to be obtained from the plain_data structure. */
				*plain_text_length = gc_pk_crypt_retrieve_plain_text(plain_data, plain_text);
			gc_pk_scrub_plaintext_keys_from_c_stack();
		}
	}
	ecode = gpgme_err_code(err);
	if (0 != ecode)
	{
		switch (ecode)
		{
			case GPG_ERR_BAD_PASSPHRASE:
				UPDATE_ERROR_STRING("Incorrect password or error while obtaining password");
				break;
			case GPG_ERR_ENOENT:
				UPDATE_ERROR_STRING("Encryption key file " STR_ARG " not found", ELLIPSIZE(cipher_file));
				break;
			case GPG_ERR_EACCES:
				UPDATE_ERROR_STRING("Encryption key file " STR_ARG " not accessible", ELLIPSIZE(cipher_file));
				break;
			case GPG_ERR_ENAMETOOLONG:
				UPDATE_ERROR_STRING("Path, or a component of the path, to encryption key file " STR_ARG
					" is too long", ELLIPSIZE(cipher_file));
				break;
			default:
				UPDATE_ERROR_STRING("Error while accessing key file " STR_ARG ": %s", ELLIPSIZE(cipher_file),
					gpgme_strerror(err));
				break;
		}
	} else
	{
		assert((0 == plain_text_length) || (NULL != plain_data));
	}
	if (NULL != plain_data)
	{	/* Scrub plaintext data before releasing it */
		assert(SYMMETRIC_KEY_MAX == SIZEOF(null_buffer));
		memset(null_buffer, 0, SYMMETRIC_KEY_MAX);
		assert((0 != ecode) || (0 != plain_text_length) || memcmp(plain_text, null_buffer, SYMMETRIC_KEY_MAX));
		gpgme_data_write(plain_data, null_buffer, SYMMETRIC_KEY_MAX);
		gpgme_data_release(plain_data);
	}
	if (NULL != cipher_data)
		gpgme_data_release(cipher_data);
	return ecode;
}

int gc_pk_gpghome_has_permissions()
{
	char		pathname[GTM_PATH_MAX], *ptr;
	int		gnupghome_set, perms;
	size_t		pathlen;

	/* See if GNUPGHOME is set in the environment */
	if (!(ptr = getenv(GNUPGHOME)))
	{	/* $GNUPGHOME is not set, use $HOME/.gnupg as the GPG home directory */
		gnupghome_set = FALSE;
		if (!(ptr = getenv(HOME)))
		{
			UPDATE_ERROR_STRING(ENV_UNDEF_ERROR, HOME);
			return -1;
		}
		SNPRINTF(pathname, GTM_PATH_MAX, "%s/%s", ptr, DOT_GNUPG);
	} else
	{
		pathlen = strlen(ptr);
		if (0 == pathlen)
		{
			UPDATE_ERROR_STRING(ENV_EMPTY_ERROR, GNUPGHOME);
			return -1;
		}
		if (GTM_PATH_MAX <= pathlen)
		{
			UPDATE_ERROR_STRING("$GNUPGHOME is too long -" STR_ARG, ELLIPSIZE(pathname));
			return -1;
		}
		gnupghome_set = TRUE;
		memcpy(pathname, ptr, pathlen);
		pathname[pathlen] = '\0';
	}
	if (-1 != (perms = access(pathname, R_OK | X_OK)))
		return 0;
	else if (EACCES == errno)
	{
		if (gnupghome_set)
		{
			UPDATE_ERROR_STRING("No read permissions on $%s", GNUPGHOME);
		} else
			UPDATE_ERROR_STRING("No read permissions on $%s/%s", HOME, DOT_GNUPG);
	} else	/* Some other error */
		UPDATE_ERROR_STRING("Cannot stat on " STR_ARG " - %d", ELLIPSIZE(pathname), errno);
	return -1;
}
