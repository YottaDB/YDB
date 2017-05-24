/****************************************************************
 *								*
 * Copyright (c) 2009-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTMCRYPT_REF_H
#define GTMCRYPT_REF_H

#ifdef USE_OPENSSL
# include <openssl/sha.h>
# include <openssl/evp.h>
# include <openssl/err.h>
typedef EVP_CIPHER_CTX		*crypt_key_t;
#else
# include <gcrypt.h>
typedef gcry_cipher_hd_t	crypt_key_t;
#endif

#define DOT_GNUPG			".gnupg"
#define SYMMETRIC_KEY_MAX		32
#define GTMCRYPT_IV_LEN			16

#define GC_MIN_STATIC_BLOCK_SIZE	4096			/* Have a good size block, so that we dont keep reallocating */

/* Some environment variables that encryption plugin cares about */
#define GNUPGHOME			"GNUPGHOME"
#define HOME				"HOME"

/* Following makes sure that at no point we are in the encryption library without a prior call to gtmcrypt_init. */
#define GC_VERIFY_INITED													\
{																\
	if (!gtmcrypt_inited)													\
	{															\
		UPDATE_ERROR_STRING("Encryption library has not been initialized");						\
		return -1;												\
	}															\
}

#endif /* GTMCRYPT_REF_H */
