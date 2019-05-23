/****************************************************************
 *								*
 * Copyright (c) 2013-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include <gpgme.h>			/* gpgme functions */
#include <gpg-error.h>			/* gcry*_err_t */
#include <libconfig.h>

#include "gtmxc_types.h"

#include "gtmcrypt_util.h"
#include "ydbcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */

#include "gtmcrypt_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_sym_ref.h"
#include "gtmcrypt_pk_ref.h"

#ifndef USE_OPENSSL
/*
 * Initialize encryption state if libgcrypt is used.
 *
 * Returns:	0 if the initialization succeeded; -1 otherwise.
 */
int gc_sym_init(void)
{
	gcry_error_t rv;

	if (!gcry_check_version(GCRYPT_VERSION))
	{
		UPDATE_ERROR_STRING("libgcrypt version mismatch. %s or higher is required", GCRYPT_VERSION);
		return -1;
	}
	if (0 != (rv = gcry_control(GCRYCTL_DISABLE_SECMEM, 0)))
	{
		GC_APPEND_GCRY_ERROR(rv, "Failed to disable secure memory.");
		return -1;
	}
	if (0 != (rv = gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0)))
	{
		GC_APPEND_GCRY_ERROR(rv, "Failed to finish encryption initialization.")
		return -1;
	}
	return 0;
}
#endif

/*
 * Destroy the specified encryption / decryption state object.
 *
 * Arguments:	handle	Encryption / decryption state object to destroy.
 *
 * Returns:	N/A neither OpenSSL nor GCrypt destructors return a status.
 */
void gc_sym_destroy_cipher_handle(crypt_key_t handle)
{
	if (NULL != handle)
#ifdef USE_OPENSSL
		EVP_CIPHER_CTX_free(handle);
#elif defined(USE_GCRYPT)
		gcry_cipher_close(handle);
#else
	error Encryption library not defined, please use either -DUSE_OPENSSL or -DUSE_GCRYPT
#endif
}

/*
 * Create an encryption / decryption state object based on the specified key and IV and assign it to the passed pointer. If the
 * reuse flag is TRUE, then the passed cipher state is not recreated but reinitialized with the specified IV. Note that for a
 * successful reuse of the state object with OpenSSL the *same* raw key needs to be provided as during its creation.
 *
 * Arguments:	raw_key		Raw contents of the symmetric key to use.
 * 		iv		Initialization vector to use.
 * 		handle		Pointer to assign the newly created encryption / decryption state object to.
 * 		direction	Indicator of whether encryption or decryption state object is to be constructed.
 * 		reuse		Indicator of whether the state object should be reused or recreated.
 *
 * Returns:	0 if the state object was successfully constructed; -1 otherwise.
 */
int gc_sym_create_cipher_handle(unsigned char *raw_key, unsigned char *iv, crypt_key_t *handle, int direction, int reuse)
{
	int rv, plain_text_length;

#	ifdef USE_OPENSSL
	if (NULL == *handle)
		*handle = EVP_CIPHER_CTX_new();
	else if (!reuse)
	{
		EVP_CIPHER_CTX_init(*handle);
	}
	if (!EVP_CipherInit_ex(*handle, ALGO, NULL, raw_key, iv, direction))
	{
		GC_APPEND_OPENSSL_ERROR("Failed to initialize encryption key handle.");
		return -1;
	}
#	else
	if (!reuse)
	{
		if (0 != (rv = gcry_cipher_open(handle, ALGO, MODE, 0)))
		{
			GC_APPEND_GCRY_ERROR(rv, "Failed to initialize encryption key handle ('gcry_cipher_open').");
			return -1;
		}
		if (0 != (rv = gcry_cipher_setkey(*handle, raw_key, SYMMETRIC_KEY_MAX)))
		{
			GC_APPEND_GCRY_ERROR(rv, "Failed to initialize encryption key handle ('gcry_cipher_setkey').");
			return -1;
		}
	}
	gcry_cipher_setiv(*handle, iv, GTMCRYPT_IV_LEN);
#	endif
	return 0;
}

/*
 * Perform an encryption or decryption operation using the specified state object and buffers (or buffer, if in-place).
 *
 * Arguments:	key		Pointer to the encryption / decryption state object.
 * 		in_block	Block from which to take the input data for the operation.
 * 		in_block_len	Length of the block from which to take the input data for the operation; it should match the length
 * 				of the block for the output data, if not NULL.
 * 		out_block	Block where to place the output data from the operation.
 * 		flag		Indicator of whether encryption or decryption is to be performed.
 *
 * Returns:	0 if the operation went successfully; -1 otherwise.
 */
int gc_sym_encrypt_decrypt(crypt_key_t *key, unsigned char *in_block, int in_block_len, unsigned char *out_block, int flag)
{
	int rv, tmp_len, out_block_len;

	assert(in_block);
	assert(0 < in_block_len);
	if (NULL == out_block)
		out_block = in_block;
	out_block_len = in_block_len;
#	ifdef USE_GCRYPT
	if (out_block == in_block)
	{
		in_block = NULL;
		in_block_len = 0;
	}
	rv = (GTMCRYPT_OP_ENCRYPT == flag)
		? gcry_cipher_encrypt(*key, out_block, out_block_len, in_block, in_block_len)
		: gcry_cipher_decrypt(*key, out_block, out_block_len, in_block, in_block_len);
	if (0 != rv)
	{
		GC_APPEND_GCRY_ERROR(rv, "Libgcrypt function 'gcry_cipher_encrypt' or 'gcry_cipher_decrypt' failed.");
		return -1;
	}
#	endif
#	ifdef USE_OPENSSL
	if (!EVP_CipherUpdate(*key, out_block, &out_block_len, in_block, in_block_len))
	{
		GC_APPEND_OPENSSL_ERROR("OpenSSL function 'EVP_CipherUpdate' failed.")
		return -1;
	}
	if (!EVP_CipherFinal_ex(*key, out_block + out_block_len, &tmp_len))
	{
		GC_APPEND_OPENSSL_ERROR("OpenSSL function 'EVP_CipherFinal_ex' failed.")
		return -1;
	}
#	endif
	return 0;
}
