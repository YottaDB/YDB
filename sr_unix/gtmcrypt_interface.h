/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license   If you do not know the terms of	*
 *	the license, please stop and do not read further 	*
 *								*
 y***************************************************************/

#ifndef GTMCRYPT_INTERFACE_H
#define GTMCRYPT_INTERFACE_H

xc_status_t	gtmcrypt_init(int);
xc_status_t	gtmcrypt_close(void);
xc_status_t	gtmcrypt_hash_gen(gtmcrypt_key_t, xc_string_t *);
xc_status_t	gtmcrypt_encrypt(gtmcrypt_key_t, xc_string_t *, xc_string_t *);
xc_status_t	gtmcrypt_decrypt(gtmcrypt_key_t, xc_string_t *, xc_string_t *);
xc_status_t	gtmcrypt_getkey_by_hash(xc_string_t *, gtmcrypt_key_t *);
xc_status_t	gtmcrypt_getkey_by_name(xc_string_t *, gtmcrypt_key_t *);
char		*gtmcrypt_strerror(void);

#endif /* GTMCRYPT_INTERFACE_H */
