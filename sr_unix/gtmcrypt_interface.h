/****************************************************************
 *								*
 * Copyright (c) 2009-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/* Definitions must match those in gtm_tls_interface.h */
/* Flag to be used whenever password can be obtained interactively. */
#define GTMCRYPT_OP_INTERACTIVE_MODE	0x00000001
/* No environment variable for password - used by gc_update_passwd so must be same in gtmcrypt_interface.h */
#define	GTMCRYPT_OP_NOPWDENVVAR		0x00000800

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
 * Initialize encryption if not yet initialized. Use this function to load necessary libraries and set appropriate configuration
 * options. Upon a successful return this function is never invoked again.
 *
 * Arguments:	flags	Encryption flags to use.
 *
 * Returns:	0 if encryption was initialized successfully; -1 otherwise.
 */
gtm_status_t	gtmcrypt_init(gtm_int_t flags);

/*
 * Return the error string. Use this function to provide the current error status. The function is normally invoked following a
 * non-zero return from one of the other functions defined in the interface, which means that each of them should start by clearing
 * the error buffer.
 *
 * Returns:	The error string constructed so far.
 */
gtm_char_t	*gtmcrypt_strerror(void);

/*
 * Find the key by hash and database path and set up database encryption and decryption state objects, if not created yet. Use this
 * function to locate a particular key by its hash and, if found, initialize the objects for subsequent encryption and decryption
 * operations on any database that will use this key, unless already initialized. If the db_path argument specifies a non-null
 * string, then the key should additionally correspond to that database in the configuration file.
 *
 * The reason that any database relying on the same key may use the same encryption and decryption state objects is this: Every
 * database's encryption and decryption handles are initialized with a null IV, and every block is processed using either a null IV
 * or IV corresponding to the block number. So, for every encryption and decryption operation the IV is always preset to the
 * "correct" value, effectively making it suitable for every database using the same hash.
 *
 * Arguments:	handle		Pointer to the database encryption state object supplied by the caller and filled in by this
 * 				routine.
 * 		key_hash	Hash of the key.
 * 		db_path		Path to the database file that should be associated with the sought key. Can be an empty string.
 * 		iv		Initialization vector to use for encryption or decryption.
 *
 * Returns:	0 if the routine found the key, and either found existing database encryption and decryption state objects or
 * 		initialized them; -1 otherwise.
 */
gtm_status_t	gtmcrypt_init_db_cipher_context_by_hash(gtmcrypt_key_t *handle, gtm_string_t key_hash,
				gtm_string_t db_path, gtm_string_t iv);

/*
 * Find the key by its name and set up device encryption or decryption state object. Use this function to locate a particular key by
 * its name (as specified in the configuration file) and, if found, initialize an object for subsequent encryption or decryption
 * operations (depending on the 'encrypt' parameter) with one device using this key. Note that, unlike databases, different devices
 * relying on the same key require individual encryption and decryption state objects as their states evolve with each encryption or
 * decryption operation.
 *
 * Arguments:	handle		Pointer to the database encryption state object supplied by the caller and filled in by this
 * 				routine.
 * 		key_name	Name of the key.
 * 		iv		Initialization vector to use for encryption or decryption.
 * 		operation	Flag indicating whether encryption or decryption is desired; use GTMCRYPT_OP_ENCRYPT or
 * 				GTMCRYPT_OP_DECRYPT, respectively.
 *
 * Returns:	0 if the routine found the key, and either found existing database encryption and decryption state objects or
 *		initialized them; -1 otherwise.
 */
gtm_status_t	gtmcrypt_init_device_cipher_context_by_keyname(gtmcrypt_key_t *handle, gtm_string_t key_name,
				gtm_string_t iv, gtm_int_t operation);

/*
 * Find the key by the path of the database it corresponds to as well as its own path, and obtain its hash. Use this function to
 * locate a particular key by the path of the database that is associated with the key in the configuration file and calculate (or
 * copy, if precalculated) its hash to the 'hash_dest' address. If the key_path argument specifies a non-null string, then the key
 * should have the corresponding path; otherwise, the *last* of all keys associated with the specified database in the configuration
 * file is used.
 *
 * Arguments:	db_path		Path to the database file that should be associated with the sought key.
 * 		key_path	Path to the key. Can be an empty string.
 * 		hash_dest	Pointer to the location for this routine to copy the key's hash.
 *
 * Returns:	0 if the routine found the key and copied its hash to the specified location; -1 otherwise.
 */
gtm_status_t	gtmcrypt_obtain_db_key_hash_by_keyname(gtm_string_t db_path, gtm_string_t key_path, gtm_string_t *hash_dest);

/*
 * Release the specified encryption or decryption state object, also releasing the decryption state if database encryption state is
 * specified.
 *
 * Arguments:	handle	Pointer to the encryption or decryption state object to release.
 *
 * Returns:	0 if the operation succeeded; -1 otherwise.
 */
gtm_status_t	gtmcrypt_release_cipher_context(gtmcrypt_key_t handle);

/*
 * Perform encryption or decryption of the provided data based on the specified encryption / decryption state. If the target buffer
 * pointer is NULL, the operation is done in-place. It is also possible to set the initialization vector (IV) to a particular value,
 * or reset it to the original value, before attempting the operation. Note that the changes are persistent.
 *
 * Arguments:	handle			Encryption state object.
 * 		unencr_block		Block of unencrypted data.
 * 		unencr_block_len	Length of the unencrypted and encrypted data blocks.
 * 		encr_block		Block of encrypted data.
 * 		operation		Flag indicating whether to perform encryption or decryption; use GTMCRYPT_OP_ENCRYPT or
 * 					GTMCRYPT_OP_DECRYPT, respectively.
 * 		iv_mode			Flag indicating whether to change the initialization vector (IV) prior to the operation; use
 * 					GTMCRYPT_IV_CONTINUE to proceed without changing the IV, GTMCRYPT_IV_SET to set the IV the
 * 					value supplied in the iv argument, and GTMCRYPT_IV_RESET to reset the IV to the value
 * 					specified at initialization.
 * 		iv			Initialization vector for the encryption state to take when iv_mode is GTMCRYPT_IV_SET.
 *
 * Returns:	0 if the operation succeeded; -1 otherwise.
 */
gtm_status_t	gtmcrypt_encrypt_decrypt(gtmcrypt_key_t handle, gtm_char_t *src_block, gtm_int_t src_block_len,
					  gtm_char_t *dest_block, gtm_int_t operation, gtm_int_t iv_mode, gtm_string_t iv);

/*
 * Compare the keys associated with two encryption or decryption state objects.
 *
 * Arguments:	handle1		First encryption or decryption state object.
 * 		handle2		Second encryption or decryption state object.
 *
 * Returns:	1 if both encryption or decryption state objects use the same key; 0 otherwise.
 */
gtm_int_t	gtmcrypt_same_key(gtmcrypt_key_t handle1, gtmcrypt_key_t handle2);

/*
 * Disable encryption and discard any sensitive data in memory.
 *
 * Returns:	0 if the operation succeeded; -1 otherwise.
 */
gtm_status_t	gtmcrypt_close(void);

#endif
