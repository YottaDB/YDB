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

#include <signal.h>
#include <rtnhdr.h>
#include "stack_frame.h"
#include "gtmxc_types.h"
#include "gtmcrypt_interface.h"
#include "gtmimagename.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "have_crit.h"
#include "deferred_signal_handler.h"
#include "gtmmsg.h"
#include "gtmci.h"
#include "wbox_test_init.h"
#include "error.h"			/* for MAKE_MSG_WARNING macro */

#define MAX_GTMCRYPT_ERR_STRLEN		2048		/* Should be kept in sync with the one in gtmcrypt_ref.h */

typedef xc_status_t			(*gtmcrypt_init_t)(int);
typedef xc_status_t			(*gtmcrypt_close_t)();
typedef xc_status_t			(*gtmcrypt_hash_gen_t)(gtmcrypt_key_t, xc_string_t *);
typedef xc_status_t			(*gtmcrypt_encrypt_t)(gtmcrypt_key_t, xc_string_t *, xc_string_t *);
typedef xc_status_t			(*gtmcrypt_decrypt_t)(gtmcrypt_key_t, xc_string_t *, xc_string_t *);
typedef xc_status_t			(*gtmcrypt_getkey_by_name_t)(xc_string_t *, gtmcrypt_key_t *);
typedef xc_status_t			(*gtmcrypt_getkey_by_hash_t)(xc_string_t *, gtmcrypt_key_t *);
typedef char*				(*gtmcrypt_strerror_t)();

GBLREF	gtmcrypt_init_t			gtmcrypt_init_fnptr;
GBLREF	gtmcrypt_close_t		gtmcrypt_close_fnptr;
GBLREF	gtmcrypt_hash_gen_t		gtmcrypt_hash_gen_fnptr;
GBLREF	gtmcrypt_encrypt_t		gtmcrypt_encrypt_fnptr;
GBLREF	gtmcrypt_decrypt_t		gtmcrypt_decrypt_fnptr;
GBLREF	gtmcrypt_getkey_by_name_t 	gtmcrypt_getkey_by_name_fnptr;
GBLREF	gtmcrypt_getkey_by_hash_t 	gtmcrypt_getkey_by_hash_fnptr;
GBLREF	gtmcrypt_strerror_t		gtmcrypt_strerror_fnptr;
GBLREF	boolean_t			gtmcrypt_initialized;
LITREF	char				gtmcrypt_repeat_msg[];

error_def(ERR_CRYPTDLNOOPEN);
error_def(ERR_CRYPTDLNOOPEN2);
error_def(ERR_CRYPTHASHGENFAILED);
error_def(ERR_CRYPTINIT);
error_def(ERR_CRYPTKEYFETCHFAILED);
error_def(ERR_CRYPTKEYFETCHFAILEDNF);
error_def(ERR_CRYPTNOPSWDINTP);
error_def(ERR_CRYPTOPFAILED);

/* The standard shared library suffix for HPUX on HPPA is .sl.
 * On HPUX/IA64, the standard suffix was changed to .so (to match other Unixes) but for
 * the sake of compatibility, they still accept (and look for) .sl if .so is not present.
 * Nevertheless, we use the standard suffix on all platforms.
 */
#if (defined(__hpux) && defined(__hppa))
#	define	GTMCRYPT_LIBNAME	"libgtmcrypt.sl"
#elif defined(__MVS__)
#	define	GTMCRYPT_LIBNAME        "libgtmcrypt.dll"
#else
#	define	GTMCRYPT_LIBNAME	"libgtmcrypt.so"
#endif

#define	GTMCRYPT_LIBFLAGS		(RTLD_NOW | RTLD_GLOBAL)

#define	GTMCRYPT_INIT_FNAME		"gtmcrypt_init"
#define	GTMCRYPT_CLOSE_FNAME		"gtmcrypt_close"
#define	GTMCRYPT_HASH_GEN_FNAME		"gtmcrypt_hash_gen"
#define	GTMCRYPT_ENCRYPT_FNAME		"gtmcrypt_encrypt"
#define	GTMCRYPT_DECRYPT_FNAME		"gtmcrypt_decrypt"
#define GTMCRYPT_GETKEY_BY_NAME		"gtmcrypt_getkey_by_name"
#define GTMCRYPT_GETKEY_BY_HASH		"gtmcrypt_getkey_by_hash"
#define	GTMCRYPT_STRERROR		"gtmcrypt_strerror"

#define	GTM_PASSWD			"gtm_passwd"

#define GTMCRYPT_INVALID_KEY_HANDLE		-1		/* Should be kept in sync with INVALID_HANDLE in gtmcrypt_ref.h */

