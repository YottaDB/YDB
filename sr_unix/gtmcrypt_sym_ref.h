/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc 	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMCRYPT_SYM_REF_H
#define GTMCRYPT_SYM_REF_H

#ifdef USE_OPENSSL
# include <openssl/blowfish.h>
# include <openssl/sha.h>
# include <openssl/evp.h>
# include <openssl/err.h>
# if defined(USE_AES256CFB)
#  define ALGO			EVP_aes_256_cfb128()
#  define UNIQ_ENC_PARAM_STRING	"AES256CFB"
# elif defined(USE_BLOWFISHCFB)
#  define ALGO			EVP_bf_cfb64()
#  define UNIQ_ENC_PARAM_STRING	"BLOWFISHCFB"
# else
#  error			"Unsupported Algorithm for OpenSSL"
# endif
typedef EVP_CIPHER_CTX		crypt_key_t;
#elif defined USE_GCRYPT
# include <gcrypt.h>
# if defined(USE_AES256CFB)
#  define IV_LEN		16
#  define ALGO			GCRY_CIPHER_AES256
#  define MODE			GCRY_CIPHER_MODE_CFB
#  define UNIQ_ENC_PARAM_STRING	"AES256CFB"
#  define FLAGS			0
# else
#  error			"Unsupported Algorithm for Libgcrypt"
# endif
STATICDEF int			gcry_already_inited;
STATICDEF char			iv[IV_LEN];
typedef gcry_cipher_hd_t	crypt_key_t;
#else
# error 			"Unsupported encryption library. Reference implementation currently supports OpenSSL and Libgcrypt"
#endif	/* USE_GCRYPT */

#define UNIQ_ENC_PARAM_LEN	SIZEOF(UNIQ_ENC_PARAM_STRING) - 1
#define HASH_INPUT_BUFF_LEN	UNIQ_ENC_PARAM_LEN + SYMMETRIC_KEY_MAX

/* ==================================================================================== */
/*           Macros and functions for symmetric encryption tasks                        */
/* ==================================================================================== */

#ifdef USE_OPENSSL
#define GC_SYM_CREATE_HANDLES(cur_entry)											\
{																\
	int		ecode;													\
	unsigned char	*key = cur_entry->symmetric_key;									\
																\
	EVP_CIPHER_CTX_init(&(cur_entry->encr_key_handle));									\
	ecode = EVP_CipherInit(&(cur_entry->encr_key_handle), ALGO, key, NULL, GC_ENCRYPT);					\
	GC_SYM_ERROR(ecode, GC_FAILURE);											\
																\
	EVP_CIPHER_CTX_init(&(cur_entry->decr_key_handle));									\
	ecode = EVP_CipherInit(&(cur_entry->decr_key_handle), ALGO, key, NULL, GC_DECRYPT);					\
	GC_SYM_ERROR(ecode, GC_FAILURE);											\
}

#define GC_SYM_ERROR(err, return_value)												\
{																\
	if (!err)														\
	{															\
		ERR_error_string_n(err, gtmcrypt_err_string, MAX_GTMCRYPT_ERR_STRLEN);						\
		return return_value;												\
	}															\
}
#else
#define GC_SYM_CREATE_HANDLES(cur_entry)											\
{																\
	gcry_error_t	err;													\
	unsigned char	*key = cur_entry->symmetric_key;									\
																\
	GC_SYM_INIT;														\
	err = gcry_cipher_open(&(cur_entry->encr_key_handle), ALGO, MODE, FLAGS);						\
	if (!err)														\
		err = gcry_cipher_setkey(cur_entry->encr_key_handle, key, SYMMETRIC_KEY_MAX);					\
	GC_SYM_ERROR(err, GC_FAILURE);												\
	err = gcry_cipher_open(&(cur_entry->decr_key_handle), ALGO, MODE, FLAGS);						\
	if (!err)														\
		err = gcry_cipher_setkey(cur_entry->decr_key_handle, key, SYMMETRIC_KEY_MAX);					\
	GC_SYM_ERROR(err, GC_FAILURE);												\
}
#define GC_SYM_ERROR(err, return_value)												\
{																\
	if (GPG_ERR_NO_ERROR != err)												\
	{															\
		UPDATE_ERROR_STRING("%s", gcry_strerror(err));									\
		return return_value;												\
	}															\
}
#endif

