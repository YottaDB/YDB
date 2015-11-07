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
#ifndef __GTMCRYPT_UTIL_H
#define __GTMCRYPT_UTIL_H

#if !defined(DEBUG) && defined(assert)
# undef assert
# define assert(x)
#endif

#include "gtm_sizeof.h"			/* for SIZEOF */
#include "gtm_common_defs.h"		/* To import common macros implemented by GT.M */

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
#define	GTM_PATH_MAX			PATH_MAX + 1

#define GTM_DIST_ENV			"gtm_dist"
#define USER_ENV			"USER"
#define ENV_UNDEF_ERROR			"Environment variable %s not set"
#define ENV_EMPTY_ERROR			"Environment variable %s set to empty string"

#define MAX_GTMCRYPT_ERR_STRLEN		2048

#define GTM_PASSPHRASE_MAX		512
#define PASSPHRASE_ENVNAME_MAX		64
#define GTMCRYPT_DEFAULT_PASSWD_PROMPT	"Enter Passphrase: "

#define GTM_OBFUSCATION_KEY		"gtm_obfuscation_key"

#define GTMCRYPT_FIPS_ENV		"gtmcrypt_FIPS"

#define GC_H2D(H) ((H >= 'A' && H <= 'F') ? ((H - 'A') + 10) : (H - '0'))

/* Convert SOURCE, sequence of hexadecimal characters, into decimal representation. LEN denotes the length of the SOURCE string.
 * NOTE: Hexadecimal characters, presented in SOURCE string, has to be in upper case.
 */
#define GC_UNHEX(SOURCE, TARGET, LEN)												\
{																\
	int i;															\
	for (i = 0; i < LEN; i += 2)												\
		TARGET[i/2] = (unsigned char)(GC_H2D(SOURCE[i]) * 16 + GC_H2D(SOURCE[i + 1]));					\
}

/* Convert SOURCE, sequence of decimal characters, into hexadecimal representation. LEN denotes the length of the SOURCE string. */
#define GC_HEX(SOURCE, TARGET, LEN)												\
{																\
	int i;															\
	for (i = 0; i < LEN; i += 2)												\
		SPRINTF(TARGET + i, "%02X", (unsigned char)SOURCE[i / 2]);							\
}

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

/* Some helper error reporting macros. */
#define UPDATE_ERROR_STRING(...)												\
{																\
	SNPRINTF(gtmcrypt_err_string, MAX_GTMCRYPT_ERR_STRLEN, __VA_ARGS__);							\
}

#define IS_FIPS_MODE_REQUESTED(RV)											\
{															\
	char		*ptr;												\
															\
	RV = FALSE;													\
	if (NULL != (ptr = getenv(GTMCRYPT_FIPS_ENV)))									\
	{														\
		if ((0 == strcasecmp(ptr, "YES"))									\
			|| (0 == strcasecmp(ptr, "TRUE"))								\
			|| (*ptr == '1')										\
			|| (*ptr == 'Y')										\
			|| (*ptr == 'y'))										\
		{													\
			RV = TRUE;											\
		}													\
	}														\
}

/* In OpenSSL versions prior to 1.0.1, "FIPS_mode_set", the function that enables/disables FIPS mode, is available only
 * in a FIPS capable OpenSSL (that is, OpenSSL built from source with "fips" option). From 1.0.1, the function was moved
 * to "crypto/o_fips.c" and made as a wrapper to the actual FIPS Object Module and so was available on OpenSSL versions
 * built with or without the "fips" flag. Since the plugin would be run in an environment having OpenSSL with or without
 * FIPS capability, we first do a dlopen with a special NULL parameter to get access to the global namespace and then
 * do a dlsym to see if the "FIPS_mode_set" function is available. If not (or "FIPS_mode_set" returns 0), we raise an error.
 */
#define ENABLE_FIPS_MODE(RV, FIPS_ENABLED)										\
{															\
	void	*handle;												\
	int	(*FIPS_mode_set_fnptr)(int);										\
															\
	handle = dlopen(NULL, (RTLD_NOW | RTLD_GLOBAL));								\
	RV = 0;														\
	FIPS_ENABLED = FALSE;												\
	if (NULL == handle)												\
	{														\
		UPDATE_ERROR_STRING("Failed to dlopen the global namespace. Reason: %s.", dlerror());			\
		assert(FALSE);												\
		RV = -1;												\
	} else														\
	{														\
		FIPS_mode_set_fnptr = (int(*)(int))dlsym(handle, "FIPS_mode_set");					\
		if (NULL != FIPS_mode_set_fnptr)									\
		{	/* Symbol available. Invoke it to enable FIPS mode. */						\
			if (FIPS_mode_set_fnptr(1))									\
				FIPS_ENABLED = TRUE;									\
			else												\
			{												\
				GC_APPEND_OPENSSL_ERROR("Failed to initialize FIPS mode.");				\
				RV = -1;										\
			}												\
		} else													\
		{													\
			UPDATE_ERROR_STRING("Failed to initialize FIPS mode. Reason: cannot find OpenSSL FIPS "		\
						"functions in the runtime system.");					\
			RV = -1;											\
		}													\
	}														\
}

