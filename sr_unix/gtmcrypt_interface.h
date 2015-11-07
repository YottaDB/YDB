/****************************************************************
 *								*
 *	Copyright 2009, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license   If you do not know the terms of	*
 *	the license, please stop and do not read further 	*
 *								*
 ****************************************************************/

/* WARNING: Include gtm_limits.h prior to including this file. */

#ifndef GTMCRYPT_INTERFACE_H
#define GTMCRYPT_INTERFACE_H

/* This module defines the interface for a GT.M encryption plug-in implementation. Refer to the function comments for details. */

/* Environment variable containing the obfuscated password for the key ring with which the symmetric keys are encrypted. */
#define GTM_PASSWD_ENV			"gtm_passwd"

/* Length of the plain and in-hex hash string for a symmetric key. We are currently using SHA-512, so the length is 64 bytes. */
#define GTMCRYPT_HASH_LEN		64
#define GTMCRYPT_HASH_HEX_LEN		GTMCRYPT_HASH_LEN * 2

/* Length of the key name (which for databases is an absolute path to the file). */
#define GTMCRYPT_MAX_KEYNAME_LEN	GTM_PATH_MAX

/* Flag to be used whenever password can be obtained interactively. */
#define GTMCRYPT_OP_INTERACTIVE_MODE	0x00000001

/* Special value that indicates invalid / uninitialized encryption state object. */
#define GTMCRYPT_INVALID_KEY_HANDLE	NULL

/* Constants to indicate what IV-handling mode is desired for a particular encryption or decryption operation. */
#define	GTMCRYPT_IV_CONTINUE		0
#define GTMCRYPT_IV_SET			1
#define GTMCRYPT_IV_RESET		-1

/* Constants to indicate whether a particular operation is of encryption or decryption type. */
#define GTMCRYPT_OP_ENCRYPT		1
#define GTMCRYPT_OP_DECRYPT		0

typedef	void * gtmcrypt_key_t;

/*
 * Initialize encryption if not yet initialized. Use this function to load neccessary libraries and set appropriate configuration
 * options. Upon a successful return this function is never invoked again.
 *
 * Arguments:	flags	Encryption flags to use.
 *
 * Returns:	0 if encryption was initialized successfully; -1 otherwise.
 */
gtm_status_t	gtmcrypt_init(gtm_int_t flags);
/***********************************************************************************************************************************
 * Return the error string. Use this function to provide the current error status. The function is normally invoked following a
 * non-zero return from one of the other functions defined in the interface, which means that each of them should start by clearing
 * the error buffer.
 *
 * Returns:	The error string constructed so far.
 */
gtm_char_t	*gtmcrypt_strerror(void);
/***********************************************************************************************************************************
 * Find the key by hash and set up database encryption and decryption state objects, if not created yet. Use this function to locate
 * a particular key by its hash and, if found, initialize the objects for subsequent encryption and decryption operations on any
 * database that will use this key, unless already initialized. The reason any database relying on the same key may use the same
 * encryption and decryption state objects is that for every encryption and decryption operation the initial IV is used, effectively
 * reverting to the original state.
 *
 * Arguments:	handle	Pointer which should get pointed to the database encryption state object.
 * 		hash	Hash of the key.
 * 		iv	Initialization vector to use for encryption or decryption.
 *
 * Returns:	0 if the key was found and database encryption and decryption state objects were initialized or existed already; -1
 * 		otherwise.
 */
gtm_status_t	gtmcrypt_init_db_cipher_context_by_hash(gtmcrypt_key_t *handle, gtm_string_t hash, gtm_string_t iv);
/***********************************************************************************************************************************
 * Find the key by keyname and set up device encryption or decryption state object. Use this function to locate a particular key by
 * its name (as specified in the configuration file) and, if found, initialize an object for subsequent encryption or decryption
 * operations (depending on the 'encrypt' parameter) with one device using this key. Note that, unlike databases, different devices
 * relying on the same key require individual encryption and decryption state objects as their states evolve with each encryption or
 * decryption operation.
 *
 * Arguments:	handle		Pointer which should get pointed to the device encryption or decryption state object.
 * 		keyname		Name of the key.
 * 		iv		Initialization vector to use for encryption or decryption.
 * 		operation	Flag indicating whether encryption or decryption is desired; use GTMCRYPT_OP_ENCRYPT or
 * 				GTMCRYPT_OP_DECRYPT, respectively.
 *
 * Returns:	0 if the key was found and device encryption or decryption state object was initialized; -1 otherwise.
 */
