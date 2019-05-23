/****************************************************************
 *								*
 * Copyright (c) 2009-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include <libconfig.h>

#include "gtmxc_types.h"		/* gtm_string, gtm_status_t and other callin interfaces gtm_fileid */

#include "gtmcrypt_util.h"
#include "ydbcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */

#include "gtmcrypt_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_sym_ref.h"
#include "gtmcrypt_pk_ref.h"
#include "ydb_getenv.h"

#define CHECK_IV_LENGTH(IV)									\
{												\
	if (IV.length > GTMCRYPT_IV_LEN)							\
	{											\
		UPDATE_ERROR_STRING("Specified IVEC has length %ld, which is greater than "	\
			"the maximum allowed IVEC length %d", iv.length, GTMCRYPT_IV_LEN);	\
		return -1;									\
	}											\
}

#ifdef GTM_CRYPT_KEYS_LOG
#  define PRINT_KEYSTORE_STATE(MESSAGE, ...)							\
{												\
	fprintf(stderr, "----------------------------------\n" MESSAGE				\
			"----------------------------------\n", __VA_ARGS__);			\
	print_debug();										\
}
#else
#  define PRINT_KEYSTORE_STATE(MESSAGE, ...)
#endif

GBLDEF	int			gtmcrypt_inited;
GBLDEF	int			gtmcrypt_init_flags;

GBLREF	passwd_entry_t		*gtmcrypt_pwent;

/*
 * Initialize encryption if not yet initialized.
 *
 * Arguments:	flags	Encryption flags to use.
 *
 * Returns:	0 if encryption was initialized successfully; -1 otherwise.
 */
gtm_status_t gtmcrypt_init(gtm_int_t flags)
{
	int fips_requested, fips_enabled, rv;

	if (gtmcrypt_inited)
		return 0;
	if (0 != gc_load_yottadb_symbols())
		return -1;
#	ifdef USE_GCRYPT
	gcry_set_log_handler(gtm_gcry_log_handler, NULL);
#	endif
	IS_FIPS_MODE_REQUESTED(fips_requested);
	if (fips_requested)
	{
#		ifdef USE_GCRYPT
#		ifndef GCRYPT_NO_FIPS
		if (0 != (rv = gcry_control(GCRYCTL_FORCE_FIPS_MODE)))
		{
			GC_APPEND_GCRY_ERROR(rv, "Failed to initialize FIPS mode.");
			return -1;
		}
#		endif
#		else
		ENABLE_FIPS_MODE(rv, fips_enabled);
		/* Relevant error detail populated in the above macro. */
		if (-1 == rv)
			return -1;
#		endif
	}
#	ifdef USE_GCRYPT
	if (0 != gc_sym_init())
		return -1;
#	endif
	GC_PK_INIT;
	/* Update $ydb_passwd/$gtm_passwd for future invocation */
	if (0 != gc_update_passwd(YDBENVINDX_PASSWD, NULL_SUFFIX, &gtmcrypt_pwent, GTMCRYPT_DEFAULT_PASSWD_PROMPT,
					GTMCRYPT_OP_INTERACTIVE_MODE & flags))
	{
		return -1;
	}
	gtmcrypt_inited = TRUE;
	gtmcrypt_init_flags = flags;
	gtmcrypt_err_string[0] = '\0';
	if (0 != gc_pk_gpghome_has_permissions())
		return -1;
	return 0;
}

/*
 * Return the error string.
 *
 * Returns:	The error string constructed so far.
 */
char* gtmcrypt_strerror(void)
{
	return gtmcrypt_err_string;
}

/*
 * Find the key by hash and database path and set up database encryption and decryption state objects, if not created yet.
 *
 * Arguments:	handle		Pointer to the database encryption state object supplied by the caller and filled in by this
 * 				routine.
 * 		key_hash	Hash of the key.
 * 		db_path		Path to the database file that should be associated with the sought key. Can be an empty string.
 * 		iv		Initialization vector to use for encryption or decryption.
 *
 * Returns:	0 if the key was found and database encryption and decryption state objects were initialized or existed already; -1
 * 		otherwise.
 */
