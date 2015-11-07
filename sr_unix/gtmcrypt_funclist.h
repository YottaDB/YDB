/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Any time a new function gets added to the GT.M encryption interface, add the corresponding entry here. This file is included in
 * `gtmcrypt_entry', by appropriately, defining GTMCRYPT_DEF, to generate necessary string literals and function pointers which
 * is used to `dlsym' symbols from the GT.M encryption shared library.
 */
GTMCRYPT_DEF(gtmcrypt_strerror)
GTMCRYPT_DEF(gtmcrypt_init)
GTMCRYPT_DEF(gtmcrypt_hash_gen)
GTMCRYPT_DEF(gtmcrypt_encrypt)
GTMCRYPT_DEF(gtmcrypt_decrypt)
GTMCRYPT_DEF(gtmcrypt_getkey_by_name)
GTMCRYPT_DEF(gtmcrypt_getkey_by_hash)
GTMCRYPT_DEF(gtmcrypt_close)
