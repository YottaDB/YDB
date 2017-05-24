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
#define gtmcrypt_release_cipher_context			(*gtmcrypt_release_cipher_context_fnptr)
#define	gtmcrypt_same_key				(*gtmcrypt_same_key_fnptr)
#define	gtmcrypt_strerror				(*gtmcrypt_strerror_fnptr)

/* It's important that the "gtmcrypt_interface.h" include should be *after* the above macro definitions. This way, the function
 * prototypes defined in the header file will automatically be expanded to function pointers saving us the trouble of explicitly
 * defining them once again.
 */
#include "gtmcrypt_interface.h"

#define GTM_MAX_IV_LEN					16

typedef struct enc_handles_struct
{
	gtmcrypt_key_t	encr_key_handle;
	gtmcrypt_key_t	encr_key_handle2;
} enc_handles;

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
	intrpt_state_t		prev_intrpt_state;										\
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
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
		errptr = (const char *)gtmcrypt_strerror();									\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
	}															\
	CLEAR_REPEAT_MSG_MASK(errid);												\
	MECHANISM(VARLSTCNT(6) errid, 4, LEN, PTR, LEN_AND_STR(errptr));							\
}

#define CORE_ON_CRYPTOPFAILED													\
MBSTART {															\
	/* if we are not expecting nor forcing CRYPTOPFAILED get a core for analysis */						\
	if (!WBTEST_ENABLED(WBTEST_EXPECT_CRYPTOPFAILED) && !ENCR_WBOX_ENABLED)							\
		gtm_fork_n_core();												\
} MBEND

/* =====================================================================================================*/
/* 					GT.M Related Macros						*/
/* =====================================================================================================*/

#define IS_ENCRYPTED_BIT				1
#define TO_BE_ENCRYPTED_BIT				2

#define UNSTARTED					-1

#define EMPTY_GTMCRYPT_HASH16				"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define EMPTY_GTMCRYPT_HASH32				EMPTY_GTMCRYPT_HASH16 EMPTY_GTMCRYPT_HASH16
#define EMPTY_GTMCRYPT_HASH				EMPTY_GTMCRYPT_HASH32 EMPTY_GTMCRYPT_HASH32

/* Below macros accept any other field in place of CSD and CSA as long they contain the requisite fields. */
#define IS_ENCRYPTED(IS_ENCRYPTED_FIELD)		(IS_ENCRYPTED_FIELD & IS_ENCRYPTED_BIT)
#define TO_BE_ENCRYPTED(IS_ENCRYPTED_FIELD)		(IS_ENCRYPTED_FIELD & TO_BE_ENCRYPTED_BIT)
#define	USES_ENCRYPTION(IS_ENCRYPTED_FIELD)		(IS_ENCRYPTED_FIELD != 0)
#define MARK_AS_ENCRYPTED(IS_ENCRYPTED_FIELD)		IS_ENCRYPTED_FIELD |= IS_ENCRYPTED_BIT
#define MARK_AS_TO_BE_ENCRYPTED(IS_ENCRYPTED_FIELD)	IS_ENCRYPTED_FIELD |= TO_BE_ENCRYPTED_BIT
#define SET_AS_ENCRYPTED(IS_ENCRYPTED_FIELD)		IS_ENCRYPTED_FIELD = IS_ENCRYPTED_BIT
#define SET_AS_UNENCRYPTED(IS_ENCRYPTED_FIELD)		IS_ENCRYPTED_FIELD = 0

#define USES_NEW_KEY(CSD)				(TO_BE_ENCRYPTED((CSD)->is_encrypted)					\
								&& (UNSTARTED < (CSD)->encryption_hash_cutoff))
#define USES_ANY_KEY(CSD)				(IS_ENCRYPTED((CSD)->is_encrypted) 					\
								|| (TO_BE_ENCRYPTED((CSD)->is_encrypted)			\
									&& (UNSTARTED < (CSD)->encryption_hash_cutoff)))
#define NEEDS_NEW_KEY(CSD, TN)				(TO_BE_ENCRYPTED((CSD)->is_encrypted)					\
								&& (UNSTARTED < (CSD)->encryption_hash_cutoff)			\
								&& ((CSD)->encryption_hash2_start_tn <= TN))
#define NEEDS_ANY_KEY(CSD, TN)				(IS_ENCRYPTED((CSD)->is_encrypted)					\
								|| (TO_BE_ENCRYPTED((CSD)->is_encrypted)			\
									&& (UNSTARTED < (CSD)->encryption_hash_cutoff)		\
									&& ((CSD)->encryption_hash2_start_tn <= TN)))

