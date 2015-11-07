/****************************************************************
 *								*
 *	Copyright 2009, 2014 Fidelity Information Services, Inc *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef	GTMCRYPT_H
#define GTMCRYPT_H

#include "gtmxc_types.h"
#include "gtmimagename.h"
#include "have_crit.h"
#include "deferred_signal_handler.h"
#include "wbox_test_init.h"
#include "gtmmsg.h"
#include "error.h"					/* for MAKE_MSG_WARNING macro */

#define	gtmcrypt_close					(*gtmcrypt_close_fnptr)
#define	gtmcrypt_encrypt_decrypt			(*gtmcrypt_encrypt_decrypt_fnptr)
#define	gtmcrypt_init					(*gtmcrypt_init_fnptr)
#define gtmcrypt_init_db_cipher_context_by_hash		(*gtmcrypt_init_db_cipher_context_by_hash_fnptr)
#define gtmcrypt_init_device_cipher_context_by_keyname	(*gtmcrypt_init_device_cipher_context_by_keyname_fnptr)
#define gtmcrypt_obtain_db_key_hash_by_keyname		(*gtmcrypt_obtain_db_key_hash_by_keyname_fnptr)
#define gtmcrypt_release_key				(*gtmcrypt_release_key_fnptr)
#define	gtmcrypt_same_key				(*gtmcrypt_same_key_fnptr)
#define	gtmcrypt_strerror				(*gtmcrypt_strerror_fnptr)

/* It's important that the "gtmcrypt_interface.h" include should be *after* the above macro definitions. This way, the function
 * prototypes defined in the header file will automatically be expanded to function pointers saving us the trouble of explicitly
 * defining them once again.
 */
#include "gtmcrypt_interface.h"

GBLREF	boolean_t					gtmcrypt_initialized;
GBLREF	mstr						pvt_crypt_buf;
GBLREF	char						dl_err[];
GBLREF	char						*gtmcrypt_badhash_size_msg;

LITREF	char						gtmcrypt_repeat_msg[];
LITREF	gtm_string_t					null_iv;

error_def(ERR_CRYPTDLNOOPEN);
error_def(ERR_CRYPTDLNOOPEN2);
error_def(ERR_CRYPTHASHGENFAILED);
error_def(ERR_CRYPTINIT);
error_def(ERR_CRYPTKEYFETCHFAILED);
error_def(ERR_CRYPTOPFAILED);

/* =====================================================================================================*
 * 					Error Reporting Macros						*
 * =====================================================================================================*/

#define CRYPTERR_MASK					0x10000000
#define REPEAT_MSG_MASK					0x20000000

#define IS_CRYPTERR_MASK(ERRID)				((ERRID) & CRYPTERR_MASK)
#define IS_REPEAT_MSG_MASK(ERRID)			((ERRID) & REPEAT_MSG_MASK)
#define SET_CRYPTERR_MASK(ERRID)			((ERRID) | CRYPTERR_MASK)
#define SET_REPEAT_MSG_MASK(ERRID)			((ERRID) | REPEAT_MSG_MASK)
#define CLEAR_CRYPTERR_MASK(ERRID)			(ERRID = ((ERRID) & ~CRYPTERR_MASK))
#define CLEAR_REPEAT_MSG_MASK(ERRID)			(ERRID = ((ERRID) & ~REPEAT_MSG_MASK))

#define REALLOC_CRYPTBUF_IF_NEEDED(LEN)												\
{																\
	if (!pvt_crypt_buf.addr || (pvt_crypt_buf.len < LEN))									\
	{															\
		if (pvt_crypt_buf.addr)												\
			free(pvt_crypt_buf.addr);										\
		pvt_crypt_buf.addr = (char *)malloc(LEN);									\
		pvt_crypt_buf.len = LEN;											\
	}															\
}

#define GTMCRYPT_REPORT_ERROR(ERRID, MECHANISM, LEN, PTR)									\
{																\
	int			errid;												\
	const char		*errptr;											\
																\
	errid = ERRID;														\
	assert(IS_CRYPTERR_MASK(errid));											\
	CLEAR_CRYPTERR_MASK(errid);												\
	if (IS_REPEAT_MSG_MASK(errid))												\
		errptr = &gtmcrypt_repeat_msg[0];										\
	else if ((ERR_CRYPTDLNOOPEN == errid) || (ERR_CRYPTDLNOOPEN2 == errid)							\
			|| (MAKE_MSG_WARNING(ERR_CRYPTDLNOOPEN2) == errid) || (MAKE_MSG_WARNING(ERR_CRYPTDLNOOPEN) == errid))	\
		errptr = (const char *)&dl_err[0];										\
	else if (ERR_CRYPTHASHGENFAILED == errid)										\
		errptr = (const char *)gtmcrypt_badhash_size_msg;								\
	else															\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		errptr = (const char *)gtmcrypt_strerror();									\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	}															\
	CLEAR_REPEAT_MSG_MASK(errid);												\
	MECHANISM(VARLSTCNT(6) errid, 4, LEN, PTR, LEN_AND_STR(errptr));							\
}

