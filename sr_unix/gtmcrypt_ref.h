/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTMCRYPT_REF_H
#define GTMCRYPT_REF_H

#if !defined(DEBUG) && defined(assert)
# undef assert
# define assert(x)
#endif

#include "gtm_common_defs.h"		/* To import common macros implemented by GT.M */
#include "gtm_sizeof.h"			/* for SIZEOF */

/* ==================================================================================== */
/* Legend to namespaces used -
 * gc_XXXXX    - All functions start with the gc_ namespace
 * gc_dbk_XXX  - All functions related to db key mapping and internal book keeping
 * gc_sym_XXX  - All functions related to usages of symmetric enc/dec activities, primarily using libgcrypt or libcrypto
 * gc_pk_XXX   - All functions related to usages of public/private key enc/dec activities, primarily using libgpgme
 */
/* ==================================================================================== */


/* The value 1023 for PATH_MAX is derived using pathconf("path", _PC_PATH_MAX) on z/OS and
 * we figure other POSIX platforms are at least as capable if they don't define PATH_MAX.
 * Since we can't afford to call a function on each use of PATH_MAX/GTM_PATH_MAX, this
 * value is hardcoded here.
 *
 * Note on Linux (at least), PATH_MAX is actually defined in <sys/param.h>. We would include
 * that here unconditionally but on AIX, param.h includes limits.h. Note that regardless of where
 * it gets defined, PATH_MAX needs to be defined prior to including stdlib.h. This is because in a
 * pro build, at least Linux verifies the 2nd parm of realpath() is PATH_MAX bytes or more.
 * Since param.h sets PATH_MAX to 4K on Linux, this can cause structures defined as GTM_PATH_MAX
 * to raise an error when used in the 2nd argument of realpath().
 * Note : The definition of PATH_MAX and GTM_PATH_MAX is borrowed from gtm_limits.h. Change in one
 * should be reflected in the other.
 */
#ifndef PATH_MAX
#  ifdef __linux__
#    include <sys/param.h>
#  else
#    define PATH_MAX 			1023
#  endif
#endif
/* Now define our version which includes space for a terminating NULL byte */
#define	GTM_PATH_MAX			PATH_MAX + 1
#define TAB_NAME_MAX			512
#define GTM_PASSPHRASE_MAX		512
#define GC_ENCRYPT			1
#define GC_DECRYPT			0
#define GC_FAILURE			1
#define GC_SUCCESS			0
#define DOT_GTM_DBKEYS			".gtm_dbkeys"
#define DOT_GNUPG			".gnupg"
#define SYMMETRIC_KEY_MAX		32
#define GTMCRYPT_HASH_LEN		64
#define GTMCRYPT_HASH_HEX_LEN		GTMCRYPT_HASH_LEN * 2
#define INVALID_HANDLE			-1
#define GTMCI				"GTMCI"
#define MAX_GTMCRYPT_ERR_STRLEN		2048

#define GC_MIN_STATIC_BLOCK_SIZE	4096			/* Have a good size block, so that we dont keep reallocating */
#define GC_FLAGS			(RTLD_NOW | RTLD_GLOBAL)

/* Some environment variables that encryption plugin cares about */
#define GNUPGHOME			"GNUPGHOME"
#define GTM_PASSWD			"gtm_passwd"
#define GTM_OBFUSCATION_KEY		"gtm_obfuscation_key"
#define GTM_DBKEYS			"gtm_dbkeys"
#define HOME				"HOME"
#define USER				"USER"
#define GTM_DIST			"gtm_dist"

#define ENV_UNDEF_ERROR			"Environment variable %s not set"
#define ENV_EMPTY_ERROR			"Environment variable %s set to empty string"

/* Define function pointers to the functions that the encryption plugin imports from libgtmshr.so at runtime */
#define GTM_MALLOC_FUNC			"gtm_malloc"
#define GTM_FREE_FUNC			"gtm_free"
#define GTM_FILENAME_TO_ID_FUNC		"gtm_filename_to_id"
#define GTM_CI_FUNC			"gtm_ci"
#define GTM_ZSTATUS_FUNC		"gtm_zstatus"
#define GTM_IS_FILE_IDENTICAL_FUNC	"gtm_is_file_identical"
#define GTM_XCFILEID_FREE_FUNC		"gtm_xcfileid_free"

