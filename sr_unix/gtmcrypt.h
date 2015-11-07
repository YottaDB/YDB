/****************************************************************
 *								*
 *	Copyright 2009, 2013 Fidelity Information Services, Inc *
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
#include "error.h"			/* for MAKE_MSG_WARNING macro */

#define	gtmcrypt_init			(*gtmcrypt_init_fnptr)
#define	gtmcrypt_close			(*gtmcrypt_close_fnptr)
#define	gtmcrypt_hash_gen		(*gtmcrypt_hash_gen_fnptr)
#define	gtmcrypt_encrypt		(*gtmcrypt_encrypt_fnptr)
#define	gtmcrypt_decrypt		(*gtmcrypt_decrypt_fnptr)
#define	gtmcrypt_getkey_by_name 	(*gtmcrypt_getkey_by_name_fnptr)
#define	gtmcrypt_getkey_by_hash 	(*gtmcrypt_getkey_by_hash_fnptr)
#define	gtmcrypt_strerror		(*gtmcrypt_strerror_fnptr)

/* It's important that the "gtmcrypt_interface.h" include should be *after* the above macro definitions. This way, the function
 * prototypes defined in the header file will automatically be expanded to function pointers saving us the trouble of explicitly
 * defining them once again.
 */
#include "gtmcrypt_interface.h"

GBLREF	boolean_t			gtmcrypt_initialized;
GBLREF	mstr				pvt_crypt_buf;
GBLREF	char				dl_err[];

LITREF	char				gtmcrypt_repeat_msg[];

error_def(ERR_CRYPTDLNOOPEN);
error_def(ERR_CRYPTDLNOOPEN2);
error_def(ERR_CRYPTHASHGENFAILED);
error_def(ERR_CRYPTINIT);
error_def(ERR_CRYPTKEYFETCHFAILED);
error_def(ERR_CRYPTOPFAILED);

/* =====================================================================================================*
 * 					Error Reporting Macros						*
 * =====================================================================================================*/

#define CRYPTERR_MASK				0x10000000
#define REPEAT_MSG_MASK				0x20000000

#define IS_CRYPTERR_MASK(ERRID)			((ERRID) & CRYPTERR_MASK)
#define IS_REPEAT_MSG_MASK(ERRID)		((ERRID) & REPEAT_MSG_MASK)
#define SET_CRYPTERR_MASK(ERRID)		((ERRID) | CRYPTERR_MASK)
#define SET_REPEAT_MSG_MASK(ERRID)		((ERRID) | REPEAT_MSG_MASK)
#define CLEAR_CRYPTERR_MASK(ERRID)		(ERRID = ((ERRID) & ~CRYPTERR_MASK))
#define CLEAR_REPEAT_MSG_MASK(ERRID)		(ERRID = ((ERRID) & ~REPEAT_MSG_MASK))

#define REALLOC_CRYPTBUF_IF_NEEDED(LEN)					\
{									\
	if (!pvt_crypt_buf.addr || (pvt_crypt_buf.len < LEN))		\
	{								\
		if (pvt_crypt_buf.addr)					\
			free(pvt_crypt_buf.addr);			\
		pvt_crypt_buf.addr = (char *)malloc(LEN);		\
		pvt_crypt_buf.len = LEN;				\
	}								\
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
	{															\
		errptr = (const char *) &dl_err[0];										\
	} else															\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		errptr = (const char *) gtmcrypt_strerror();									\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	}															\
	CLEAR_REPEAT_MSG_MASK(errid);												\
	MECHANISM(VARLSTCNT(6) errid, 4, LEN, PTR, LEN_AND_STR(errptr));							\
}


/* =====================================================================================================*/
/* 					Utility Macros							*/
/* =====================================================================================================*/

/* Helper macro to package the address and length in a gtm_string_t type */
#define PACKAGE_XCSTRING(xcstring, buf, buflen)											\
{																\
	xcstring.address = buf;													\
	xcstring.length = buflen;												\
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

/* General Note : All macros below (except GTMCRYPT_CLOSE) takes CSA as their first parameter. Currently, most macros don't
 * use CSA, but is supplied by the caller anyways in case a need arises, in the future, to reference CSA.
 */

/* Database specific initialization - gets the encryption key corresponding to the HASH (SHA-512 currently) found in the database
 * file header and allocates a buffer large enough to encrypt/decrypt database block sizes.
 */
#define INIT_DB_ENCRYPTION(CSA, CSD, RC)											\
{																\
	RC = 0;															\
	GTMCRYPT_GETKEY(CSA, CSD->encryption_hash, CSA->encr_key_handle, RC);							\
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
			gtm_status_t init_ret_status = gtmcrypt_init(IS_INTERACTIVE_MODE ? GTMCRYPT_OP_INTERACTIVE_MODE : 0);	\
			ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);								\
			if (0 != init_ret_status)										\
				RC = SET_CRYPTERR_MASK(ERR_CRYPTINIT);								\
			else													\
				gtmcrypt_initialized = TRUE; /* No more per-process initialization needed */			\
		} else														\
			RC = SET_CRYPTERR_MASK(RC);										\
	}															\
}