#define IS_BLK_ENCRYPTED(LEVL, BSIZ)			((0 <= ((char)LEVL)) && (0 < BSIZ))

#define BLK_NEEDS_ENCRYPTION(LEVL, BSIZ)		IS_BLK_ENCRYPTED(LEVL, BSIZ)

#define BLK_NEEDS_ENCRYPTION3(FLAG, LEVL, BSIZ) 	(FLAG && IS_BLK_ENCRYPTED(LEVL, BSIZ))

#define ENCR_INITIALIZED 				gtmcrypt_initialized

#define ENCR_WBOX_ENABLED				(gtm_white_box_test_case_enabled 					\
				 				&& (WBTEST_ENCRYPT_INIT_ERROR == gtm_white_box_test_case_number))

#define ASSERT_ENCRYPTION_INITIALIZED			assert(ENCR_INITIALIZED || ENCR_WBOX_ENABLED)

#define	IS_INTERACTIVE_MODE				(IS_MUMPS_IMAGE)

#define GTMCRYPT_COPY_ENCRYPT_SETTINGS(SRC, DST)										\
{																\
	(DST)->is_encrypted = (SRC)->is_encrypted;										\
	memcpy((DST)->encryption_hash, (SRC)->encryption_hash, GTMCRYPT_HASH_LEN);						\
	memcpy((DST)->encryption_hash2, (SRC)->encryption_hash2, GTMCRYPT_HASH_LEN);						\
	(DST)->non_null_iv = (SRC)->non_null_iv;										\
	(DST)->encryption_hash_cutoff = (SRC)->encryption_hash_cutoff;								\
	(DST)->encryption_hash2_start_tn = (SRC)->encryption_hash2_start_tn;							\
}																\

#define SAME_ENCRYPTION_SETTINGS(SRC1, SRC2)											\
	(((SRC1)->is_encrypted == (SRC2)->is_encrypted)										\
		&& (!IS_ENCRYPTED((SRC1)->is_encrypted)										\
			|| (!memcmp((SRC1)->encryption_hash, (SRC2)->encryption_hash, GTMCRYPT_HASH_LEN)			\
				&& ((SRC1)->non_null_iv == (SRC2)->non_null_iv)))						\
		&& ((SRC1)->encryption_hash_cutoff == (SRC2)->encryption_hash_cutoff)						\
		&& ((UNSTARTED == (SRC1)->encryption_hash_cutoff)								\
			|| (!memcmp((SRC1)->encryption_hash2, (SRC2)->encryption_hash2, GTMCRYPT_HASH_LEN)			\
				&& ((SRC1)->encryption_hash2_start_tn == (SRC2)->encryption_hash2_start_tn))))

/* General Note: All macros below (except GTMCRYPT_CLOSE) take CSA as their first parameter. Currently, most macros do not use CSA,
 * but include a reference to CSA in case a need arises in the future.
 */

/* Database specific initialization - gets the encryption key corresponding to the HASH (SHA-512 currently) found in the database
 * file header and allocates a buffer large enough to encrypt/decrypt database block sizes.
 */
#define INIT_DB_OR_JNL_ENCRYPTION(CSA, CSD, FILENAME_LEN, FILENAME, RC)								\
{																\
	RC = 0;															\
	(CSA)->encr_key_handle = GTMCRYPT_INVALID_KEY_HANDLE;									\
	(CSA)->encr_key_handle2 = GTMCRYPT_INVALID_KEY_HANDLE;									\
	if (IS_ENCRYPTED((CSD)->is_encrypted))											\
	{															\
		GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(CSA, (CSD)->encryption_hash,							\
				FILENAME_LEN, FILENAME, (CSA)->encr_key_handle, RC);						\
	}															\
	if ((0 == RC) && USES_NEW_KEY(CSD))											\
	{															\
		GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(CSA, (CSD)->encryption_hash2,						\
				FILENAME_LEN, FILENAME, (CSA)->encr_key_handle2, RC);						\
	}															\
}

/* Process specific initialization - dlopen libgtmcrypt.so and invoke gtmcrypt_init() */
#define INIT_PROC_ENCRYPTION(CSA, RC)												\
{																\
	intrpt_state_t prev_intrpt_state;											\
																\
	RC = 0;															\
	if (!gtmcrypt_initialized)												\
	{															\
		if (0 == (RC = gtmcrypt_entry()))										\
		{	/* dlopen succeeded */											\
			DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);						\
			if (0 != gtmcrypt_init(IS_INTERACTIVE_MODE ? GTMCRYPT_OP_INTERACTIVE_MODE : 0))				\
				RC = SET_CRYPTERR_MASK(ERR_CRYPTINIT);								\
			else													\
				gtmcrypt_initialized = TRUE; /* Intialization is done for this process. */			\
			ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);						\
		} else														\
			RC = SET_CRYPTERR_MASK(RC);										\
	}															\
}