/* OpenSSL specific error handling. */
#define GC_APPEND_OPENSSL_ERROR(...)											\
{															\
	char	*errptr, *end;												\
	int	rv;													\
															\
	errptr = &gtmcrypt_err_string[0];										\
	end = errptr + MAX_GTMCRYPT_ERR_STRLEN;										\
	SNPRINTF(errptr, MAX_GTMCRYPT_ERR_STRLEN, __VA_ARGS__);								\
	errptr += STRLEN(errptr);											\
	SNPRINTF(errptr, end - errptr, "%s", " Reason: ");								\
	errptr += STRLEN(errptr);											\
	rv = ERR_get_error();												\
	ERR_error_string_n(rv, errptr, end - errptr);									\
}

/* Libgcrypt specific error handling. */
#define GC_APPEND_GCRY_ERROR(ERR, ...)											\
{															\
	char	*errptr, *end;												\
															\
	errptr = &gtmcrypt_err_string[0];										\
	end = errptr + MAX_GTMCRYPT_ERR_STRLEN;										\
	SNPRINTF(errptr, MAX_GTMCRYPT_ERR_STRLEN, __VA_ARGS__);								\
	errptr += STRLEN(errptr);											\
	SNPRINTF(errptr, end - errptr, "%s", " Reason: ");								\
	errptr += STRLEN(errptr);											\
	SNPRINTF(errptr, end - errptr, "%s", gcry_strerror(ERR));							\
}

#ifndef USE_SYSLIB_FUNCS
#define	MALLOC			(*gtm_malloc_fnptr)
#define FREE			(*gtm_free_fnptr)
#define GTM_FILENAME_TO_ID	(*gtm_filename_to_id_fnptr)
#define GTM_IS_FILE_IDENTICAL	(*gtm_is_file_identical_fnptr)
#define GTM_XCFILEID_FREE	(*gtm_xcfileid_free_fnptr)
#else
#define MALLOC			malloc
#define FREE			free
#endif

typedef struct
{
	char			env_name[PASSPHRASE_ENVNAME_MAX];
	char			*env_value;	/* Obfuscated version of the password stored in the environment (as hex) */
	char			*passwd;	/* Password in clear text. */
	int			passwd_len;	/* Length of the password that's allocated. */
} passwd_entry_t;

typedef void *				(*gtm_malloc_fnptr_t)(size_t);
typedef void				(*gtm_free_fnptr_t)(void *);
typedef gtm_status_t			(*gtm_filename_to_id_fnptr_t)(xc_string_t *, xc_fileid_ptr_t *);
typedef gtm_status_t			(*gtm_is_file_identical_fnptr_t)(xc_fileid_ptr_t, xc_fileid_ptr_t);
typedef void				(*gtm_xcfileid_free_fnptr_t)(xc_fileid_ptr_t);

GBLREF gtm_malloc_fnptr_t		gtm_malloc_fnptr;
GBLREF gtm_free_fnptr_t			gtm_free_fnptr;
GBLREF gtm_filename_to_id_fnptr_t	gtm_filename_to_id_fnptr;
GBLREF gtm_is_file_identical_fnptr_t	gtm_is_file_identical_fnptr;
GBLREF gtm_xcfileid_free_fnptr_t	gtm_xcfileid_free_fnptr;

GBLREF	char				gtmcrypt_err_string[MAX_GTMCRYPT_ERR_STRLEN];

int					gc_load_gtmshr_symbols(void);
void 					gtm_gcry_log_handler(void *opaque, int level, const char *fmt, va_list arg_ptr);
int					gc_read_passwd(char *prompt, char *buf, int maxlen);
int					gc_mask_unmask_passwd(int nparm, gtm_string_t *in, gtm_string_t *out);
void					gc_freeup_pwent(passwd_entry_t *pwent);
int 					gc_update_passwd(char *name, passwd_entry_t **ppwent, char *prompt, int interactive);
#endif
