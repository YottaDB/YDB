/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc 	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define _FILE_OFFSET_BITS	64	/* Needed to compile gpgme client progs also with large file support */

#include <stdio.h>			/* BYPASSOK -- Plugin doesn't have access to gtm_* header files */
#include <string.h>			/* BYPASSOK -- see above */
#include <unistd.h>			/* BYPASSOK -- see above */
#include <stdlib.h>			/* BYPASSOK -- see above */
#include <sys/stat.h>			/* BYPASSOK -- see above */
#include <dlfcn.h>
#include <assert.h>
#include <errno.h>
#include <gpgme.h>			/* gpgme functions */
#include <gpg-error.h>			/* gcry*_err_t */
#include "gtmxc_types.h"		/* xc_string, xc_status_t and other callin interfaces xc_fileid */
#include "gtmcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */

#include "gtmcrypt_ref.h"
#include "gtmcrypt_sym_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_pk_ref.h"

#ifdef __MVS__
#define GTMSHR_IMAGENAME	"libgtmshr.dll"
#endif

GBLDEF	int		gtmcrypt_inited;
GBLDEF	char		gtmcrypt_err_string[MAX_GTMCRYPT_ERR_STRLEN];

xc_status_t gc_init_interface(int prompt_passwd)
{ 	/* zOS is special when it comes to dynamic linking.
	 * (1). Building DLL with UNRESOLVED symbols
	 * =========================================
	 * Unlike other Unix platforms, on zOS DLL cannot be built having unresolved symbols and expecting them to get resolved
	 * by the loader.
	 * In this particular scenario we have symbols gtm_malloc, gtm_is_file_identical, gtm_free, gtm_filename_to_id and
	 * gtm_xcfileid_free that are part of mupip executable.
	 * As an workaround we are using function pointers to call into the interface functions so that we don't have an link-time
	 * errors.
	 * At runtime we do an dlopen with NULL which returns handle to global space and dlsym sets the function pointers to point
	 * to the correct functions at runtime.
	 *
	 * (2). DLSYM on symbols that are already resolved from another DLL
	 * ================================================================
	 * When mumps calls into libgtmcrypt it has above mentioned symbols already resolved from libgtmshr.dll.
	 * On zOS, when we try to DLSYM using the handle returned by DLOPEN(NULL,..), DLSYM crashes while trying to find symbols
	 * that are already loaded from another DLL(libgtmshr.dll).
	 * As an work around we dlopen libgtmshr.dll when called from MUMPS.
	 */
#	ifdef __MVS__
	void			*handle = NULL;
	const char		*gtm_dist, *dlerr_ptr;
	char 			gtmshr_file[GTM_PATH_MAX];
	int			dir_len;

	if (!(gtm_dist = getenv(GTM_DIST)))
		UPDATE_ERROR_STRING(ENV_UNDEF_ERROR, GTM_DIST);
	dir_len = STRLEN(gtm_dist);
	memcpy(&gtmshr_file[0], gtm_dist, dir_len);
	gtmshr_file[dir_len] = DIR_SEPARATOR;
	MEMCPY_LIT(&gtmshr_file[dir_len + 1], GTMSHR_IMAGENAME);
	gtmshr_file[dir_len + STR_LIT_LEN(GTMSHR_IMAGENAME) + 1] = '\0';
	/* prompt_passwd = TRUE implies plugin is invoked from MUMPS. We need to dlopen libgtmshr when invoked from MUMPS.
	 * Please refer comment (2) above.
	 */
	handle = dlopen(prompt_passwd ? gtmshr_file : NULL, GC_FLAGS);
	if (NULL == handle)
	{
		if (NULL == (dlerr_ptr = dlerror()))
		{
			UPDATE_ERROR_STRING("Unable to resolve GT.M interface functions. Unknown system error");
		} else
	 		UPDATE_ERROR_STRING("Unable to resolve GT.M interface functions. %s", dlerr_ptr);
		return GC_FAILURE;
	}
	DLSYM_ERR_AND_EXIT(gtm_is_file_identical_fptr_t, gtm_is_file_identical_fptr, GTM_IS_FILE_IDENTICAL_FUNC);
	DLSYM_ERR_AND_EXIT(gtm_malloc_fptr_t, gtm_malloc_fptr, GTM_MALLOC_FUNC);
	DLSYM_ERR_AND_EXIT(gtm_free_fptr_t, gtm_free_fptr, GTM_FREE_FUNC);
	DLSYM_ERR_AND_EXIT(gtm_filename_to_id_fptr_t, gtm_filename_to_id_fptr, GTM_FILENAME_TO_ID_FUNC);
	DLSYM_ERR_AND_EXIT(gtm_ci_fptr_t, gtm_ci_fptr, GTM_CI_FUNC);
	DLSYM_ERR_AND_EXIT(gtm_zstatus_fptr_t, gtm_zstatus_fptr, GTM_ZSTATUS_FUNC);
	DLSYM_ERR_AND_EXIT(gtm_xcfileid_free_fptr_t, gtm_xcfileid_free_fptr, GTM_XCFILEID_FREE_FUNC);
#	else
	gtm_is_file_identical_fptr = &gtm_is_file_identical;
	gtm_malloc_fptr = &gtm_malloc;
	gtm_free_fptr = &gtm_free;
	gtm_filename_to_id_fptr = &gtm_filename_to_id;
	gtm_ci_fptr = &gtm_ci;
	gtm_zstatus_fptr = &gtm_zstatus;
	gtm_xcfileid_free_fptr = &gtm_xcfileid_free;
#	endif
	return GC_SUCCESS;
}

