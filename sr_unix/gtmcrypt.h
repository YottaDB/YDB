/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc 	*
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
#include "rtnhdr.h"
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

typedef xc_status_t	(*gtmcrypt_init_t)(int);
typedef xc_status_t	(*gtmcrypt_close_t)();
typedef xc_status_t	(*gtmcrypt_hash_gen_t)(gtmcrypt_key_t, xc_string_t *);
typedef xc_status_t	(*gtmcrypt_encode_t)(gtmcrypt_key_t, xc_string_t *, xc_string_t *);
typedef xc_status_t	(*gtmcrypt_decode_t)(gtmcrypt_key_t, xc_string_t *, xc_string_t *);
typedef xc_status_t	(*gtmcrypt_getkey_by_name_t)(xc_string_t *, gtmcrypt_key_t *);
typedef xc_status_t	(*gtmcrypt_getkey_by_hash_t)(xc_string_t *, gtmcrypt_key_t *);
typedef char*		(*gtmcrypt_strerror_t)();

GBLREF	gtmcrypt_init_t			gtmcrypt_init_fnptr;
GBLREF	gtmcrypt_close_t		gtmcrypt_close_fnptr;
GBLREF	gtmcrypt_hash_gen_t		gtmcrypt_hash_gen_fnptr;
GBLREF	gtmcrypt_encode_t		gtmcrypt_encode_fnptr;
GBLREF	gtmcrypt_decode_t		gtmcrypt_decode_fnptr;
GBLREF	gtmcrypt_getkey_by_name_t 	gtmcrypt_getkey_by_name_fnptr;
GBLREF	gtmcrypt_getkey_by_hash_t 	gtmcrypt_getkey_by_hash_fnptr;
GBLREF	gtmcrypt_strerror_t		gtmcrypt_strerror_fnptr;

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
#define	GTMCRYPT_ENCODE_FNAME		"gtmcrypt_encode"
#define	GTMCRYPT_DECODE_FNAME		"gtmcrypt_decode"
#define GTMCRYPT_GETKEY_BY_NAME		"gtmcrypt_getkey_by_name"
#define GTMCRYPT_GETKEY_BY_HASH		"gtmcrypt_getkey_by_hash"
#define	GTMCRYPT_STRERROR		"gtmcrypt_strerror"

#define	GTM_PASSWD			"gtm_passwd"
/* Global variable GBLDEF'ed in gbldefs.c */
GBLREF	int4	gtmcrypt_init_state;
/* Possible states for encryption library */
typedef enum
{
	GTMCRYPT_UNINITIALIZED,		/* This is when, so far in the code, gtmcrypt_setup or gtmcrypt_init hasn't been called */
	GTMCRYPT_INITIALIZED,		/* This is the state when both dlopen and gtmcrypt_init has successfully passed */
} gtmcrypt_init_values;

void gtmcrypt_entry(void);

/* =====================================================================================================*/
/* 					Error Reporting Macros						*/
/* =====================================================================================================*/

/* Call the plugin's strerror equivalent to get the last error message */
#define GC_GET_ERR_STRING(err)				\
{							\
	assert(NULL != gtmcrypt_strerror_fnptr);	\
	err = (*gtmcrypt_strerror_fnptr)();		\
}

/* ERR_CRYPTKEYFETCHFAILED is a unique error that has to be handled a bit differently. Whenever GT.M calls the plugin
 * to get the encryption key for a give hash, the plugin is not aware of the filename for which the hash was
 * passed. Also, GT.M itself might not be aware of the filenames in cases like bin_load. To handle this case,
 * GC_GTM_PUTMSG and GC_RTS_ERROR will explicitly carry the filename for which the current operation
 * is being done. In case GT.M cannot figure out the filename, it will pass NULL. The following constructs the
 * appropriate error message in the event that the filename is present and for the case where GT.M has passed
 * NULL. */

