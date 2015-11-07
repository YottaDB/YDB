/****************************************************************
 *								*
 *	Copyright 2009, 2013 Fidelity Information Services, Inc *
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
#include <sys/stat.h>
#include <dlfcn.h>
#include <assert.h>
#include <errno.h>

#include <gpgme.h>			/* gpgme functions */
#include <gpg-error.h>			/* gcry*_err_t */

#include <openssl/err.h>

#include "gtmxc_types.h"		/* gtm_string, gtm_status_t and other callin interfaces gtm_fileid */

#include "gtmcrypt_util.h"
#include "gtmcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */

#include "gtmcrypt_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_sym_ref.h"
#include "gtmcrypt_pk_ref.h"

#ifdef __MVS__
#define GTMSHR_IMAGENAME	"libgtmshr.dll"
#endif

#define MESSAGE1		"Verify encrypted key file and your GNUPGHOME settings"
#define MESSAGE2_DBKEYS		"Verify encryption key in configuration file pointed to by $gtm_dbkeys"
#define MESSAGE2_LIBCONFIG	"Verify encryption key in configuration file pointed to by $gtmcrypt_config"

GBLDEF	int		gtmcrypt_inited;
GBLDEF	int		gtmcrypt_init_flags;

GBLREF	int		gc_dbk_file_format;
GBLREF	passwd_entry_t	*gtmcrypt_pwent;

_GTM_APIDEF char* gtmcrypt_strerror()
{
	return gtmcrypt_err_string;
}

_GTM_APIDEF gtm_status_t gtmcrypt_init(int flags)
{
	int		fips_requested, fips_enabled, rv;

	if (gtmcrypt_inited)
		return GC_SUCCESS;
	if (0 != gc_load_gtmshr_symbols())
		return GC_FAILURE;
#	ifndef USE_OPENSSL
	gcry_set_log_handler(gtm_gcry_log_handler, NULL);
#	endif
	IS_FIPS_MODE_REQUESTED(fips_requested);
	if (fips_requested)
	{
#		ifndef USE_OPENSSL
#		ifndef GCRYPT_NO_FIPS
		if (0 != (rv = gcry_control(GCRYCTL_FORCE_FIPS_MODE)))
		{
			GC_APPEND_GCRY_ERROR(rv, "Failed to initialize FIPS mode.");
			return GC_FAILURE;
		}
#		endif
#		else
		ENABLE_FIPS_MODE(rv, fips_enabled);
		if (-1 == rv)
			return GC_FAILURE; /* Relevant error detail populated in the above macro. */
#		endif
	}
#	ifndef USE_OPENSSL
	if (0 != gc_sym_init())
		return GC_FAILURE;
#	endif
	GC_PK_INIT;
	/* Update $gtm_passwd for future invocation */
	if (0 != gc_update_passwd(GTM_PASSWD_ENV, &gtmcrypt_pwent, GTMCRYPT_DEFAULT_PASSWD_PROMPT,
					GTMCRYPT_OP_INTERACTIVE_MODE & flags))
	{
		return GC_FAILURE;
	}
	gtmcrypt_inited = TRUE;
	gtmcrypt_init_flags = flags;
	return GC_SUCCESS;
}

_GTM_APIDEF gtm_status_t gtmcrypt_getkey_by_name(gtm_string_t *filename, gtmcrypt_key_t *handle)
{
	gtm_fileid_ptr_t	fileid = NULL;
	gtm_dbkeys_tbl		*entry;
	gtm_status_t		status = 0;

	GC_VERIFY_INITED;
	*handle = GTMCRYPT_INVALID_KEY_HANDLE;
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	if (!GTM_FILENAME_TO_ID(filename, &fileid))
	{
		UPDATE_ERROR_STRING("Database file %s not found", filename->address);
		return GC_FAILURE;
	}
	if (NULL == (entry = gc_dbk_get_entry_by_fileid(fileid)))
	{	/* Try re-loading the configuration/db-keys file before giving up. */
		if (0 != gc_dbk_init_dbkeys_tbl())
			return GC_FAILURE;	/* No point continuing. */
		status = gc_dbk_fill_symkey_hash(fileid, NULL);
	}
	if (0 == status)
	{
		entry = gc_dbk_get_entry_by_fileid(fileid);
		if (NULL == entry)
		{
			UPDATE_ERROR_STRING("Database file %s missing in DB keys file or does not exist", filename->address);
			return GC_FAILURE;
		}
		*handle = entry->index;
	}
	return status;
}