typedef void *				(*gtm_malloc_fptr_t)(size_t);
typedef void				(*gtm_free_fptr_t)(void *);
typedef xc_status_t			(*gtm_filename_to_id_fptr_t)(xc_string_t *, xc_fileid_ptr_t *);
typedef xc_status_t			(*gtm_ci_fptr_t)(const char *c_rtn_name, ...);
typedef void				(*gtm_zstatus_fptr_t)(char *msg, int len);
typedef xc_status_t			(*gtm_is_file_identical_fptr_t)(xc_fileid_ptr_t, xc_fileid_ptr_t);
typedef void				(*gtm_xcfileid_free_fptr_t)(xc_fileid_ptr_t);

GBLDEF gtm_malloc_fptr_t		gtm_malloc_fptr;
GBLDEF gtm_free_fptr_t			gtm_free_fptr;
GBLDEF gtm_filename_to_id_fptr_t	gtm_filename_to_id_fptr;
GBLDEF gtm_ci_fptr_t			gtm_ci_fptr;
GBLDEF gtm_zstatus_fptr_t		gtm_zstatus_fptr;
GBLDEF gtm_is_file_identical_fptr_t	gtm_is_file_identical_fptr;
GBLDEF gtm_xcfileid_free_fptr_t		gtm_xcfileid_free_fptr;

xc_status_t 		gc_init_interface(int prompt_passwd);		/* function that sets up the above function pointers */

GBLREF	char		gtmcrypt_err_string[MAX_GTMCRYPT_ERR_STRLEN];	/* most recent error that the plugin encountered */

#define SNPRINTF(SRC, LEN, ...)													\
{																\
	int		rc;													\
																\
	do															\
	{															\
		rc = snprintf(SRC, LEN, __VA_ARGS__);	/* BYPASSOK */								\
	} while ((-1 == rc) && (EINTR == errno)); /* EINTR-safe */								\
}

#define SPRINTF(SRC, ...)													\
{																\
	int		rc;													\
																\
	do															\
	{															\
		rc = sprintf(SRC, __VA_ARGS__);	/* BYPASSOK */									\
	} while ((-1 == rc) && (EINTR == errno)); /* EINTR-safe */								\
}

#define UPDATE_ERROR_STRING(...)												\
{																\
	SNPRINTF(gtmcrypt_err_string, MAX_GTMCRYPT_ERR_STRLEN, __VA_ARGS__);							\
}

#define DLSYM_ERR_AND_EXIT(fptr_type, fptr, func_name)										\
{																\
	char		*dlerr_ptr;												\
																\
	fptr = (fptr_type)dlsym(handle, func_name);										\
	if (NULL == fptr)													\
	{															\
		if (NULL == (dlerr_ptr = dlerror()))										\
		{														\
			UPDATE_ERROR_STRING("Unable to resolve symbol %s. Unknown system error", func_name);			\
		} else														\
			UPDATE_ERROR_STRING("Unable to resolve symbol %s. %s", func_name, dlerr_ptr);				\
		return GC_FAILURE;												\
	}															\
}

#define GC_MALLOC(blk, len, type)												\
{																\
	blk = (type *)gtm_malloc_fptr(len);											\
	assert(blk);														\
}

#define GC_FREE(blk)	gtm_free_fptr(blk)


/* Following makes sure that at no point we are in the encryption library without gtmcrypt_init getting called
 * prior to the current call
 */
#define GC_VERIFY_INITED													\
{																\
	if (!gtmcrypt_inited)													\
	{															\
		UPDATE_ERROR_STRING("Encryption library has not been initialized");						\
		return GC_FAILURE;												\
	}															\
}

#define GC_INT(H) ((H >= 'A' && H <= 'F') ? ((H - 'A') + 10) : (H - '0'))

#define GC_UNHEX(a, b, len)													\
{																\
	int i;															\
	for (i = 0; i < len; i += 2)												\
		b[i/2] = (unsigned char)(GC_INT(a[i]) * 16 + GC_INT(a[i + 1]));							\
}

#define GC_HEX(a, b, len)													\
{																\
	int i;															\
	for (i = 0; i < len; i += 2)												\
		SPRINTF(b + i, "%02X", (unsigned char)a[i / 2]);								\
}

/* Allocate a single block, and reuse everytime this macro is called */
#ifdef USE_OPENSSL
#define GC_GET_STATIC_BLOCK(out, block_len)											\
{																\
	static char *blk = (char *)NULL;											\
	static int allocated_len = GC_MIN_STATIC_BLOCK_SIZE;									\
	if (blk == NULL || (block_len > allocated_len))										\
	{															\
		if (blk)													\
			GC_FREE(blk);												\
		allocated_len = (block_len > allocated_len) ?  ROUND_UP(block_len, GC_MIN_STATIC_BLOCK_SIZE) : allocated_len; 	\
		GC_MALLOC(blk, allocated_len, char);										\
	}															\
	out = blk;														\
}
#endif
#endif /* GTMCRYPT_REF_H */
