/****************************************************************
 *								*
 * Copyright (c) 2009-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
# if defined(USE_AES256CFB)
#  define ALGO			EVP_aes_256_cfb128()
#  define UNIQ_ENC_PARAM_STRING	"AES256CFB"
# else
#  error			"Unsupported Algorithm for OpenSSL"
# endif
#elif defined USE_GCRYPT
# if defined(USE_AES256CFB)
#  define ALGO			GCRY_CIPHER_AES256
#  define MODE			GCRY_CIPHER_MODE_CFB
#  define UNIQ_ENC_PARAM_STRING	"AES256CFB"
#  define FLAGS			0
# else
#  error			"Unsupported Algorithm for Libgcrypt"
# endif
#else
# error 			"Unsupported encryption library. Reference implementation currently supports OpenSSL and Libgcrypt"
#endif	/* USE_GCRYPT */

#define UNIQ_ENC_PARAM_LEN	SIZEOF(UNIQ_ENC_PARAM_STRING) - 1
#define HASH_INPUT_BUFF_LEN	UNIQ_ENC_PARAM_LEN + SYMMETRIC_KEY_MAX

#ifndef USE_OPENSSL
int gc_sym_init(void);
#endif
int gc_sym_destroy_key_handles(gtm_keystore_t *entry);
int gc_sym_create_cipher_handle(unsigned char *raw_key, unsigned char *iv, crypt_key_t *handle, int direction, int reuse);
void gc_sym_destroy_cipher_handle(crypt_key_t handle);
int gc_sym_encrypt_decrypt(crypt_key_t *key, unsigned char *in_block, int in_block_len, unsigned char *out_block, int flag);
#endif /* GTMCRYPT_SYM_REF_H */
