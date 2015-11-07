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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include "gtmxc_types.h"

#include "gtmcrypt_util.h"
#include "gtmcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */

#include "gtmcrypt_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_sym_ref.h"

#ifndef USE_OPENSSL
int gc_sym_init()
{
	gcry_error_t	rv;

	memset(iv, 0, IV_LEN);
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

int gc_sym_create_key_handles(gtm_dbkeys_tbl *entry)
{
	int		rv;
	unsigned char	*key;

	key = entry->symmetric_key;
#	ifdef USE_OPENSSL
	/* Create the encryption key handle. */
	EVP_CIPHER_CTX_init(&entry->encr_key_handle);
	if (!EVP_CipherInit(&entry->encr_key_handle, ALGO, key, NULL, GC_ENCRYPT))
	{
		GC_APPEND_OPENSSL_ERROR("Failed to initialize encryption key handle.");
		return -1;
	}
	/* Create the decryption key handle. */
	EVP_CIPHER_CTX_init(&entry->decr_key_handle);
	if (0 == (rv = EVP_CipherInit(&entry->decr_key_handle, ALGO, key, NULL, GC_DECRYPT)))
	{
		GC_APPEND_OPENSSL_ERROR("Failed to initialize decryption key handle.");
		return -1;
	}
#	else
	if (0 != (rv = gcry_cipher_open(&entry->encr_key_handle, ALGO, MODE, 0)))
	{
		GC_APPEND_GCRY_ERROR(rv, "Failed to initialize encryption key handle (`gcry_cipher_open').");
		return -1;
	}
	if (0 != (rv = gcry_cipher_setkey(entry->encr_key_handle, key, SYMMETRIC_KEY_MAX)))
	{
		GC_APPEND_GCRY_ERROR(rv, "Failed to initialize encryption key handle (`gcry_cipher_setkey').");
		return -1;
	}
	if (0 != (rv = gcry_cipher_open(&entry->decr_key_handle, ALGO, MODE, 0)))
	{
		GC_APPEND_GCRY_ERROR(rv, "Failed to initialize decryption key handle (`gcry_cipher_open').");
		return -1;
	}
	if (0 != (rv = gcry_cipher_setkey(entry->decr_key_handle, key, SYMMETRIC_KEY_MAX)))
	{
		GC_APPEND_GCRY_ERROR(rv, "Failed to initialize decryption key handle (`gcry_cipher_setkey').");
		return -1;
	}
#	endif
	return 0;
}

int gc_sym_encrypt_decrypt(crypt_key_t *key, gtm_string_t *in_block, gtm_string_t *out_block, int flag)
{
	int		inl, outl, rv, tmp_len;
	unsigned char	*in, *out;

	assert(in_block->address);
	assert(0 < in_block->length);
	in = (unsigned char *)in_block->address;
	out = (NULL == out_block->address) ? in : (unsigned char *)out_block->address;
	outl = inl = in_block->length;
#	ifndef USE_OPENSSL
	if (out == in)
	{
		in = NULL;
		inl = 0;
	}
	/* This is important. We have to reset the IV back to all zeros to reset the encryption state machine. Otherwise, the 'iv'
	 * from the previous call to this function would be reused for this call resulting in incorrect encryption/decryption.
	 */
	gcry_cipher_setiv(*key, iv, IV_LEN);
	rv = (GC_ENCRYPT == flag) ? gcry_cipher_encrypt(*key, out, outl, in, inl) : gcry_cipher_decrypt(*key, out, outl, in, inl);
	if (0 != rv)
	{
		GC_APPEND_GCRY_ERROR(rv, "Libgcrypt function `gcry_cipher_encrypt' or `gcry_cipher_decrypt' failed.");
		return -1;
	}
#	else
	if (!EVP_CipherUpdate(key, out, &outl, in, inl))
	{
		GC_APPEND_OPENSSL_ERROR("OpenSSL function `EVP_CipherUpdate' failed.")
		return -1;
	}
	if (!EVP_CipherFinal(key, out + outl, &tmp_len))
	{
		GC_APPEND_OPENSSL_ERROR("OpenSSL function `EVP_CipherFinal' failed.")
		return -1;
	}
#	endif
	return 0;
}