/* Given a cryptographic hash (currently SHA-512), the below macro retrieves a handle to the symmetric key corresponding to
 * the hash. This macro is always called before attempting an encrypt or decrypt operation.
 */
#define GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(CSA, HASH, DB_PATH_LENGTH, DB_PATH, ENCRYPT_KEY_HANDLE, RC)				\
{																\
	gtm_string_t	hash_string, db_path_string;										\
	intrpt_state_t	prev_intrpt_state;											\
																\
	ENCRYPT_KEY_HANDLE = GTMCRYPT_INVALID_KEY_HANDLE;									\
	if (gtmcrypt_initialized)												\
	{															\
		assert(NULL != HASH);												\
		hash_string.length = (gtm_long_t)GTMCRYPT_HASH_LEN;								\
		hash_string.address = (gtm_char_t *)(HASH);									\
		db_path_string.length = (gtm_long_t)DB_PATH_LENGTH;								\
		db_path_string.address = (gtm_char_t *)(DB_PATH);								\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
		if (0 != gtmcrypt_init_db_cipher_context_by_hash(&(ENCRYPT_KEY_HANDLE), hash_string, db_path_string, null_iv))	\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTKEYFETCHFAILED);							\
		else														\
			RC = 0;													\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
	} else															\
	{															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
		DEBUG_ONLY(CORE_ON_CRYPTOPFAILED);										\
	}															\
}

/* Ensure that the symmetric key corresponding to the specified hash exists and that a handle is created. */
#define GTMCRYPT_HASH_CHK(CSA, HASH, DB_PATH_LENGTH, DB_PATH, RC)								\
{																\
	gtmcrypt_key_t handle;													\
																\
	GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(CSA, HASH, DB_PATH_LENGTH, DB_PATH, handle, RC);					\
}

/* The below macro retrieves a handle to the symmetric key corresponding to the provided key name as specified in the
 * configuration file.
 */
#define GTMCRYPT_INIT_CIPHER_CONTEXT(KEYNAME_LENGTH, KEYNAME, IV_LENGTH, IV, KEY_HANDLE, OPERATION, RC)				\
{																\
	gtm_string_t	keyname, iv;												\
	intrpt_state_t	prev_intrpt_state;											\
																\
	KEY_HANDLE = GTMCRYPT_INVALID_KEY_HANDLE;										\
	if (gtmcrypt_initialized)												\
	{															\
		assert(NULL != KEYNAME);											\
		assert(NULL != IV);												\
		keyname.length = (gtm_long_t)KEYNAME_LENGTH;									\
		keyname.address = (gtm_char_t *)(KEYNAME);									\
		iv.length = (gtm_long_t)IV_LENGTH;										\
		iv.address = (gtm_char_t *)(IV);										\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
		if (0 != gtmcrypt_init_device_cipher_context_by_keyname(&(KEY_HANDLE), keyname, iv, OPERATION))			\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTKEYFETCHFAILED);							\
		else														\
			RC = 0;													\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
	} else															\
	{															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
		DEBUG_ONLY(CORE_ON_CRYPTOPFAILED);										\
	}															\
}

/* Safely remove the specified handle to a particular symmetric key. */
#define GTMCRYPT_REMOVE_CIPHER_CONTEXT(KEY_HANDLE, RC)										\
{																\
	intrpt_state_t	prev_intrpt_state;											\
																\
	RC = 0;															\
	if (gtmcrypt_initialized && (GTMCRYPT_INVALID_KEY_HANDLE != KEY_HANDLE))						\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
		if (0 != gtmcrypt_release_cipher_context(KEY_HANDLE))								\
		{														\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTKEYFETCHFAILED);							\
		} else														\
		{														\
			KEY_HANDLE = GTMCRYPT_INVALID_KEY_HANDLE;								\
			RC = 0;													\
		}														\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
	}															\
}

/* Based on the database name (used as a key name), the below macro looks up the corresponding symmetric key and copies its hash
 * into the passed buffer storage.
 */
