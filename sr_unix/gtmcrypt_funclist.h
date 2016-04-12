/****************************************************************
 *								*
 * Copyright (c) 2013-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Any time a new function gets added to the GT.M encryption interface, add the corresponding entry here. This file is included in
 * 'gtmcrypt_entry' by appropriately defining GTMCRYPT_DEF, to generate necessary string literals and function pointers which are
 * used to 'dlsym' symbols from the GT.M encryption shared library.
 */
GTMCRYPT_DEF(gtmcrypt_init)
GTMCRYPT_DEF(gtmcrypt_strerror)
GTMCRYPT_DEF(gtmcrypt_init_db_cipher_context_by_hash)
GTMCRYPT_DEF(gtmcrypt_init_device_cipher_context_by_keyname)
GTMCRYPT_DEF(gtmcrypt_obtain_db_key_hash_by_keyname)
GTMCRYPT_DEF(gtmcrypt_release_cipher_context)
GTMCRYPT_DEF(gtmcrypt_encrypt_decrypt)
GTMCRYPT_DEF(gtmcrypt_same_key)
GTMCRYPT_DEF(gtmcrypt_close)