gtm_status_t gtmcrypt_init_db_cipher_context_by_hash(gtmcrypt_key_t *handle, gtm_string_t key_hash,
				gtm_string_t db_path, gtm_string_t iv)
{
	gtm_keystore_t		*entry;
	char			filename[YDB_PATH_MAX], real_db_path[YDB_PATH_MAX];
	char			*db_path_ptr;
	gtm_cipher_ctx_t	**ctx;

	GC_VERIFY_INITED;
	/* Discard any previously recorded error messages. */
	gtmcrypt_err_string[0] = '\0';
	assert((YDB_PATH_MAX > db_path.length) && (0 <= db_path.length));
	CHECK_IV_LENGTH(iv);
	if (key_hash.length != GTMCRYPT_HASH_LEN)
	{
		UPDATE_ERROR_STRING("Specified symmetric key hash has length %ld, which is different from "
			"the expected hash length %d", key_hash.length, GTMCRYPT_HASH_LEN);
		return -1;
	}
	if (0 < db_path.length)
	{
		memcpy(filename, db_path.address, db_path.length);
		filename[db_path.length] = '\0';
		/* Database path needs to be fully resolved before we can reliably use it, hence realpath-ing. */
		if (NULL == realpath(filename, real_db_path))
		{
			UPDATE_ERROR_STRING("Could not obtain the real path of the database " STR_ARG ". %s",
				ELLIPSIZE(filename), strerror(errno));
			return -1;
		}
		db_path_ptr = real_db_path;
	} else
		db_path_ptr = NULL;
	if (0 != gtmcrypt_getkey_by_hash((unsigned char *)key_hash.address, db_path_ptr, &entry))
		return -1;
	assert(NULL != entry);
	if (NULL == entry->db_cipher_entry)
	{	/* This cipher context is for decryption; iv is a static global. */
		if (0 != keystore_new_cipher_ctx(entry, iv.address, iv.length, GTMCRYPT_OP_DECRYPT))
			return -1;
		/* And this cipher context (inserted ahead of the first one) is for encryption. */
		if (0 != keystore_new_cipher_ctx(entry, iv.address, iv.length, GTMCRYPT_OP_ENCRYPT))
			return -1;
		entry->db_cipher_entry = entry->cipher_head;
	} else
		assert(NULL != entry->cipher_head);
	ctx = (gtm_cipher_ctx_t **)handle;
	*ctx = entry->db_cipher_entry;
	assert(NULL != (*ctx)->next);
	PRINT_KEYSTORE_STATE("Returning from gtmcrypt_init_db_cipher_context_by_hash(db_path=%s):\n (pointing to %s)\n",
			filename, entry->key_path);
	return 0;
}

/*
 * Find the key by its name and set up device encryption or decryption state object.
 *
 * Arguments:	handle		Pointer which should get pointed to the device encryption or decryption state object.
 * 		key_name	Name of the key.
 * 		iv		Initialization vector to use for encryption or decryption.
 * 		operation	Flag indicating whether encryption or decryption is desired; use GTMCRYPT_OP_ENCRYPT or
 * 				GTMCRYPT_OP_DECRYPT, respectively.
 *
 * Returns:	0 if the key was found and device encryption or decryption state object was initialized; -1 otherwise.
 */
gtm_status_t gtmcrypt_init_device_cipher_context_by_keyname(gtmcrypt_key_t *handle, gtm_string_t key_name,
									 gtm_string_t iv, gtm_int_t operation)
{
	gtm_keystore_t		*entry;
	char			keyname[YDB_PATH_MAX];
	gtm_cipher_ctx_t	**ctx;

	GC_VERIFY_INITED;
	/* Discard any previously recorded error messages. */
	gtmcrypt_err_string[0] = '\0';
	CHECK_IV_LENGTH(iv);
	/* NULL-terminating to ensure correct lookups. */
	memcpy(keyname, key_name.address, key_name.length);
	memset(keyname + key_name.length, 0, YDB_PATH_MAX - key_name.length);
	if (0 != gtmcrypt_getkey_by_keyname(keyname, NULL, &entry, FALSE))
		return -1;
	assert(NULL != entry);
	if (0 != keystore_new_cipher_ctx(entry, iv.address, iv.length, operation))
		return -1;
	ctx = (gtm_cipher_ctx_t **)handle;
	*ctx = entry->cipher_head;
	PRINT_KEYSTORE_STATE("Returning from gtmcrypt_init_device_cipher_context_by_keyname(key_name=%s):\n (pointing to %s)\n",
			keyname, entry->key_path);
	return 0;
}