/* Given a cryptographic hash (currently SHA-512), the below function retrieves a handle to the symmetric key corresponding to
 * the hash. This function is always called before attempting an encrypt or decrypt operation.
 */
#define GTMCRYPT_GETKEY(CSA, hash, key_handle, RC)										\
{																\
	xc_string_t		xc_hash;											\
	xc_status_t		status;												\
																\
	key_handle = GTMCRYPT_INVALID_KEY_HANDLE;										\
	if (gtmcrypt_initialized)												\
	{															\
		PACKAGE_XCSTRING(xc_hash, hash, GTMCRYPT_HASH_LEN);								\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		if (0 != (status = gtmcrypt_getkey_by_hash(&xc_hash, &key_handle)))						\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTKEYFETCHFAILED);							\
		else														\
			RC = 0;													\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	} else															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
}

#define GTMCRYPT_HASH_CHK(CSA, hash, RC)											\
{																\
	gtmcrypt_key_t		handle;												\
																\
	GTMCRYPT_GETKEY(CSA, hash, handle, RC);											\
}

#define GTMCRYPT_HASH_GEN(CSA, filename, filename_len, hash, RC)								\
{																\
	xc_status_t		status;												\
	xc_string_t		xc_filename, xc_hash;										\
	gtmcrypt_key_t		handle;												\
																\
	if (gtmcrypt_initialized)												\
	{															\
		PACKAGE_XCSTRING(xc_filename, filename, filename_len);								\
		PACKAGE_XCSTRING(xc_hash, hash, GTMCRYPT_HASH_LEN);								\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		if (0 == (status = gtmcrypt_getkey_by_name(&xc_filename, &handle)))						\
		{														\
			if (0 == (status = gtmcrypt_hash_gen(handle, &xc_hash)))						\
			{													\
				memcpy(hash, xc_hash.address, GTMCRYPT_HASH_LEN);						\
				RC = 0;												\
			} else													\
				RC = SET_CRYPTERR_MASK(ERR_CRYPTHASHGENFAILED);							\
		} else														\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTKEYFETCHFAILED);							\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	} else															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
}

#define GTMCRYPT_ENCRYPT(CSA, key_handle, inbuf, inbuf_len, outbuf, RC)								\
{																\
	xc_string_t		unencrypted_block, encrypted_block;								\
	xc_status_t		status;												\
																\
	if (gtmcrypt_initialized && (GTMCRYPT_INVALID_KEY_HANDLE != key_handle))						\
	{															\
		PACKAGE_XCSTRING(unencrypted_block, inbuf, inbuf_len);								\
		PACKAGE_XCSTRING(encrypted_block, outbuf, inbuf_len);								\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		if (0 == (status = gtmcrypt_encrypt(key_handle, &unencrypted_block, &encrypted_block)))				\
			RC = 0;													\
		else														\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED);								\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	} else															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
}

#define GTMCRYPT_DECRYPT(CSA, key_handle, inbuf, inbuf_len, outbuf, RC)								\
{																\
	xc_string_t		unencrypted_block, encrypted_block;								\
	xc_status_t		status;												\
																\
	if (gtmcrypt_initialized && (GTMCRYPT_INVALID_KEY_HANDLE != key_handle))						\
	{															\
		PACKAGE_XCSTRING(encrypted_block, inbuf, inbuf_len);								\
		PACKAGE_XCSTRING(unencrypted_block, outbuf, inbuf_len);								\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		if (0 == (status = gtmcrypt_decrypt(key_handle, &encrypted_block, &unencrypted_block)))				\
			RC = 0;													\
		else														\
			RC = SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED);								\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	} else															\
		RC = SET_REPEAT_MSG_MASK((SET_CRYPTERR_MASK(ERR_CRYPTOPFAILED)));						\
}

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
