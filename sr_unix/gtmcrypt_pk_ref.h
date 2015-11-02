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

#ifndef	GTMCRYPT_PK_REF_H
#define	GTMCRYPT_PK_REF_H

int 			gc_pk_mask_unmask_passwd(char *in, char *out, int len);
int 			gc_pk_mask_unmask_passwd_interlude(int nparm, gtm_string_t *in, gtm_string_t *out, int len);
void 			gc_pk_scrub_passwd();
void 			gc_pk_crypt_load_gtmci_env();
xc_status_t 		gc_pk_crypt_prompt_passwd_if_needed(int prompt_passwd);
int 			gc_pk_crypt_passphrase_callback(void *opaque,
							const char *uid_hint,
							const char *passphrase_info,
							int last_was_bad,
							int fd);
int 			gc_pk_crypt_retrieve_plain_text(gpgme_data_t plain_data, unsigned char *plain_text);
gpgme_error_t 		gc_pk_get_decrypted_key(const char *cipher_file, unsigned char *plain_text, int *plain_text_length);
int			gc_pk_mask_unmask_passwd(char *in, char *out, int len);
void			gc_pk_scrub_passwd(void);
void			gc_pk_crypt_load_gtmci_env(void);
int			gc_pk_scrub_plaintext_keys_from_c_stack(void);
int			gc_pk_gpghome_has_permissions(void);

/* Public key cryptography related macros */
#define GC_PK_INIT													\
{															\
	gpgme_error_t			err;										\
	GBLREF	gpgme_ctx_t		pk_crypt_ctx;									\
															\
	gpgme_check_version(NULL); /* This initializes the gpgme engine. */						\
	err = gpgme_new(&pk_crypt_ctx);											\
	if (!err)													\
	{														\
		err = gpgme_set_protocol(pk_crypt_ctx, GPGME_PROTOCOL_OpenPGP);						\
		if (!err)												\
		{													\
			gpgme_set_passphrase_cb(pk_crypt_ctx,								\
						(gpgme_passphrase_cb_t) gc_pk_crypt_passphrase_callback,		\
						NULL);									\
		}													\
	}														\
	if (err)													\
	{														\
		pk_crypt_ctx = NULL;											\
		UPDATE_ERROR_STRING("Error initializing GpgME: %s/%s", gpgme_strsource(err), gpgme_strerror(err));	\
		return GC_FAILURE;											\
	}														\
}

#define GC_PK_PROMPT_PASSWD(prompt_passwd)										\
{															\
	if (0 != gc_pk_crypt_prompt_passwd_if_needed(prompt_passwd))							\
		return GC_FAILURE;											\
}

#define GC_PK_APPEND_UNIQ_STRING(in_buff, symmetric_key)								\
{															\
	memcpy(in_buff, symmetric_key, SYMMETRIC_KEY_MAX);								\
	memcpy(in_buff + SYMMETRIC_KEY_MAX, UNIQ_ENC_PARAM_STRING, UNIQ_ENC_PARAM_LEN);					\
}

#ifdef USE_OPENSSL
#define GC_PK_COMPUTE_HASH(symmetric_key_hash, symmetric_key)								\
{															\
	unsigned char	in_buff[HASH_INPUT_BUFF_LEN];									\
															\
	GC_PK_APPEND_UNIQ_STRING(in_buff, symmetric_key);								\
	EVP_Digest(in_buff, HASH_INPUT_BUFF_LEN, symmetric_key_hash, NULL, EVP_sha512(), NULL);				\
	memset(in_buff, 0, HASH_INPUT_BUFF_LEN);									\
}
#else
#define GC_PK_COMPUTE_HASH(symmetric_key_hash, symmetric_key)								\
{															\
	unsigned char	in_buff[HASH_INPUT_BUFF_LEN];									\
															\
	GC_PK_APPEND_UNIQ_STRING(in_buff, symmetric_key);								\
	GC_SYM_INIT;													\
	gcry_md_hash_buffer(GCRY_MD_SHA512, symmetric_key_hash, in_buff, HASH_INPUT_BUFF_LEN);				\
	memset(in_buff, 0, HASH_INPUT_BUFF_LEN);									\
}
#endif

#endif /* GTMCRYPT_PK_REF_H */