/*
 * Find the key by the path of the database it corresponds to as well as its own path, and obtain its hash.
 *
 * Arguments:	db_path		Path to the database file that should be associated with the sought key.
 * 		key_path	Path to the key. Can be an empty string.
 * 		hash_dest	Pointer to the location for this routine to copy the key's hash.
 *
 * Returns:	0 if the key was found and key's hash was copied to the specified location; -1 otherwise.
 */
gtm_status_t gtmcrypt_obtain_db_key_hash_by_keyname(gtm_string_t db_path, gtm_string_t key_path, gtm_string_t *hash_dest)
{
	gtm_keystore_t	*entry;
	char		filename[YDB_PATH_MAX], real_db_path[YDB_PATH_MAX], real_key_path[YDB_PATH_MAX];
	char		*key_path_ptr;

	GC_VERIFY_INITED;
	/* Discard any previously recorded error messages. */
	gtmcrypt_err_string[0] = '\0';
	assert((YDB_PATH_MAX > db_path.length) && (0 < db_path.length));
	assert((YDB_PATH_MAX > key_path.length) && (0 <= key_path.length));
	memcpy(filename, db_path.address, db_path.length);
	filename[db_path.length] = '\0';
	/* Database path needs to be fully resolved before we can reliably use it, hence realpath-ing. */
	if (NULL == realpath(filename, real_db_path))
	{
		UPDATE_ERROR_STRING("Could not obtain the real path of the database " STR_ARG ". %s",
				ELLIPSIZE(filename), strerror(errno));
		return -1;
	}
	if (0 < key_path.length)
	{
		memcpy(filename, key_path.address, key_path.length);
		filename[key_path.length] = '\0';
		/* Key path also needs to be fully resolved before we can reliably use it, hence realpath-ing. */
		if (NULL == realpath(filename, real_key_path))
		{
			UPDATE_ERROR_STRING("Could not obtain the real path of the key " STR_ARG ". %s",
					ELLIPSIZE(filename), strerror(errno));
			return -1;
		}
		key_path_ptr = real_key_path;
	} else
		key_path_ptr = NULL;
	if (0 != gtmcrypt_getkey_by_keyname(real_db_path, key_path_ptr, &entry, TRUE))
		return -1;
	assert(NULL != entry);
	hash_dest->length = GTMCRYPT_HASH_LEN;
	hash_dest->address = (char *)entry->key_hash;
	PRINT_KEYSTORE_STATE("Returning from gtmcrypt_obtain_db_key_hash_by_keyname(db_path=%s, key_path=%s):\n (pointing to %s)\n",
			real_db_path, (key_path_ptr ? key_path_ptr : ""), entry->key_path);
	return 0;
}

/*
 * Release the specified encryption or decryption state object, also releasing the decryption state if database encryption state is
 * specified.
 *
 * Arguments:	handle	Encryption or decryption state object to release.
 *
 * Returns:	0 if the operation was successful; -1 otherwise.
 */
gtm_status_t gtmcrypt_release_cipher_context(gtmcrypt_key_t handle)
{
	gtm_cipher_ctx_t *ctx;

	GC_VERIFY_INITED;
	assert(GTMCRYPT_INVALID_KEY_HANDLE != handle);
	/* Discard any previously recorded error messages. */
	gtmcrypt_err_string[0] = '\0';
	ctx = (gtm_cipher_ctx_t *)handle;
	/* In case a database encryption state object is specified, we want to make sure that the respective database decryption
	 * state object is also removed.
	 */
	if (ctx->store->db_cipher_entry == ctx)
	{
		assert(NULL != ctx->next);
		if (-1 == keystore_remove_cipher_ctx(ctx->next))
			return -1;
	}
	if (-1 == keystore_remove_cipher_ctx(ctx))
		return -1;
	return 0;
}