/* ==================================================================================== */
/*				Plugin API implementations				*/
/* ==================================================================================== */

char* gtmcrypt_strerror()
{
	return gtmcrypt_err_string;
}


xc_status_t gtmcrypt_init(int prompt_passwd)
{	/* Initialize the encryption environment. If any of the following macros fail, the error return happens within the macro. */
	if (GC_SUCCESS != gc_init_interface(prompt_passwd))
		return GC_FAILURE;
	if (gtmcrypt_inited)
		return GC_SUCCESS;
	GC_PK_INIT;
	GC_PK_PROMPT_PASSWD(prompt_passwd)
	gtmcrypt_inited = TRUE;
	return GC_SUCCESS;
}

/* Note: If any of the following macros fail, the error return happens within the macro. */
xc_status_t gtmcrypt_getkey_by_name(xc_string_t *filename, gtmcrypt_key_t *handle)
{
	xc_fileid_ptr_t	fileid = NULL;
	gtm_dbkeys_tbl	*entry;
	xc_status_t	status = GC_SUCCESS;

	GC_VERIFY_INITED;
	*handle = INVALID_HANDLE;
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	GC_DBK_FILENAME_TO_ID(filename, fileid);
	entry = gc_dbk_get_entry_by_fileid(fileid);
	/* If the load below failed, don't continue */
	GC_DBK_RELOAD_IF_NEEDED(entry, status, fileid, NULL);
	if (0 == status)
	{
		entry = gc_dbk_get_entry_by_fileid(fileid);
		if (NULL == entry)
		{
			UPDATE_ERROR_STRING("Database file %s missing in DB keys file or does not exist", filename->address);
			return GC_FAILURE;
		}
		*handle = entry->index;
	}
	return status;
}