/* =====================================================================================================*/
/* 					GT.M Related Macros						*/
/* =====================================================================================================*/
#define IS_BLK_ENCRYPTED(LEVL, BSIZ)			((0 <= ((char)LEVL)) && (0 < BSIZ))

#define BLK_NEEDS_ENCRYPTION(LEVL, BSIZ)		IS_BLK_ENCRYPTED(LEVL, BSIZ)

#define BLK_NEEDS_ENCRYPTION3(FLAG, LEVL, BSIZ) 	(FLAG && IS_BLK_ENCRYPTED(LEVL, BSIZ))

#define ENCR_INITIALIZED 				gtmcrypt_initialized

#define ENCR_WBOX_ENABLED				(gtm_white_box_test_case_enabled 					\
				 				&& (WBTEST_ENCRYPT_INIT_ERROR == gtm_white_box_test_case_number))

#define ASSERT_ENCRYPTION_INITIALIZED			assert(ENCR_INITIALIZED || ENCR_WBOX_ENABLED)

#define	IS_INTERACTIVE_MODE				(IS_MUMPS_IMAGE)

#define GTMCRYPT_COPY_HASH(SRC, DST)												\
{																\
	memcpy(DST->encryption_hash, SRC->encryption_hash, GTMCRYPT_HASH_LEN);							\
	DST->is_encrypted = SRC->is_encrypted;											\
}																\

/* General Note: All macros below (except GTMCRYPT_CLOSE) take CSA as their first parameter. Currently, most macros do not use CSA,
 * but include a reference to CSA in case a need arises in the future.
 */

/* Database specific initialization - gets the encryption key corresponding to the HASH (SHA-512 currently) found in the database
 * file header and allocates a buffer large enough to encrypt/decrypt database block sizes.
 */
#define INIT_DB_ENCRYPTION(CSA, CSD, RC)											\
{																\
	RC = 0;															\
	GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(CSA, CSD->encryption_hash, CSA->encr_key_handle, RC);				\
}

/* Process specific initialization - dlopen libgtmcrypt.so and invoke gtmcrypt_init() */
#define INIT_PROC_ENCRYPTION(CSA, RC)												\
{																\
	RC = 0;															\
	if (!gtmcrypt_initialized)												\
	{															\
		if (0 == (RC = gtmcrypt_entry()))										\
		{	/* dlopen succeeded */											\
			DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);								\
			if (0 != gtmcrypt_init(IS_INTERACTIVE_MODE ? GTMCRYPT_OP_INTERACTIVE_MODE : 0))				\
				RC = SET_CRYPTERR_MASK(ERR_CRYPTINIT);								\
			else													\
				gtmcrypt_initialized = TRUE; /* Intialization is done for this process. */			\
			ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);								\
		} else														\
			RC = SET_CRYPTERR_MASK(RC);										\
	}															\
}

/* Given a cryptographic hash (currently SHA-512), the below macro retrieves a handle to the symmetric key corresponding to
 * the hash. This macro is always called before attempting an encrypt or decrypt operation.
 */
#define GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(CSA, HASH, ENCRYPT_KEY_HANDLE, RC)							\
{																\
	gtm_string_t hash_string;												\
																\
	ENCRYPT_KEY_HANDLE = GTMCRYPT_INVALID_KEY_HANDLE;									\
	if (gtmcrypt_initialized)												\
	{															\
		assert(NULL != HASH);												\
		hash_string.length = GTMCRYPT_HASH_LEN;										\
		hash_string.address = HASH;											\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		if (0 != gtmcrypt_init_db_cipher_context_by_hash(&(ENCRYPT_KEY_HANDLE), hash_string, null_iv))			\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTKEYFETCHFAILED);							\
		else														\
			RC = 0;													\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	} else															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
}

/* Ensure that the symmetric key corresponding to the specified hash exists and that a handle is created. */
#define GTMCRYPT_HASH_CHK(CSA, HASH, RC)											\
{																\
	gtmcrypt_key_t handle;													\
																\
	GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(CSA, HASH, handle, RC);								\
}

/* The below macro retrieves a handle to the symmetric key corresponding to the provided key name as specified in the
 * configuration file.
 */