#define GC_GTM_PUTMSG(err_id, FNAME)												\
{																\
	char 		*err;													\
	GBLREF char	dl_err[];												\
																\
	error_def(ERR_CRYPTKEYFETCHFAILED);											\
	error_def(ERR_CRYPTKEYFETCHFAILEDNF);											\
	error_def(ERR_CRYPTINIT);												\
	error_def(ERR_CRYPTDLNOOPEN);												\
																\
	if (ERR_CRYPTDLNOOPEN != err_id)											\
	{															\
		GC_GET_ERR_STRING(err);												\
		if (ERR_CRYPTKEYFETCHFAILED == err_id)										\
		{														\
			if (NULL != FNAME)											\
				gtm_putmsg(VARLSTCNT(6) ERR_CRYPTKEYFETCHFAILED, 4, LEN_AND_STR(FNAME), LEN_AND_STR(err));	\
			else													\
				gtm_putmsg(VARLSTCNT(4) ERR_CRYPTKEYFETCHFAILEDNF, 2, LEN_AND_STR(err));			\
		} else 														\
			gtm_putmsg(VARLSTCNT(4) err_id, 2, LEN_AND_STR(err)); 							\
	} else															\
		gtm_putmsg(VARLSTCNT(4) ERR_CRYPTDLNOOPEN, 2, LEN_AND_STR(dl_err));						\
}

/* Fetch the last error string from the encryption plugin. If the error is ERR_KEYFETCHFAILED, we need to
 * handle them a bit differently as described above. This macro should be called whenever GT.M wants to do
 * rts_error. */
#define GC_RTS_ERROR(err_id, FNAME)												\
{																\
	char 		*err;													\
	GBLREF char	dl_err[];												\
																\
	error_def(ERR_CRYPTKEYFETCHFAILED);											\
	error_def(ERR_CRYPTKEYFETCHFAILEDNF);											\
	error_def(ERR_CRYPTINIT);												\
	error_def(ERR_CRYPTDLNOOPEN);												\
																\
	if (ERR_CRYPTDLNOOPEN != err_id)											\
	{															\
		GC_GET_ERR_STRING(err);												\
		if (ERR_CRYPTKEYFETCHFAILED == err_id)										\
		{														\
			if (NULL != FNAME)											\
				rts_error(VARLSTCNT(6) ERR_CRYPTKEYFETCHFAILED, 4, LEN_AND_STR(FNAME), LEN_AND_STR(err));	\
			else													\
				rts_error(VARLSTCNT(4) ERR_CRYPTKEYFETCHFAILEDNF, 2, LEN_AND_STR(err));				\
		} else 														\
			rts_error(VARLSTCNT(4) err_id, 2, LEN_AND_STR(err)); 							\
	} else															\
		rts_error(VARLSTCNT(4) ERR_CRYPTDLNOOPEN, 2, LEN_AND_STR(dl_err));						\
}

/* Following are the error identifiers visible to the GT.M user. Depending on the operation performed,
 * mark the appropriate status. */
#define GC_MARK_STATUS(status, RC, ERR_ID) 											\
{																\
	error_def(ERR_CRYPTDLNOOPEN);												\
	error_def(ERR_CRYPTINIT);												\
	error_def(ERR_CRYPTKEYFETCHFAILED);											\
	error_def(ERR_CRYPTOPFAILED);												\
	error_def(ERR_CRYPTHASHGENFAILED);											\
																\
	RC = (0 != status) ? ERR_ID : 0;											\
}

/* =====================================================================================================*/
/* 					Utility Macros							*/
/* =====================================================================================================*/

/* Package the required addresses in various xc_string_t which can be used by various decode, encode macros */
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

#define ENCR_INITIALIZED 		(GTMCRYPT_INITIALIZED == gtmcrypt_init_state)
#define ENCR_WBOX_ENABLED		(gtm_white_box_test_case_enabled 							\
				 			&& (WBTEST_ENCRYPT_INIT_ERROR == gtm_white_box_test_case_number))

#define ASSERT_ENCRYPTION_INITIALIZED			assert(ENCR_INITIALIZED || ENCR_WBOX_ENABLED)

#define GTMCRYPT_COPY_HASH(src, dst)												\
{																\
	memcpy(dst->encryption_hash, src->encryption_hash, GTMCRYPT_HASH_LEN);							\
	dst->is_encrypted = src->is_encrypted;											\
}																\

#define	ALLOC_BUFF_GET_ENCR_KEY(CSA, HASH, ALLOC_SIZE, RC)									\
{																\
	assert(0 < ALLOC_SIZE && NULL != CSA);											\
	RC = 0;															\
	GTMCRYPT_GETKEY(HASH, CSA->encr_key_handle, RC);									\
	if (0 == RC)														\
		(CSA)->encrypted_blk_contents = (char *)malloc(ALLOC_SIZE); 							\
}

