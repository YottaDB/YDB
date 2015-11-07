/****************************************************************
 *								*
 *	Copyright 2009, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license   If you do not know the terms of	*
 *	the license, please stop and do not read further 	*
 *								*
 y***************************************************************/

#ifndef GTMCRYPT_INTERFACE_H
#define GTMCRYPT_INTERFACE_H

#define GTM_PASSWD_ENV			"gtm_passwd"

#define GTMCRYPT_HASH_LEN		64
#define GTMCRYPT_HASH_HEX_LEN		GTMCRYPT_HASH_LEN * 2

#define GTMCRYPT_OP_INTERACTIVE_MODE	0x00000001

#define GTMCRYPT_INVALID_KEY_HANDLE	-1

typedef	int	gtmcrypt_key_t;

_GTM_APIDECL gtm_status_t	gtmcrypt_init(int flags);
_GTM_APIDECL gtm_status_t	gtmcrypt_close(void);
_GTM_APIDECL gtm_status_t	gtmcrypt_hash_gen(gtmcrypt_key_t, gtm_string_t *);
_GTM_APIDECL gtm_status_t	gtmcrypt_encrypt(gtmcrypt_key_t, gtm_string_t *, gtm_string_t *);
_GTM_APIDECL gtm_status_t	gtmcrypt_decrypt(gtmcrypt_key_t, gtm_string_t *, gtm_string_t *);
_GTM_APIDECL gtm_status_t	gtmcrypt_getkey_by_hash(gtm_string_t *, gtmcrypt_key_t *);
_GTM_APIDECL gtm_status_t	gtmcrypt_getkey_by_name(gtm_string_t *, gtmcrypt_key_t *);
_GTM_APIDECL char		*gtmcrypt_strerror(void);

#endif