#define GTMCRYPT_HASH_GEN(CSA, FILENAME_LENGTH, FILENAME, KEY_PATH_LENGTH, KEY_PATH, HASH, RC)					\
{																\
	gtm_string_t	filename_string, hash_string, key_path_string;								\
	intrpt_state_t	prev_intrpt_state;											\
																\
	if (gtmcrypt_initialized)												\
	{															\
		assert(NULL != FILENAME);											\
		assert(NULL != HASH);												\
		filename_string.length = (gtm_long_t)FILENAME_LENGTH;								\
		filename_string.address = (gtm_char_t *)(FILENAME);								\
		key_path_string.length = (gtm_long_t)KEY_PATH_LENGTH;								\
		key_path_string.address = (gtm_char_t *)(KEY_PATH);								\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
		if (0 != gtmcrypt_obtain_db_key_hash_by_keyname(filename_string, key_path_string, &hash_string))		\
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
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
	} else															\
	{															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
		DEBUG_ONLY(CORE_ON_CRYPTOPFAILED);										\
	}															\
}

/* Encrypt data with either a null IV or set to the specified value prior to the operation. */
#define GTMCRYPT_ENCRYPT(CSA, USE_NON_NULL_IV, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, IV_ADDR, IV_LEN, RC)			\
{																\
	gtm_string_t iv_macro;													\
																\
	if (!(USE_NON_NULL_IV))													\
	{															\
		GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,					\
			GTMCRYPT_OP_ENCRYPT, GTMCRYPT_IV_RESET, null_iv, RC);							\
	} else															\
	{															\
		assert(IV_LEN <= GTM_MAX_IV_LEN);										\
		iv_macro.address = (gtm_char_t *)IV_ADDR;									\
		iv_macro.length = (gtm_long_t)IV_LEN;										\
		GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,					\
				GTMCRYPT_OP_ENCRYPT, GTMCRYPT_IV_SET, iv_macro, RC);						\
	}															\
}

/* Decrypt data with either a null IV or set to the specified value prior to the operation. */
#define GTMCRYPT_DECRYPT(CSA, USE_NON_NULL_IV, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, IV_ADDR, IV_LEN, RC)			\
{																\
	gtm_string_t iv_macro;													\
																\
	if (!(USE_NON_NULL_IV))													\
	{															\
		GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,					\
			GTMCRYPT_OP_DECRYPT, GTMCRYPT_IV_RESET, null_iv, RC);							\
	} else															\
	{															\
		assert(IV_LEN <= GTM_MAX_IV_LEN);										\
		iv_macro.address = (gtm_char_t *)IV_ADDR;									\
		iv_macro.length = (gtm_long_t)IV_LEN;										\
		GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,					\
				GTMCRYPT_OP_DECRYPT, GTMCRYPT_IV_SET, iv_macro, RC);						\
	}															\
}

/* Encrypt data with the IV reset to all-NULL initial value prior to the operation. */
#define GTMCRYPT_ENCRYPT_NO_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, RC)							\
	GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,						\
			GTMCRYPT_OP_ENCRYPT, GTMCRYPT_IV_RESET, null_iv, RC)

/* Decrypt data with the IV reset to all-NULL initial value prior to the operation. */
#define GTMCRYPT_DECRYPT_NO_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, RC)							\
	GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,						\
			GTMCRYPT_OP_DECRYPT, GTMCRYPT_IV_RESET, null_iv, RC)

/* Encrypt data with the IV set to the specified value prior to the operation. */
#define GTMCRYPT_ENCRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, IV_ADDR, IV_LEN, RC)				\
{																\
	gtm_string_t iv_macro;													\
																\
	assert(IV_LEN <= GTM_MAX_IV_LEN);											\
	iv_macro.address = (gtm_char_t *)IV_ADDR;										\
	iv_macro.length = (gtm_long_t)IV_LEN;											\
	GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,						\
			GTMCRYPT_OP_ENCRYPT, GTMCRYPT_IV_SET, iv_macro, RC);							\
}

/* Decrypt data with the IV set to the specified value prior to the operation. */
#define GTMCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, IV_ADDR, IV_LEN, RC)				\
{																\
	gtm_string_t iv_macro;													\
																\
	assert(IV_LEN <= GTM_MAX_IV_LEN);											\
	iv_macro.address = (gtm_char_t *)IV_ADDR;										\
	iv_macro.length = (gtm_long_t)IV_LEN;											\
	GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,						\
			GTMCRYPT_OP_DECRYPT, GTMCRYPT_IV_SET, iv_macro, RC);							\
}

/* Encrypt data without touching the IV prior to the operation. */
#define GTMCRYPT_ENCRYPT_CONT_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, RC)							\
	GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,						\
			GTMCRYPT_OP_ENCRYPT, GTMCRYPT_IV_CONTINUE, null_iv, RC)	/* Use of null_iv as argument is irrelevant. */