gtm_status_t	gtmcrypt_init_device_cipher_context_by_keyname(gtmcrypt_key_t *handle, gtm_string_t keyname,
				gtm_string_t iv, gtm_int_t operation);
/***********************************************************************************************************************************
 * Find the key by keyname and obtain its hash. Use this function to locate a particular key by its name and calculate (or copy, if
 * precalculated) its hash to the 'hash_dest' address. Note that the keyname corresponds to a particular 'files' field in the
 * configuration file in case of devices, or a path to a database file otherwise.
 *
 * Arguments:	keyname		Name of the key.
 * 		hash_dest	Pointer to the location where the key's hash is to be copied.
 *
 * Returns:	0 if the key was found and key's hash was copied to the specified location; -1 otherwise.
 */
gtm_status_t	gtmcrypt_obtain_db_key_hash_by_keyname(gtm_string_t keyname, gtm_string_t *hash_dest);
/***********************************************************************************************************************************
 * Release the specified encryption or decryption state object, also releasing the decryption state if database encryption state is
 * specified.
 *
 * Arguments:	handle	Encryption or decryption state object to release.
 *
 * Returns:	0 if the operation was successful; -1 otherwise.
 */
gtm_status_t	gtmcrypt_release_key(gtmcrypt_key_t handle);
/***********************************************************************************************************************************
 * Perform encryption or decryption of the provided data based on the specified encryption / decryption state. If the target buffer
 * pointer is NULL, the operation is done in-place.
 *
 * It is also possible to set the initialization vector (IV) to a particular value, or reset it to the original value, before
 * attempting the operation. The results of mixing different IV modes on the *same* encryption / decryption state object are
 * different between OpenSSL and Gcrypt, though. The difference is that modifying the IV (iv_mode != GTMCRYPT_IV_CONTINUE) with
 * OpenSSL does not affect the actual encryption / decryption state, and subsequent IV-non-modifying encryptions / decryptions
 * (iv_mode == GTMCRYPT_IV_CONTINUE) are performed on whatever state the prior IV-non-modifying encryptions / decryptions arrived
 * at. With Gcrypt, on the other hand, modifying the IV (iv_mode != GTMCRYPT_IV_CONTINUE) before an operation influences the
 * subsequent IV-non-modifying (iv_mode == GTMCRYPT_IV_CONTINUE) operations.
 *
 * Arguments:	handle			Encryption state object to use.
 * 		unencr_block		Block where unencrypted data is read from.
 * 		unencr_block_len	Length of the unencrypted (and encrypted) data block.
 * 		encr_block		Block where encrypted data is put into.
 * 		operation		Flag indicating whether encryption or decryption is desired; use GTMCRYPT_OP_ENCRYPT or
 * 					GTMCRYPT_OP_DECRYPT, respectively.
 * 		iv_mode			Flag indicating whether the initialization vector (IV) should be changed prior to the
 * 					operation; use GTMCRYPT_IV_CONTINUE to proceed without changing the IV, GTMCRYPT_IV_SET to
 * 					set the IV the value supplied in the iv argument, and GTMCRYPT_IV_RESET to reset the IV to
 * 					the value specified at initialization.
 * 		iv			Initialization vector to set the encryption state to when iv_mode is GTMCRYPT_IV_SET.
 *
 * Returns:	0 if the operation succeeded; -1 otherwise.
 */
gtm_status_t	gtmcrypt_encrypt_decrypt(gtmcrypt_key_t handle, gtm_char_t *src_block, gtm_int_t src_block_len,
					  gtm_char_t *dest_block, gtm_int_t operation, gtm_int_t iv_mode, gtm_string_t iv);
/***********************************************************************************************************************************
 * Compare the keys associated with two encryption or decryption state objects.
 *
 * Arguments:	handle1		First ecryption or decryption state object to use.
 * 		handle2		Second ecryption or decryption state object to use.
 *
 * Returns:	1 if both encryption or decryption state objects use the same key; 0 otherwise.
 */
gtm_int_t	gtmcrypt_same_key(gtmcrypt_key_t handle1, gtmcrypt_key_t handle2);
/***********************************************************************************************************************************
 * Disable encryption and discard any sensitive data in memory.
 *
 * Returns:	0 if the operation succeeded; -1 otherwise.
 */
gtm_status_t	gtmcrypt_close(void);

#endif