#ifdef	USE_GCRYPT
/* Initialization and error handling functions defined only for libgcrypt.
 * OpenSSL doesn't neeed them. */
#define GC_SYM_INIT														\
{																\
	gcry_error_t	err;													\
	char		*ver;													\
																\
	if (!gcry_already_inited)												\
	{															\
		memset(iv, 0, IV_LEN);												\
		if (!gcry_check_version(GCRYPT_VERSION))									\
		{														\
			UPDATE_ERROR_STRING("libgcrypt version mismatch. %s or higher is required", GCRYPT_VERSION);		\
			return GC_FAILURE;											\
		}														\
		if (!(err = gcry_control(GCRYCTL_DISABLE_SECMEM, 0)))								\
			if (!(err = gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0)))						\
				gcry_already_inited = TRUE;									\
		GC_SYM_ERROR(err, GC_FAILURE);											\
	}															\
}
#endif
#ifdef	USE_OPENSSL
#define GC_SYM_COMMON(key_handle, in_block, out_block, flag)									\
{																\
	int			block_len, is_inplace, ecode, tmp_len;								\
	int			out_len;											\
	char			*static_out_blk;										\
	unsigned char		*in = NULL, *out = NULL;									\
																\
	assert(in_block->address);												\
	assert(0 != in_block->length);												\
	in = (unsigned char *)in_block->address;										\
	block_len = in_block->length;												\
	out = (unsigned char *)out_block->address;										\
	if (NULL == out_block->address)												\
	{															\
		GC_GET_STATIC_BLOCK(static_out_blk, block_len);									\
		out = (unsigned char *)static_out_blk;										\
		is_inplace = TRUE;												\
	} else															\
		is_inplace = FALSE;												\
	ecode = EVP_CipherUpdate(&key_handle, out, &out_len, in, block_len);							\
	if (ecode)														\
		ecode = EVP_CipherFinal(&key_handle, out + out_len, &tmp_len);							\
	GC_SYM_ERROR(ecode, GC_FAILURE);											\
	if (is_inplace)														\
		memcpy(in, out, block_len);											\
}
#else /* USE_GCRYPT */
#define GC_SYM_COMMON(key_handle, in_block, out_block, flag)									\
{																\
	size_t			inlen, outlen;											\
	char			*in, *out;											\
	gcry_error_t		err;												\
																\
	inlen = in_block->length;												\
	in = in_block->address;													\
	assert(in && inlen);													\
	if (NULL == out_block->address)												\
	{	/* IN-PLACE encryption/decryption. Adjust pointers accordingly */						\
		out = in;													\
		outlen = inlen;													\
		in = NULL;													\
		inlen = 0;													\
	} else															\
	{															\
		out = out_block->address;											\
		outlen = inlen;													\
	}															\
	GC_SYM_INIT;														\
	gcry_cipher_setiv(key_handle, iv, IV_LEN);										\
	if (GC_ENCRYPT == flag)													\
		err = gcry_cipher_encrypt(key_handle, out, outlen, in, inlen);							\
	else															\
		err = gcry_cipher_decrypt(key_handle, out, outlen, in, inlen);							\
	GC_SYM_ERROR(err, GC_FAILURE);												\
}
#endif
#define GC_SYM_DECRYPT(key_handle, encrypted_block, unencrypted_block)								\
	GC_SYM_COMMON(key_handle, encrypted_block, unencrypted_block, GC_DECRYPT)

#define GC_SYM_ENCRYPT(key_handle, unencrypted_block, encrypted_block)								\
	GC_SYM_COMMON(key_handle, unencrypted_block, encrypted_block, GC_ENCRYPT)
#endif /* GTMCRYPT_SYM_REF_H */