_GTM_APIDEF gtm_status_t gtmcrypt_getkey_by_hash(gtm_string_t *hash, gtmcrypt_key_t *handle)
{
	gtm_dbkeys_tbl	*entry;
	gtm_status_t	status = 0;
	int		err_caused_by_gpg;
	char		save_err[MAX_GTMCRYPT_ERR_STRLEN], hex_buff[GTMCRYPT_HASH_HEX_LEN + 1];
	char		*alert_msg;

	*handle = GTMCRYPT_INVALID_KEY_HANDLE;
	GC_VERIFY_INITED;
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	if (NULL == (entry = gc_dbk_get_entry_by_hash(hash)))
	{	/* Try re-loading the configuration/db-keys file before giving up. */
		if (0 != gc_dbk_init_dbkeys_tbl())
			return GC_FAILURE;	/* No point continuing. */
		status = gc_dbk_fill_symkey_hash(NULL, hash->address);
	}
	if (0 == status)
	{
		entry = gc_dbk_get_entry_by_hash(hash);
		if (NULL == entry)
		{	/* Lookup still failed. Verify if we have right permissions on GNUPGHOME or $HOME/.gnupg
			 * (if GNUPGHOME is unset). If not, then the below function will store the appropriate
			 * error message in err_string and so return GC_FAILURE.
			 */
			if (GC_SUCCESS != gc_pk_gpghome_has_permissions())
				return GC_FAILURE;
			err_caused_by_gpg = ('\0' != gtmcrypt_err_string[0]);
			if (err_caused_by_gpg)
				alert_msg = MESSAGE1;
			else
			{
				assert((DBKEYS_FILE_FORMAT == gc_dbk_file_format) || (LIBCONFIG_FILE_FORMAT == gc_dbk_file_format));
				alert_msg = (DBKEYS_FILE_FORMAT == gc_dbk_file_format ? MESSAGE2_DBKEYS : MESSAGE2_LIBCONFIG);
			}
			GC_HEX(hash->address, hex_buff, GTMCRYPT_HASH_HEX_LEN);
			if (err_caused_by_gpg)
			{
				strcpy(save_err, gtmcrypt_err_string);
				UPDATE_ERROR_STRING("Expected hash - %s - %s. %s", hex_buff, save_err, alert_msg);
			} else
				UPDATE_ERROR_STRING("Expected hash - %s. %s", hex_buff, alert_msg);
			return GC_FAILURE;
		}
		*handle = entry->index;
	}
	return status;
}

_GTM_APIDEF gtm_status_t gtmcrypt_hash_gen(gtmcrypt_key_t handle, gtm_string_t *hash)
{
	gtm_dbkeys_tbl	*entry;

	GC_VERIFY_INITED;
	RETURN_IF_INVALID_HANDLE(handle);
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	entry = (gtm_dbkeys_tbl *)fast_lookup_entry[(int)handle];
	gc_dbk_get_hash(entry, hash);
	return GC_SUCCESS;
}

_GTM_APIDEF gtm_status_t gtmcrypt_encrypt(gtmcrypt_key_t handle, gtm_string_t *unencrypted_block, gtm_string_t *encrypted_block)
{
	gtm_dbkeys_tbl	*entry;
	crypt_key_t	key;

	GC_VERIFY_INITED;
	RETURN_IF_INVALID_HANDLE(handle);
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	entry = (gtm_dbkeys_tbl *)fast_lookup_entry[(int)handle];
	/* NOTE: The below assignment, while seemingly innocuous, is important. `entry->encr_key_handle' is a scalar type in
	 * OpenSSL (EVP_CIPHER_CTX). The below assignments does a "deep copy" of `entry->encr_key_handle' into `key'. This way,
	 * we *always* use a fresh copy of the key before encrypting. By doing so, we implicitly reset the encryption state
	 * engine.
	 */
	key = entry->encr_key_handle;
	GC_SYM_ENCRYPT(&key, unencrypted_block, encrypted_block);
	return GC_SUCCESS;
}

_GTM_APIDEF gtm_status_t gtmcrypt_decrypt(gtmcrypt_key_t handle, gtm_string_t *encrypted_block, gtm_string_t *unencrypted_block)
{
	gtm_dbkeys_tbl		*entry;
	crypt_key_t		key;

	GC_VERIFY_INITED;
	RETURN_IF_INVALID_HANDLE(handle);
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	entry = (gtm_dbkeys_tbl *)fast_lookup_entry[(int)handle];
	/* NOTE: The below assignment, while seemingly innocuous, is important. `entry->decr_key_handle' is a scalar type in
	 * OpenSSL (EVP_CIPHER_CTX). The below assignments does a "deep copy" of `entry->decr_key_handle' into `key'. This way,
	 * we *always* use a fresh copy of the key before decrypting. By doing so, we implicitly reset the encryption state
	 * engine.
	 */
	key = entry->decr_key_handle;
	GC_SYM_DECRYPT(&key, encrypted_block, unencrypted_block);
	return GC_SUCCESS;
}

_GTM_APIDEF gtm_status_t gtmcrypt_close()
{
	GC_VERIFY_INITED;
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	gc_pk_scrub_passwd();
	gc_dbk_scrub_entries();
	gtmcrypt_inited = FALSE;
	return GC_SUCCESS;
}