#define INIT_DB_ENCRYPTION(fname, CSA, CSD, RC)											\
{																\
	GBLREF stack_frame		*frame_pointer;										\
	GBLREF uint4			dollar_tlevel;										\
	char 				*ptr, *key_hash = CSD->encryption_hash;							\
	boolean_t			call_ci_ret_code_quit = FALSE, prompt_passwd = FALSE;					\
																\
	error_def(ERR_CRYPTNOPSWDINTP);												\
	RC = 0;															\
	/* If we are in a TP transaction and the environment is setup in such a way that we will be doing a gtm_ci call 	\
	 * then let's error out as gtm_ci doesn't work inside a TP transaction */						\
	prompt_passwd = PROMPT_PASSWD;												\
	if (prompt_passwd && dollar_tlevel)											\
		rts_error(VARLSTCNT(1) ERR_CRYPTNOPSWDINTP);									\
	/* Make sure we are not in gtm_ci already. */										\
	assert(!IS_MUMPS_IMAGE || !(frame_pointer->flags & SFF_CI));								\
	/* The below macro eventually calls gtmcrypt_getkey_by_hash to get the encryption key for the database based on 	\
	 * the hash in the database file header. It could be possible that initially the user had a wrong password and 		\
	 * tried accessing a global which might have landed in db_init and would have successfully done encryption 		\
	 * initialization. But later call to this macro would have failed since the password turned out to be wrong and 	\
	 * the encryption library failed to do the decryption. Now, after setting the password to a null string, the user	\
	 * now tries to access the global again which will reach this macro again but now the call to gtmcrypt_getkey_by_hash	\
	 * will call gtm_ci for prompting password. Hence before calling the encryption library, make a note if we had 		\
	 * to do ci_ret_code_quit below. */											\
	call_ci_ret_code_quit = (prompt_passwd && !(frame_pointer->flags & SFF_CI));						\
	ALLOC_BUFF_GET_ENCR_KEY(CSA, key_hash, (CSD->blk_size + SIZEOF(int4)), RC);						\
	if (call_ci_ret_code_quit)												\
		ci_ret_code_quit();												\
}

#define PROMPT_PASSWD			(IS_MUMPS_IMAGE										\
					 && (NULL != (ptr = (char *)getenv(GTM_PASSWD))) 					\
					 && (0 == strlen(ptr)))

/* =====================================================================================================*/
/* 					Plugin Related Macros						*/
/* =====================================================================================================*/

/* INIT_PROC_ENCRYPTION is called whenever GT.M wants to initialize the encryption library and it's related
 * modules. The context in which the caller calls INIT_PROC_ENCRYPTION might force non encryption related
 * tasks (like MUPIP JOURNAL -SHOW=HEADER -NOVERIFY -FORWARD) to error out in case the below macro fails. To
 * avoid this, we note down the error status and the corresponding error string in global variable. This way,
 * any task requiring the actual encryption (MUPIP INTEG -FILE) will verify if the global error code is holding
 * a non zero value and error out accordingly. */
