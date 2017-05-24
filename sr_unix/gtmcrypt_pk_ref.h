/****************************************************************
 *								*
 * Copyright (c) 2009-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
xc_status_t 		gc_pk_update_passwd();
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

#ifdef USE_GPGME_PINENTRY_MODE_LOOPBACK
/* Tell GPGME to set the pinentry mode to LOOPBACK. Note that turning this on implies that the
 * installed gpg/gpg2 supports the command line argument. If it does not, gpg will error out
 * leading to confusion. Rely on the Makefile to detect GPG 2.1.12+ under the assumption that
 * an upgrade will not break this option.
 * We could use gpgme_set_global_flag("require-gnupg","2.1.0"), but that causes a failure in
 * GPGME where we want to operate in a compatible mode.
 * Attempting to use the GPGME library version does not work because distributions have
 * packaged GPGME versions ahead of GnuPG binaries (this was true at least on Debian).
 *   https://lists.gt.net/gnupg/devel/72103#72103
 */
#define	GPG_MAJOR_PINENTRY_LOOPBACK	2
#define	GPG_SEPERATOR_PINENTRY_LOOPBACK	'.'
#define	GPG_MINOR_PINENTRY_LOOPBACK	1
#define	GPG_SUBMINOR_PINENTRY_LOOPBACK	12
#define GC_PK_INIT_PINENTRY_MODE											\
{															\
	gpgme_engine_info_t		info;										\
	int				i, len, major, minor, sub;							\
															\
	GC_PK_INIT_DO_OR_ERR(gpgme_get_engine_info(&info));								\
	/* Find the right protocol entry for the version check */							\
	while (info && (info->protocol != GPGME_PROTOCOL_OpenPGP))							\
		info = info->next;											\
	/* Check the version before using */										\
	if (info && info->version)											\
	{ 														\
		len = strlen(info->version);										\
		for (major = i = 0; (i <= len) && (('0' < info->version[i]) && ('9' >= info->version[i])); i++)		\
			major = (major * 10) + (info->version[i] - '0');						\
		if (GPG_SEPERATOR_PINENTRY_LOOPBACK == info->version[i]) i++;						\
		for (minor = 0; (i <= len) && (('0' < info->version[i]) && ('9' >= info->version[i])); i++)		\
			minor = (minor * 10) + (info->version[i] - '0');						\
		if (GPG_SEPERATOR_PINENTRY_LOOPBACK == info->version[i]) i++;						\
		for (sub = 0; (i <= len) && (('0' < info->version[i]) && ('9' >= info->version[i])); i++)		\
			sub = (sub * 10) + (info->version[i] - '0');							\
		if ((GPG_MAJOR_PINENTRY_LOOPBACK < major) ||								\
			((GPG_MAJOR_PINENTRY_LOOPBACK == major) &&							\
			 ((GPG_MINOR_PINENTRY_LOOPBACK < minor) ||							\
			 ((GPG_MINOR_PINENTRY_LOOPBACK ==  minor) && (GPG_SUBMINOR_PINENTRY_LOOPBACK <= sub)))))	\
		{													\
			GC_PK_INIT_DO_OR_ERR(gpgme_set_pinentry_mode(pk_crypt_ctx, GPGME_PINENTRY_MODE_LOOPBACK));	\
		}													\
	}														\
}
#else
#define GC_PK_INIT_PINENTRY_MODE
#endif


#define GC_PK_INIT_DO_OR_ERR(LINE)											\
{															\
	err = LINE;													\
	if (err)													\
	{														\
		pk_crypt_ctx = NULL;											\
		UPDATE_ERROR_STRING("Error initializing GpgME: %s/%s", gpgme_strsource(err), gpgme_strerror(err));	\
		return -1;												\
	}														\
}

/* Public key cryptography related macros */
#define GC_PK_INIT													\
{															\
	gpgme_error_t			err;										\
	GBLREF	gpgme_ctx_t		pk_crypt_ctx;									\
															\
	gpgme_check_version(NULL); /* This initializes the gpgme engine. */						\
	GC_PK_INIT_DO_OR_ERR(gpgme_new(&pk_crypt_ctx));									\
	GC_PK_INIT_DO_OR_ERR(gpgme_set_protocol(pk_crypt_ctx, GPGME_PROTOCOL_OpenPGP));					\
	GC_PK_INIT_PINENTRY_MODE;											\
	/* Attempt to set a callback to retrieve the password. Works with GPG classic and GPG 2.1+ */			\
	gpgme_set_passphrase_cb(pk_crypt_ctx,										\
				(gpgme_passphrase_cb_t) gc_pk_crypt_passphrase_callback, NULL);				\
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
	gcry_md_hash_buffer(GCRY_MD_SHA512, symmetric_key_hash, in_buff, HASH_INPUT_BUFF_LEN);				\
	memset(in_buff, 0, HASH_INPUT_BUFF_LEN);									\
}
#endif

#endif /* GTMCRYPT_PK_REF_H */
