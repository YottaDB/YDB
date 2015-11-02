/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc 	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMCRYPT_SYM_REF_H
#define GTMCRYPT_SYM_REF_H
/* ==================================================================================== */
/*           Macros and functions for symmetric encryption tasks                        */
/* ==================================================================================== */

#ifdef USE_OPENSSL
#define GC_SYM_CREATE_HANDLES(cur_entry)							\
{												\
	int		ecode;									\
	unsigned char	*key = (unsigned char *)(cur_entry->key_string.address);		\
												\
	EVP_CIPHER_CTX_init(&(cur_entry->encr_key_handle));					\
	ecode = EVP_CipherInit(&(cur_entry->encr_key_handle), ALGO, key, NULL, GC_ENCRYPT);	\
	GC_SYM_ERROR(ecode, GC_FAILURE);							\
												\
	EVP_CIPHER_CTX_init(&(cur_entry->decr_key_handle));					\
	ecode = EVP_CipherInit(&(cur_entry->decr_key_handle), ALGO, key, NULL, GC_DECRYPT);	\
	GC_SYM_ERROR(ecode, GC_FAILURE);							\
}

#define GC_SYM_ERROR(err, return_value)					\
{									\
	if (!err)							\
	{								\
		ERR_error_string_n(err, err_string, ERR_STRLEN);	\
		return return_value;					\
	}								\
}
#else
#define GC_SYM_CREATE_HANDLES(cur_entry)						\
{											\
	gcry_error_t	err;								\
	char		*key = cur_entry->key_string.address;				\
	size_t		keylen = cur_entry->key_string.length;				\
											\
	GC_SYM_INIT;									\
	err = gcry_cipher_open(&(cur_entry->encr_key_handle), ALGO, MODE, FLAGS);	\
	if (!err)									\
		err = gcry_cipher_setkey(cur_entry->encr_key_handle, key, keylen);	\
	GC_SYM_ERROR(err, GC_FAILURE);							\
	err = gcry_cipher_open(&(cur_entry->decr_key_handle), ALGO, MODE, FLAGS);	\
	if (!err)									\
		err = gcry_cipher_setkey(cur_entry->decr_key_handle, key, keylen);	\
	GC_SYM_ERROR(err, GC_FAILURE);							\
}
#define GC_SYM_ERROR(err, return_value)						\
{										\
	if (GPG_ERR_NO_ERROR != err)						\
	{									\
		snprintf(err_string, ERR_STRLEN, "%s", gcry_strerror(err));	\
		return return_value;						\
	}									\
}
#endif

#ifdef	USE_GCRYPT
/* Initialization and error handling functions defined only for libgcrypt.
 * OpenSSL doesn't neeed them. */
#define GC_SYM_INIT									\
{											\
	gcry_error_t	err;								\
	char		*ver;								\
											\
	if (!gcry_already_inited)							\
	{										\
		memset(iv, 0, IV_LEN);							\
		if (!gcry_check_version(GCRYPT_VERSION))				\
		{									\
			snprintf(err_string, 						\
				ERR_STRLEN,						\
				"libgcrypt version mismatch. %s or higher is required",	\
				GCRYPT_VERSION);					\
			return GC_FAILURE;						\
		}									\
		if (!(err = gcry_control(GCRYCTL_DISABLE_SECMEM, 0)))			\
			if (!(err = gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0)))	\
				gcry_already_inited = TRUE;				\
		GC_SYM_ERROR(err, GC_FAILURE);						\
	}										\
}
#endif
#ifdef	USE_OPENSSL
#define GC_SYM_COMMON(key_handle, in_block, out_block, flag)				\
{											\
	int			block_len, is_inplace, ecode, tmp_len;			\
	int			out_len;						\
	char			*static_out_blk;					\
	unsigned char		*in = NULL, *out = NULL;				\
											\
	assert(in_block->address);							\
	assert(0 != in_block->length);							\
	in = (unsigned char *)in_block->address;					\
	block_len = in_block->length;							\
	out = (unsigned char *)out_block->address;					\
	if (NULL == out_block->address)							\
	{										\
		GC_GET_STATIC_BLOCK(static_out_blk, block_len);				\
		out = (unsigned char *)static_out_blk;					\
		is_inplace = TRUE;							\
	} else										\
		is_inplace = FALSE;							\
	ecode = EVP_CipherUpdate(&key_handle, out, &out_len, in, block_len);		\
	if (ecode)									\
		ecode = EVP_CipherFinal(&key_handle, out + out_len, &tmp_len);		\
	GC_SYM_ERROR(ecode, GC_FAILURE);						\
	if (is_inplace)									\
		memcpy(in, out, block_len); 						\
}
#else /* USE_GCRYPT */
#define GC_SYM_COMMON(key_handle, in_block, out_block, flag)					\
{												\
	int			is_inplace = 0;							\
	size_t			blen;								\
	gcry_error_t		err;								\
												\
	assert(in_block->address);								\
	assert(0 != in_block->length);								\
	blen = in_block->length;								\
	if (NULL == out_block->address)								\
		is_inplace = TRUE;								\
												\
	GC_SYM_INIT;										\
	gcry_cipher_setiv(key_handle, iv, IV_LEN);						\
	if (is_inplace)										\
	{											\
		if (flag == GC_ENCRYPT)								\
		{										\
			err = gcry_cipher_encrypt(key_handle, in_block->address, blen, NULL, 0);\
			GC_SYM_ERROR(err, GC_FAILURE);						\
		} else										\
		{										\
			err = gcry_cipher_decrypt(key_handle, in_block->address, blen, NULL, 0);\
			GC_SYM_ERROR(err, GC_FAILURE);						\
		}										\
	} else											\
	{											\
		if (flag == GC_ENCRYPT)								\
		{										\
			err = gcry_cipher_encrypt(key_handle,					\
						out_block->address, 				\
						blen,						\
						in_block->address,				\
						blen);						\
			GC_SYM_ERROR(err, GC_FAILURE);						\
		} else										\
		{										\
			err = gcry_cipher_decrypt(key_handle,					\
						out_block->address,				\
						blen,						\
						in_block->address,				\
						blen);						\
			GC_SYM_ERROR(err, GC_FAILURE);						\
		}										\
	}											\
}
#endif
#define GC_SYM_DECODE(key_handle, encrypted_block, unencrypted_block)			\
	GC_SYM_COMMON(key_handle, encrypted_block, unencrypted_block, GC_DECRYPT)

#define GC_SYM_ENCODE(key_handle, unencrypted_block, encrypted_block)			\
	GC_SYM_COMMON(key_handle, unencrypted_block, encrypted_block, GC_ENCRYPT)
#endif /* GTMCRYPT_SYM_REF_H */