#define INIT_PROC_ENCRYPTION(RC)												\
{																\
	GBLREF int4			gbl_encryption_ecode;									\
	GBLREF stack_frame		*frame_pointer;										\
	GBLREF uint4			dollar_tlevel;										\
	boolean_t			call_ci_ret_code_quit = FALSE, prompt_passwd = FALSE;					\
																\
	error_def(ERR_CRYPTNOPSWDINTP);												\
	error_def(ERR_CRYPTINIT);												\
	RC = 0;															\
	gbl_encryption_ecode = 0;												\
	if (GTMCRYPT_UNINITIALIZED == gtmcrypt_init_state)									\
	{															\
		gtmcrypt_entry();												\
		/* If in the above call, dlopen failed for some reason, then gbl_encryption_ecode will be set to		\
		 * ERR_CRYPTDLNOOPEN. Also, the dlopen error message will be stored in dl_err. */				\
		RC = gbl_encryption_ecode;											\
		if (0 == gbl_encryption_ecode)											\
		{														\
			char		*ptr;											\
			boolean_t	has_prompted_passwd;									\
																\
			/* dlopen on the encryption library succeeded. */							\
			assert(NULL != gtmcrypt_init_fnptr);									\
			/* If we are in a TP transaction and the environment is setup in such a way that we will be doing a 	\
			 * gtm_ci call then let's error out as gtm_ci doesn't work inside a TP transaction */			\
			prompt_passwd = PROMPT_PASSWD;										\
			if (prompt_passwd && dollar_tlevel)								\
				rts_error(VARLSTCNT(1) ERR_CRYPTNOPSWDINTP);							\
			/* Make sure we are not in gtm_ci already. */								\
			assert(!IS_MUMPS_IMAGE || !(frame_pointer->flags & SFF_CI));						\
			/* The call to gtmcrypt_init below will try to call gtm_ci on finding that password is set to 		\
			 * empty string and if the calling process is MUMPS. Make a note of the condition under which we	\
			 * would be calling ci_ret_code_quit. */								\
			call_ci_ret_code_quit = (prompt_passwd && !(frame_pointer->flags & SFF_CI));				\
			/* Call the encryption library's init routine. Also, pass a boolean indicating whether 			\
			 * the library should do the password prompting(for MUMPS) or not(for MUPIP, DSE, etc.)*/		\
			xc_status_t init_ret_status = (*gtmcrypt_init_fnptr)(IS_MUMPS_IMAGE);					\
			/* Unwind the stack frames if necessary. */								\
			if (call_ci_ret_code_quit)										\
				ci_ret_code_quit();										\
			/* If the call failed, we have to indicate the caller that an error happened. Also, we			\
			 * will mark the gbl_encryption_ecode to ERR_CRYPTINIT. */						\
			if (0 != init_ret_status)										\
				RC = gbl_encryption_ecode = ERR_CRYPTINIT;							\
			else													\
			{													\
				/* Everything went on smoothly. So indicate that we don't need to do the init			\
				 * again. */											\
				gtmcrypt_init_state = GTMCRYPT_INITIALIZED;							\
				gbl_encryption_ecode = RC = 0;									\
			}													\
		}														\
	}															\
}

/* GTMCRYPT_GETKEY is mostly called when the caller wants to get the encryption key well before the actual
 * encryption or decryption happens. As mentioned above the context in which GTMCRYPT_GETKEY is called
 * might force the non encryption related tasks to error out. So we follow a similar approach as mentioned above. */
#define GTMCRYPT_GETKEY(hash, key_handle, RC)											\
{																\
	xc_string_t	xc_hash;												\
	xc_status_t	status;													\
	GBLREF int4	gbl_encryption_ecode;											\
																\
	error_def(ERR_CRYPTKEYFETCHFAILED);											\
	/* Call the encryption library only if we are clean from the last encryption						\
	 * call. We find this from gbl_encryption_ecode. */									\
	RC = gbl_encryption_ecode;												\
	if (0 == RC)														\
	{															\
		PACKAGE_XCSTRING(xc_hash, hash, GTMCRYPT_HASH_LEN);								\
		assert(NULL != gtmcrypt_getkey_by_hash_fnptr);									\
		status = (*gtmcrypt_getkey_by_hash_fnptr)(&xc_hash, &key_handle);						\
		RC = gbl_encryption_ecode = (0 != status) ? ERR_CRYPTKEYFETCHFAILED : 0;					\
	}															\
}

#define GTMCRYPT_HASH_CHK(hash, RC)												\
{																\
	gtmcrypt_key_t		handle;												\
																\
	GTMCRYPT_GETKEY(hash, handle, RC);											\
}

/* DSE CHANGE -FILE -ENCRYPTION_HASH will call the GTMCRYPT_HASH_GEN  macro to reset the encryption hash in the database file
 * header. But, before doing so, it would have encountered an error in db_init but instead of reporting the error in db_init, it
 * stores the error code in gbl_encryption_ecode. But the GTMCRYPT_HASH_GEN macro cannot proceed if it finds that the error code is
 * non-zero. The below GTMCRYPT_RESET_ERR macro will reset this global error code back to zero so that GTMCRYPT_HASH_GEN can
 * proceed. However, we don't want to reset the error if the initialization failed because of a dlopen error as this would mean
 * that we won't have the function pointers to encryption library APIs properly initialized which would be used by GTMCRYPT_HASH_GEN
 * when we should ideally be reporting an error. */
#define GTMCRYPT_RESET_HASH_MISMATCH_ERR											\
{																\
	GBLREF int4		gbl_encryption_ecode;										\
																\
	error_def(ERR_CRYPTDLNOOPEN);												\
	if (0 != gbl_encryption_ecode && (ERR_CRYPTDLNOOPEN != gbl_encryption_ecode))						\
		gbl_encryption_ecode = 0;											\
}