uint4 gtmcrypt_entry(void);

/* =====================================================================================================*/
/* 					Error Reporting Macros						*/
/* =====================================================================================================*/

#define CRYPTERR_MASK				0x10000000
#define REPEAT_MSG_MASK				0x20000000

#define IS_CRYPTERR_MASK(ERRID)			((ERRID) & CRYPTERR_MASK)
#define IS_REPEAT_MSG_MASK(ERRID)		((ERRID) & REPEAT_MSG_MASK)
#define SET_CRYPTERR_MASK(ERRID)		((ERRID) | CRYPTERR_MASK)
#define SET_REPEAT_MSG_MASK(ERRID)		((ERRID) | REPEAT_MSG_MASK)
#define CLEAR_CRYPTERR_MASK(ERRID)		(ERRID = ((ERRID) & ~CRYPTERR_MASK))
#define CLEAR_REPEAT_MSG_MASK(ERRID)		(ERRID = ((ERRID) & ~REPEAT_MSG_MASK))

#define GTMCRYPT_REPORT_ERROR(ERRID, MECHANISM, LEN, PTR)									\
{																\
	GBLREF	char		dl_err[];											\
																\
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
		errptr = (const char *) (*gtmcrypt_strerror_fnptr)();								\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	}															\
	CLEAR_REPEAT_MSG_MASK(errid);												\
	MECHANISM(VARLSTCNT(6) errid, 4, LEN, PTR, LEN_AND_STR(errptr));							\
}


/* =====================================================================================================*/
/* 					Utility Macros							*/
/* =====================================================================================================*/

/* Helper macro to package the address and length in a xc_string_t type */
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

#define BLOCK_REQUIRE_ENCRYPTION(FLAG, LEVL, BSIZ) 	(FLAG && IS_BLK_ENCRYPTED(LEVL, BSIZ))

#define ENCR_INITIALIZED 				gtmcrypt_initialized
#define ENCR_WBOX_ENABLED				(gtm_white_box_test_case_enabled 					\
				 				&& (WBTEST_ENCRYPT_INIT_ERROR == gtm_white_box_test_case_number))

#define ASSERT_ENCRYPTION_INITIALIZED			assert(ENCR_INITIALIZED || ENCR_WBOX_ENABLED)

#define PROMPT_PASSWD(PTR)				(IS_MUMPS_IMAGE								\
								&& (NULL != (PTR = (char *)getenv(GTM_PASSWD)))			\
								&& (0 == strlen(PTR)))						\


#define GTMCRYPT_COPY_HASH(SRC, DST)												\
{																\
	memcpy(DST->encryption_hash, SRC->encryption_hash, GTMCRYPT_HASH_LEN);							\
	DST->is_encrypted = SRC->is_encrypted;											\
}																\

/* General Note : All macros below (execpt GTMCRYPT_CLOSE) takes in CSA as their first parameter. Currently, most macros don't
 * use CSA, but is supplied by the caller anyways in case later a need to arises to reference CSA.
 */
#define	ALLOC_BUFF_GET_ENCR_KEY(CSA, HASH, ALLOC_SIZE, RC)									\
{																\
	assert((0 < ALLOC_SIZE) && (NULL != CSA));										\
	RC = 0;															\
	GTMCRYPT_GETKEY(CSA, HASH, CSA->encr_key_handle, RC);									\
	if (0 == RC)														\
		(CSA)->encrypted_blk_contents = (char *)malloc(ALLOC_SIZE); 							\
}

/* Database specific initialization - gets the encryption key corresponding to the HASH (SHA-512 currently) found in the database
 * file header and allocates a buffer large enough to encrypt/decrypt database block sizes.
 */