/*
 * Perform encryption or decryption of the provided data based on the specified encryption / decryption state. If the target buffer
 * pointer is NULL, the operation is done in-place. It is also possible to set the initialization vector (IV) to a particular value,
 * or reset it to the original value, before attempting the operation. Note that the changes are persistent.
 *
 * Arguments:	handle			Encryption state object to use.
 * 		unencr_block		Block where unencrypted data is read from.
 * 		unencr_block_len	Length of the unencrypted (and encrypted) data block.
 * 		encr_block		Block where encrypted data is put into.
 * 		operation		Flag indicating whether encryption or decryption is desired; use GTMCRYPT_OP_ENCRYPT or
 * 					GTMCRYPT_OP_DECRYPT, respectively.
 * 		iv_mode			Flag indicating whether the initialization vector (IV) should be changed prior to the
 * 					operation; use GTMCRYPT_IV_CONTINUE to proceed without changing the IV, GTMCRYPT_IV_SET to
 * 					set the IV the value supplied in the iv argument, and GTMCRYPT_IV_RESET to reset the IV to
 * 					the value specified at initialization.
 * 		iv			Initialization vector to set the encryption state to when iv_mode is GTMCRYPT_IV_SET.
 *
 * Returns:	0 if the operation succeeded; -1 otherwise.
 */
gtm_status_t gtmcrypt_encrypt_decrypt(gtmcrypt_key_t handle, gtm_char_t *src_block, gtm_int_t src_block_len,
					  gtm_char_t *dest_block, gtm_int_t operation, gtm_int_t iv_mode, gtm_string_t iv)
{
	gtm_cipher_ctx_t	*ctx;
	unsigned char		iv_array[GTMCRYPT_IV_LEN];

	GC_VERIFY_INITED;
	assert(GTMCRYPT_INVALID_KEY_HANDLE != handle);
	/* Discard any previously recorded error messages. */
	gtmcrypt_err_string[0] = '\0';
	ctx = (gtm_cipher_ctx_t *)handle;
	assert(NULL != ctx);
	switch (iv_mode)
	{
		case GTMCRYPT_IV_SET:
			CHECK_IV_LENGTH(iv);
			memcpy(iv_array, iv.address, iv.length);
			if (GTMCRYPT_IV_LEN != iv.length)
				memset(iv_array + iv.length, 0, GTMCRYPT_IV_LEN - iv.length);
			/* CAUTION: Fall-through. */
		case GTMCRYPT_IV_RESET:
			if (GTMCRYPT_OP_DECRYPT == operation)
			{	/* We expect the IV to be set on a particular operation only for databases, which is why we obtain
				 * the correct crypt_key_t object in case of decryption.
				 */
				ctx = ctx->next;
				assert(NULL != ctx);
			}
			if (gc_sym_create_cipher_handle(ctx->store->key, (GTMCRYPT_IV_SET == iv_mode) ? iv_array : ctx->iv,
						&ctx->handle, operation, TRUE))
				return -1;
			break;
		case GTMCRYPT_IV_CONTINUE:
			/* For devices, encryption and decryption state should be maintained right from the first byte, and for this
			 * purpose the IV should not be touched.
			 */
			break;
		default:
			assert(FALSE);
	}
	if (0 != gc_sym_encrypt_decrypt(&ctx->handle, (unsigned char *)src_block, src_block_len,
				(unsigned char *)dest_block, operation))
		return -1;
	return 0;
}

/*
 * Compare the keys associated with two encryption or decryption state objects.
 *
 * Arguments:	handle1		First ecryption or decryption state object to use.
 * 		handle2		Second ecryption or decryption state object to use.
 *
 * Returns:	1 if both encryption or decryption state objects use the same key; 0 otherwise.
 */
gtm_int_t gtmcrypt_same_key(gtmcrypt_key_t handle1, gtmcrypt_key_t handle2)
{
	gtm_cipher_ctx_t *ctx1, *ctx2;

	assert((GTMCRYPT_INVALID_KEY_HANDLE != handle1) && (GTMCRYPT_INVALID_KEY_HANDLE != handle2));
	ctx1 = (gtm_cipher_ctx_t *)handle1;
	ctx2 = (gtm_cipher_ctx_t *)handle2;
	if (ctx1 == NULL)
		return (ctx2 == NULL);
	if (ctx2 == NULL)
		return 0;
	return (ctx1->store == ctx2->store);
}

/*
 * Disable encryption and discard any sensitive data in memory.
 *
 * Returns:	0 if the operation succeeded; -1 otherwise.
 */
gtm_status_t gtmcrypt_close(void)
{
	int status;

	GC_VERIFY_INITED;
	/* Discard any previously recorded error messages. */
	gtmcrypt_err_string[0] = '\0';
	gc_pk_scrub_passwd();
	status = gtm_keystore_cleanup_all();
	gtmcrypt_inited = FALSE;
	return status;
}