#define GTMCRYPT_INIT_CIPHER_CONTEXT(KEYNAME_LENGTH, KEYNAME, IV_LENGTH, IV, KEY_HANDLE, OPERATION, RC)				\
{																\
	gtm_string_t keyname, iv;												\
																\
	KEY_HANDLE = GTMCRYPT_INVALID_KEY_HANDLE;										\
	if (gtmcrypt_initialized)												\
	{															\
		assert(NULL != KEYNAME);											\
		assert(NULL != IV);												\
		keyname.length = KEYNAME_LENGTH;										\
		keyname.address = KEYNAME;											\
		iv.length = IV_LENGTH;												\
		iv.address = IV;												\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		if (0 != gtmcrypt_init_device_cipher_context_by_keyname(&(KEY_HANDLE), keyname, iv, OPERATION))			\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTKEYFETCHFAILED);							\
		else														\
			RC = 0;													\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	} else															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
}

/* Safely remove the specified handle to a particular symmetric key. */
#define GTMCRYPT_REMOVE_CIPHER_CONTEXT(KEY_HANDLE)										\
{																\
	if (gtmcrypt_initialized)												\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		gtmcrypt_release_key(KEY_HANDLE);										\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	}															\
}

/* Based on the database name (used as a key name), the below macro looks up the corresponding symmetric key and copies its hash
 * into the passed buffer storage.
 */
#define GTMCRYPT_HASH_GEN(CSA, FILENAME, FILENAME_LENGTH, HASH, RC)								\
{																\
	gtm_string_t filename, hash_string;											\
																\
	if (gtmcrypt_initialized)												\
	{															\
		assert(NULL != FILENAME);											\
		assert(NULL != HASH);												\
		filename.length = FILENAME_LENGTH;										\
		filename.address = FILENAME;											\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		if (0 != gtmcrypt_obtain_db_key_hash_by_keyname(filename, &hash_string))					\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTKEYFETCHFAILED);							\
		else														\
		{														\
			if (hash_string.length != GTMCRYPT_HASH_LEN)								\
			{	/* Populate the message about the bad hash size, allocating the buffer, if necessary. */	\
				if (NULL == gtmcrypt_badhash_size_msg)								\
					gtmcrypt_badhash_size_msg = (char *)malloc(1024);					\
				SNPRINTF(gtmcrypt_badhash_size_msg, 1023, "Specified symmetric key hash has "			\
					"length %d, which is different from the expected hash length %d",			\
					hash_string.length, GTMCRYPT_HASH_LEN);							\
				RC = SET_CRYPTERR_MASK(ERR_CRYPTHASHGENFAILED);							\
			} else													\
			{	/* Note that the copy is not NULL-terminated. */						\
				memcpy(HASH, hash_string.address, hash_string.length);						\
				RC = 0;												\
			}													\
		}														\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	} else															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
}

/* Encrypt data with the IV reset to the initial value prior to the operation. */
#define GTMCRYPT_ENCRYPT(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, RC)								\
	GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, GTMCRYPT_OP_ENCRYPT, GTMCRYPT_IV_RESET, RC)

/* Decrypt data with the IV reset to the initial value prior to the operation. */
#define GTMCRYPT_DECRYPT(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, RC)								\
	GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, GTMCRYPT_OP_DECRYPT, GTMCRYPT_IV_RESET, RC)

/* Encrypt or decrypt data with the IV optionally set to a specified, or reset to the initial, value prior to the operation. */
#define GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, OPERATION, IV_MODE, RC)			\
{																\
	assert(INBUF);														\
	if (gtmcrypt_initialized && (GTMCRYPT_INVALID_KEY_HANDLE != KEY_HANDLE))						\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		if (0 == gtmcrypt_encrypt_decrypt(KEY_HANDLE, (char *)INBUF, INBUF_LEN, (char *)OUTBUF,				\
				OPERATION, IV_MODE, null_iv))									\
			RC = 0;													\
		else														\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED);								\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	} else															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
}

/* Check whether the specified symmetric key handles belong to the same key. */
#define GTMCRYPT_SAME_KEY(KEY_HANDLE1, KEY_HANDLE2)										\
	gtmcrypt_same_key(KEY_HANDLE1, KEY_HANDLE2)

/* Shut down the encryption for this process. */
#define GTMCRYPT_CLOSE														\
{																\
	if (gtmcrypt_initialized)												\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		gtmcrypt_close();												\
		gtmcrypt_initialized = FALSE;											\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	}															\
}

uint4 gtmcrypt_entry(void);
boolean_t verify_lib_loadpath(const char *libname, char *loadpath);

#endif /* GTMCRYPT_H */