#define INIT_DB_ENCRYPTION(CSA, CSD, RC)											\
{																\
	GBLREF stack_frame		*frame_pointer;										\
	GBLREF uint4			dollar_tlevel;										\
	char 				*ptr, *key_hash;									\
	boolean_t			call_ci_ret_code_quit, prompt_passwd;							\
																\
	RC = 0;															\
	prompt_passwd = PROMPT_PASSWD(ptr);											\
	if (prompt_passwd && dollar_tlevel)											\
	{	/* GT.M call-ins don't support TP transactions (see gtm_ci_exec) */						\
		rts_error_csa(CSA_ARG(CSA) VARLSTCNT(1) ERR_CRYPTNOPSWDINTP); 							\
	}															\
	assert(!IS_MUMPS_IMAGE || !(frame_pointer->flags & SFF_CI)); /* ensures not already in gtm_ci */			\
	/* ALLOC_BUFF_GET_ENCR_KEY eventually invokes gtmcrypt_getkey_by_hash to obtain the encryption key for the concerned	\
	 * database. At this point encryption initialization is already done (as part of INIT_PROC_ENCRYPTION) even if the	\
	 * user had entered a wrong password. But, the wrong password isn't validated until the actual encryption/decryption	\
	 * happens (which is now). If user fixes the wrong-password situation by setting gtm_passwd to null-string and once	\
	 * again accesses the database, we'd come back here; but this time, gtmcrypt_getkey_by_hash will end up invoking	\
	 * gtm_ci (to obtain the password) and so a corresponding ci_ret_code_quit must be done. Make a note of this so we	\
	 * can accordingly invoke ci_ret_code_quit (after gtmcrypt_getkey_by_hash)						\
	 */															\
	call_ci_ret_code_quit = (prompt_passwd && !(frame_pointer->flags & SFF_CI));						\
	key_hash = CSD->encryption_hash;											\
	ALLOC_BUFF_GET_ENCR_KEY(CSA, key_hash, (CSD->blk_size + SIZEOF(int4)), RC);						\
	if (call_ci_ret_code_quit)												\
		ci_ret_code_quit();												\
}

/* =====================================================================================================*/
/* 					Plugin Related Macros						*/
/* =====================================================================================================*/

/* Process specific initialization - dlopen libgtmcrypt.so and invoke gtmcrypt_init() */
#define INIT_PROC_ENCRYPTION(CSA, RC)												\
{																\
	GBLREF stack_frame		*frame_pointer;										\
	GBLREF uint4			dollar_tlevel;										\
	boolean_t			call_ci_ret_code_quit, prompt_passwd;							\
	char 				*ptr;											\
																\
	RC = 0;															\
	if (!gtmcrypt_initialized)												\
	{															\
		if (0 == (RC = gtmcrypt_entry()))										\
		{	/* dlopen succeeded */											\
			assert(NULL != gtmcrypt_init_fnptr);									\
			assert(NULL != gtmcrypt_getkey_by_hash_fnptr);								\
			assert(NULL != gtmcrypt_getkey_by_name_fnptr);								\
			assert(NULL != gtmcrypt_hash_gen_fnptr);								\
			assert(NULL != gtmcrypt_encrypt_fnptr); 								\
			assert(NULL != gtmcrypt_decrypt_fnptr); 								\
			assert(NULL != gtmcrypt_strerror_fnptr);								\
			assert(NULL != gtmcrypt_close_fnptr);									\
			prompt_passwd = PROMPT_PASSWD(ptr);									\
			if (prompt_passwd && dollar_tlevel)									\
			{	/* GT.M call-ins don't support TP transactions (see gtm_ci_exec)*/				\
				rts_error_csa(CSA_ARG(CSA) VARLSTCNT(1) ERR_CRYPTNOPSWDINTP);					\
			}													\
			assert(!IS_MUMPS_IMAGE || !(frame_pointer->flags & SFF_CI)); /* ensures not already in gtm_ci */	\
			/* If password is set to empty string, gtmcrypt_init (called below) invokes gtm_ci to obtain the	\
			 * password at runtime (if we are MUMPS) in which case ci_ret_code_quit() needs to be invoked. Make	\
			 * a note of this so we can accordingly invoke ci_ret_code_quit (after gtmcrypt_init)			\
			 */													\
			call_ci_ret_code_quit = (prompt_passwd && !(frame_pointer->flags & SFF_CI));				\
			/* IS_MUMPS_IMAGE below tells the plugin to prompt for password (if not already provided in env) */	\
			DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);								\
			xc_status_t init_ret_status = (*gtmcrypt_init_fnptr)(IS_MUMPS_IMAGE);					\
			ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);								\
			if (call_ci_ret_code_quit)										\
				ci_ret_code_quit();	/* Unwind stack frames */						\
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
		if (0 != (status = (*gtmcrypt_getkey_by_hash_fnptr)(&xc_hash, &key_handle)))					\
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
		if (0 == (status = (*gtmcrypt_getkey_by_name_fnptr)(&xc_filename, &handle)))					\
		{														\
			if (0 == (status = (*gtmcrypt_hash_gen_fnptr)(handle, &xc_hash)))					\
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
		if (0 == (status = (*gtmcrypt_encrypt_fnptr)(key_handle, &unencrypted_block, &encrypted_block)))		\
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
		if (0 == (status = (*gtmcrypt_decrypt_fnptr)(key_handle, &encrypted_block, &unencrypted_block)))		\
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
		(*gtmcrypt_close_fnptr)();											\
		gtmcrypt_initialized = FALSE;											\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
	}															\
}

#endif /* GTMCRYPT_H */
