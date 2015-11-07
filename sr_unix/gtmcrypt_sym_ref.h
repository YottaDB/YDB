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

#ifndef GTMCRYPT_SYM_REF_H
#define GTMCRYPT_SYM_REF_H

#ifdef USE_OPENSSL
# if defined(USE_AES256CFB)
#  define ALGO			EVP_aes_256_cfb128()
#  define UNIQ_ENC_PARAM_STRING	"AES256CFB"
# elif defined(USE_BLOWFISHCFB)
#  define ALGO			EVP_bf_cfb64()
#  define UNIQ_ENC_PARAM_STRING	"BLOWFISHCFB"
# else
#  error			"Unsupported Algorithm for OpenSSL"
# endif
#elif defined USE_GCRYPT
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
#else
# error 			"Unsupported encryption library. Reference implementation currently supports OpenSSL and Libgcrypt"
#endif	/* USE_GCRYPT */

#define UNIQ_ENC_PARAM_LEN	SIZEOF(UNIQ_ENC_PARAM_STRING) - 1
#define HASH_INPUT_BUFF_LEN	UNIQ_ENC_PARAM_LEN + SYMMETRIC_KEY_MAX

#define GC_SYM_DECRYPT(KEY_HANDLE, ENCRYPTED_BLOCK, UNENCRYPTED_BLOCK)								\
	gc_sym_encrypt_decrypt(KEY_HANDLE, ENCRYPTED_BLOCK, UNENCRYPTED_BLOCK, GC_DECRYPT)

#define GC_SYM_ENCRYPT(KEY_HANDLE, UNENCRYPTED_BLOCK, ENCRYPTED_BLOCK)								\
	gc_sym_encrypt_decrypt(KEY_HANDLE, UNENCRYPTED_BLOCK, ENCRYPTED_BLOCK, GC_ENCRYPT)

#ifndef USE_OPENSSL
int gc_sym_init(void);
#endif
int gc_sym_create_key_handles(gtm_dbkeys_tbl *entry);
int gc_sym_encrypt_decrypt(crypt_key_t *key, gtm_string_t *in_block, gtm_string_t *out_block, int flag);
#endif /* GTMCRYPT_SYM_REF_H */