#define GTMCRYPT_HASH_GEN(filename, filename_len, hash, RC)									\
{																\
	xc_status_t		status;												\
	xc_string_t		xc_filename, xc_hash;										\
	gtmcrypt_key_t		handle;												\
	GBLREF int4		gbl_encryption_ecode;										\
																\
	error_def(ERR_CRYPTDLNOOPEN);												\
	RC = gbl_encryption_ecode;												\
	if (0 == RC || (ERR_CRYPTDLNOOPEN != RC))										\
	{															\
		PACKAGE_XCSTRING(xc_filename, filename, filename_len);								\
		PACKAGE_XCSTRING(xc_hash, hash, GTMCRYPT_HASH_LEN);								\
		assert(NULL != gtmcrypt_getkey_by_name_fnptr);									\
		status = (*gtmcrypt_getkey_by_name_fnptr)(&xc_filename, &handle);						\
		GC_MARK_STATUS(status, RC, ERR_CRYPTKEYFETCHFAILED);								\
		if (0 == RC)													\
		{														\
			assert(NULL != gtmcrypt_hash_gen_fnptr);								\
			status = (*gtmcrypt_hash_gen_fnptr)(handle, &xc_hash);							\
			GC_MARK_STATUS(status, RC, ERR_CRYPTHASHGENFAILED);							\
			if (0 == RC)												\
				memcpy(hash, xc_hash.address, GTMCRYPT_HASH_LEN);						\
		}														\
	}															\
}

#define GTMCRYPT_ENCODE_FAST(key_handle, inbuf, inbuf_len, outbuf, RC)								\
{																\
	GBLREF  volatile int4   fast_lock_count;										\
	xc_string_t		unencrypted_block, encrypted_block;								\
	xc_status_t		status;												\
	int             	save_fast_lock_count;										\
	GBLREF int4		gbl_encryption_ecode;										\
																\
	RC = gbl_encryption_ecode;												\
	if (0 == RC)														\
	{															\
		PACKAGE_XCSTRING(unencrypted_block, inbuf, inbuf_len);								\
		PACKAGE_XCSTRING(encrypted_block, outbuf, inbuf_len);								\
		assert(NULL != gtmcrypt_encode_fnptr); 										\
		save_fast_lock_count = fast_lock_count;										\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		fast_lock_count++;												\
		status = (*gtmcrypt_encode_fnptr)(key_handle,									\
						&unencrypted_block,								\
						&encrypted_block);								\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		GC_MARK_STATUS(status, RC, ERR_CRYPTOPFAILED);									\
		fast_lock_count = save_fast_lock_count;										\
	}															\
}

#define GTMCRYPT_DECODE_FAST(key_handle, inbuf, inbuf_len, outbuf, RC)								\
{																\
	GBLREF  volatile int4   fast_lock_count;										\
	xc_string_t		unencrypted_block, encrypted_block;								\
	xc_status_t		status;												\
	int             	save_fast_lock_count;										\
	GBLREF int4		gbl_encryption_ecode;										\
																\
	RC = gbl_encryption_ecode;												\
	if (0 == RC)														\
	{															\
		PACKAGE_XCSTRING(encrypted_block, inbuf, inbuf_len);								\
		PACKAGE_XCSTRING(unencrypted_block, outbuf, inbuf_len);								\
		assert(NULL != gtmcrypt_decode_fnptr); 										\
		save_fast_lock_count = fast_lock_count;										\
		fast_lock_count++;												\
		DEFER_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		status = (*gtmcrypt_decode_fnptr)(key_handle,									\
						&encrypted_block,								\
						&unencrypted_block);								\
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_SECTION);									\
		GC_MARK_STATUS(status, RC, ERR_CRYPTOPFAILED);									\
		fast_lock_count = save_fast_lock_count;										\
	}															\
}

#define GTMCRYPT_CLOSE														\
{																\
	if (GTMCRYPT_INITIALIZED == gtmcrypt_init_state)									\
	{															\
		(*gtmcrypt_close_fnptr)();											\
		gtmcrypt_init_state = GTMCRYPT_UNINITIALIZED;									\
	}															\
}

#endif /* GTMCRYPT_H */