/* Note: If any of the following macros fail, the error return happens within the macro. */
xc_status_t gtmcrypt_getkey_by_hash(xc_string_t *hash, gtmcrypt_key_t *handle)
{
	gtm_dbkeys_tbl	*entry;
	xc_status_t	status = GC_SUCCESS;
	int		err_caused_by_gpg;
	char		save_err[MAX_GTMCRYPT_ERR_STRLEN], hex_buff[GTMCRYPT_HASH_HEX_LEN + 1];
	char		*gpg_msg = "Verify encrypted key file and your GNUPGHOME settings";
	char		*correct_key_msg = "Verify encryption key in DB keys file";
	char		*alert_msg;

	*handle = INVALID_HANDLE;
	GC_VERIFY_INITED;
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	entry = gc_dbk_get_entry_by_hash(hash);
	/* If the load below failed, don't continue */
	GC_DBK_RELOAD_IF_NEEDED(entry, status, NULL, hash->address);
	if (0 == status)
	{
		entry = gc_dbk_get_entry_by_hash(hash);
		if (NULL == entry)
		{	/* Lookup still failed. Verify if we have right permissions on GNUPGHOME or $HOME/.gnupg
			 * (if GNUPGHOME is unset). If not, then the below function will store the appropriate
			 * error message in err_string and so return GC_FAILURE.
			 */
			if (GC_SUCCESS != gc_pk_gpghome_has_permissions())
				return GC_FAILURE;
			err_caused_by_gpg = ('\0' != gtmcrypt_err_string[0]);
			alert_msg = (err_caused_by_gpg ? gpg_msg : correct_key_msg);
			GC_HEX(hash->address, hex_buff, GTMCRYPT_HASH_HEX_LEN);
			if (err_caused_by_gpg)
			{
				strcpy(save_err, gtmcrypt_err_string);
				UPDATE_ERROR_STRING("Expected hash - %s - %s. %s", hex_buff, save_err, alert_msg);
			} else
				UPDATE_ERROR_STRING("Expected hash - %s. %s", hex_buff, alert_msg);
			return GC_FAILURE;
		}
		*handle = entry->index;
	}
	return status;
}

/* Note: If any of the following macros fail, the error return happens within the macro. */
xc_status_t gtmcrypt_hash_gen(gtmcrypt_key_t handle, xc_string_t *hash)
{
	gtm_dbkeys_tbl	*entry;

	GC_VERIFY_INITED;
	assert(INVALID_HANDLE != handle);
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	GC_DBK_GET_ENTRY_FROM_HANDLE(handle, entry, GC_FAILURE);
	gc_dbk_get_hash(entry, hash);
	return GC_SUCCESS;
}

/* Note: If any of the following macros fail, the error return happens within the macro. */
xc_status_t gtmcrypt_encrypt(gtmcrypt_key_t handle, xc_string_t *unencrypted_block, xc_string_t *encrypted_block)
{
	crypt_key_t	key_handle;
	gtm_dbkeys_tbl	*entry;

	GC_VERIFY_INITED;
	assert(INVALID_HANDLE != handle);
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	GC_DBK_GET_ENTRY_FROM_HANDLE(handle, entry, GC_FAILURE);
	key_handle = entry->encr_key_handle;
	GC_SYM_ENCRYPT(key_handle, unencrypted_block, encrypted_block);
	return GC_SUCCESS;
}

/* Note: If any of the following macros fail, the error return happens within the macro. */
xc_status_t gtmcrypt_decrypt(gtmcrypt_key_t handle, xc_string_t *encrypted_block, xc_string_t *unencrypted_block)
{
	crypt_key_t	key_handle;
	gtm_dbkeys_tbl	*entry;

	GC_VERIFY_INITED;
	assert(INVALID_HANDLE != handle);
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	GC_DBK_GET_ENTRY_FROM_HANDLE(handle, entry, GC_FAILURE);
	key_handle = entry->decr_key_handle;
	GC_SYM_DECRYPT(key_handle, encrypted_block, unencrypted_block);
	return GC_SUCCESS;
}

/* Note: If any of the following macros fail, the error return happens within the macro. */
xc_status_t gtmcrypt_close()
{
	GC_VERIFY_INITED;
	gtmcrypt_err_string[0] = '\0';	/* discard any previously recorded error messages */
	gc_pk_scrub_passwd();
	gc_dbk_scrub_entries();
	gtmcrypt_inited = FALSE;
	return GC_SUCCESS;
}