/* Decrypt data without touching the IV prior to the operation. */
#define GTMCRYPT_DECRYPT_CONT_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, RC)							\
	GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF,						\
			GTMCRYPT_OP_DECRYPT, GTMCRYPT_IV_CONTINUE, null_iv, RC) /* Use of null_iv as argument is irrelevant. */

/* Encrypt or decrypt data with the IV optionally set to a specified, or reset to the initial, value prior to the operation. */
#ifdef GTM_CRYPT_ENCRYPT_DECRYPT_LOG
#  define GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, OPERATION, IV_MODE, IV, RC)		\
{																\
	int		i;													\
	unsigned char	c;													\
	intrpt_state_t	prev_intrpt_state;											\
																\
	assert(INBUF);														\
	if (gtmcrypt_initialized && (GTMCRYPT_INVALID_KEY_HANDLE != KEY_HANDLE))						\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
		FPRINTF(stderr, (OPERATION == GTMCRYPT_OP_ENCRYPT ? "Going to ENCRYPT\n" : "Going to DECRYPT\n"));		\
		for (i = 0; i < INBUF_LEN; i++)											\
		{														\
			c = ((char *)INBUF)[i];											\
			FPRINTF(stderr, "%c(%02x) ", 31 < c && 127 > c ? c : '.', c);						\
		}														\
		FPRINTF(stderr, "\n  IV:\n  ");											\
		for (i = 0; i < (IV).length; i++)										\
		{														\
			c = ((char *)(IV).address)[i];										\
			FPRINTF(stderr, "%c(%02x) ", 31 < c && 127 > c ? c : '.', c);						\
		}														\
		FPRINTF(stderr, "\n");												\
		FFLUSH(stderr);													\
		if (0 == gtmcrypt_encrypt_decrypt(KEY_HANDLE, (char *)(INBUF), INBUF_LEN, (char *)(OUTBUF),			\
				OPERATION, IV_MODE, IV))									\
		{														\
			FPRINTF(stderr, "  Result:\n  ");									\
			for (i = 0; i < INBUF_LEN; i++)										\
			{													\
				c = ((OUTBUF == NULL) ? (char *)INBUF : (char *)(OUTBUF))[i];					\
				FPRINTF(stderr, "%c(%02x) ", 31 < c && 127 > c ? c : '.', c);					\
			}													\
			FPRINTF(stderr, "\n---------------------------------------------------------\n");			\
			FFLUSH(stderr);												\
			RC = 0;													\
		} else														\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED);								\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
	} else															\
	{															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
		DEBUG_ONLY(CORE_ON_CRYPTOPFAILED);										\
	}															\
}
#else
#  define GTMCRYPT_ENCRYPT_DECRYPT_WITH_IV(CSA, KEY_HANDLE, INBUF, INBUF_LEN, OUTBUF, OPERATION, IV_MODE, IV, RC)		\
{																\
	intrpt_state_t	prev_intrpt_state;											\
	char		*gcedwiv_inbuf = (char *)(INBUF);									\
																\
	assert(gcedwiv_inbuf);													\
	if (gtmcrypt_initialized && (GTMCRYPT_INVALID_KEY_HANDLE != KEY_HANDLE))						\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
		if (0 == gtmcrypt_encrypt_decrypt(KEY_HANDLE, gcedwiv_inbuf, INBUF_LEN, (char *)(OUTBUF),			\
				OPERATION, IV_MODE, IV))									\
			RC = 0;													\
		else														\
		{														\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED);								\
			DEBUG_ONLY(CORE_ON_CRYPTOPFAILED);									\
		}														\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
	} else															\
	{															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
		DEBUG_ONLY(CORE_ON_CRYPTOPFAILED);										\
	}															\
}
#endif

/* Check whether the specified symmetric key handles belong to the same key. */
#define GTMCRYPT_SAME_KEY(KEY_HANDLE1, KEY_HANDLE2)										\
	gtmcrypt_same_key(KEY_HANDLE1, KEY_HANDLE2)

/* Shut down the encryption for this process. */
#define GTMCRYPT_CLOSE														\
{																\
	intrpt_state_t prev_intrpt_state;											\
																\
	if (gtmcrypt_initialized)												\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
		gtmcrypt_close();												\
		gtmcrypt_initialized = FALSE;											\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION, prev_intrpt_state);							\
	}															\
}

uint4 gtmcrypt_entry(void);
boolean_t verify_lib_loadpath(const char *libname, char *loadpath);

#endif /* GTMCRYPT_H */
