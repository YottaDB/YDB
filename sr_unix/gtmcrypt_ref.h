/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTMCRYPT_REF_H
#define GTMCRYPT_REF_H

#ifdef USE_OPENSSL
# include <openssl/blowfish.h>
# include <openssl/sha.h>
# include <openssl/evp.h>
#elif defined USE_GCRYPT
# include <gcrypt.h>
#else
# error "Unsupported encryption library. Reference implementation currently supports openssl and gcrypt"
#endif

#ifndef DEBUG
 #undef assert
 #define assert(x)
#endif

/* Any change done to the below macro should be reflected in mdef.h and vice versa */
/* Note: sizeof returns a "unsigned long" type by default. In expressions involving 32-bit quantities,
* using sizeof causes a compiler warning for every 64->32 bit auto cast (zOS compiler for now).
* Hence typecasting the return to "int" on zOS (to avoid warning) in most common sizeof usages.
* Whenever SIZEOF needs to be used in expressions involving 64-bit pointer quantities, use ((INTPTR_T)SIZEOF(...)).
* Whenever SIZEOF needs to be used in expressions involving 64-bit integer quantities, use ((long)SIZEOF(...)).
*/
#if defined(__MVS__)
# define SIZEOF(X) ((int)(sizeof(X)))
#else
# define SIZEOF(X) ((long)sizeof(X))
#endif

typedef enum
{
	ERROR_LINE_INFO = -1,
	DAT_LINE_INFO,
	KEY_LINE_INFO
} dbkeyfile_line_type;

typedef enum
{
	LOOKING_FOR_DAT_ENTRY = 1,
	LOOKING_FOR_KEY_ENTRY,
	NUM_STATES
} dbkeyfile_states;
#ifdef USE_OPENSSL
typedef EVP_CIPHER_CTX		crypt_key_t;
#else
typedef gcry_cipher_hd_t	crypt_key_t;
#endif

#define TAB_NAME_MAX		512
#define GTM_PASSPHRASE_MAX	512
#define GC_ENCRYPT		1
#define GC_DECRYPT		0
#define GC_FAILURE		1
#define GC_SUCCESS		0
#define TRUE			1
#define FALSE			0
#define GNUPGHOME		"GNUPGHOME"
#define DOT_GNUPG		".gnupg"
#define GTM_PASSWD		"gtm_passwd"
#define GTM_DBKEYS		"gtm_dbkeys"
#define DOT_GTM_DBKEYS		"."GTM_DBKEYS
#define PASSWD_EMPTY		"Environment variable gtm_passwd set to empty string. Password prompting not allowed for utilites"
#define GTM_PATH_MAX		1024
#define GTM_KEY_MAX		32
#define GTMCRYPT_HASH_LEN	64
#define GTMCRYPT_HASH_HEX_LEN	GTMCRYPT_HASH_LEN * 2
#define DAT_LINE_INDICATOR 	"dat "
#define KEY_LINE_INDICATOR 	"key "
#define DAT_LINE_INDICATOR_SIZE	(SIZEOF(DAT_LINE_INDICATOR) - 1)
#define KEY_LINE_INDICATOR_SIZE	(SIZEOF(KEY_LINE_INDICATOR) - 1)
#define INVALID_HANDLE		-1
#define GTMCI			"GTMCI"
#define ERR_STRLEN		2048

#ifdef USE_GCRYPT
#define IV_LEN			16
#define ALGO			GCRY_CIPHER_AES256
#define MODE			GCRY_CIPHER_MODE_CFB
/* This string uniquely identifies the encryption algorithm and its parameters.
 * It will be appended to the encryption key and the combination will be hashed (with SHA512).
 * This hash will be used verify that the same algorithm (including parameters) and key are used to
 * open the database file as were used to create the database file. */
#define UNIQ_ENC_PARAM_STRING	"AES256CFB"
#define FLAGS			0
static char			iv[IV_LEN];
static int			gcry_already_inited = FALSE;
#else
#define ALGO			EVP_bf_cfb64()
#define UNIQ_ENC_PARAM_STRING	"BLOWFISHCFB"
#endif
#define UNIQ_ENC_PARAM_LEN	SIZEOF(UNIQ_ENC_PARAM_STRING) - 1
#define HASH_INPUT_BUFF_LEN	UNIQ_ENC_PARAM_LEN + GTM_KEY_MAX


/* ==================================================================================== */
/* Legend to namespaces used -
 * gc_XXXXX    - All functions start with the gc_ namespace
 * gc_dbk_XXX  - All functions related to db key mapping and internal book keeping
 * gc_sym_XXX  - All functions related to usages of symmetric enc/dec activities, primarily using libgcrypt or libcrypto
 * gc_pk_XXX   - All functions related to usages of public/private key enc/dec activities, primarily using libgpgme
 */
/* ==================================================================================== */

/* ==================================================================================== */
/*               Generic macros and functions related to this plugin                    */
/* ==================================================================================== */

#define GC_MIN_STATIC_BLOCK_SIZE	4096 /* Have a good size block, so that we dont keep reallocating  */
#define GC_ROUNDUP(x, y)		((x / y) * y) + ((x % y) ? y : 0)
#define GC_FLAGS			(RTLD_NOW | RTLD_GLOBAL)

#define GTM_MALLOC_FUNC			"gtm_malloc"
#define GTM_FREE_FUNC			"gtm_free"
#define GTM_FILENAME_TO_ID_FUNC		"gtm_filename_to_id"
#define GTM_CI_FUNC			"gtm_ci"
#define GTM_ZSTATUS_FUNC		"gtm_zstatus"
#define GTM_IS_FILE_IDENTICAL_FUNC	"gtm_is_file_identical"
#define GTM_XCFILEID_FREE_FUNC		"gtm_xcfileid_free"

xc_status_t gc_init_interface(int prompt_passwd);

typedef void *		(*gtm_malloc_fptr_t)(size_t);
typedef void		(*gtm_free_fptr_t)(void *);
typedef xc_status_t	(*gtm_filename_to_id_fptr_t)(xc_string_t *, xc_fileid_ptr_t *);
typedef xc_status_t	(*gtm_ci_fptr_t)(const char *c_rtn_name, ...);
typedef void		(*gtm_zstatus_fptr_t)(char *msg, int len);
typedef xc_status_t	(*gtm_is_file_identical_fptr_t)(xc_fileid_ptr_t, xc_fileid_ptr_t);
typedef void		(*gtm_xcfileid_free_fptr_t)(xc_fileid_ptr_t);

gtm_malloc_fptr_t		gtm_malloc_fptr;
gtm_free_fptr_t			gtm_free_fptr;
gtm_filename_to_id_fptr_t	gtm_filename_to_id_fptr;
gtm_ci_fptr_t			gtm_ci_fptr;
gtm_zstatus_fptr_t		gtm_zstatus_fptr;
gtm_is_file_identical_fptr_t	gtm_is_file_identical_fptr;
gtm_xcfileid_free_fptr_t	gtm_xcfileid_free_fptr;

#define DLSYM_ERR_AND_EXIT(fptr_type, fptr, func_name)					\
{											\
	fptr = (fptr_type)dlsym(handle, func_name);					\
	if (NULL == fptr)								\
	{										\
		snprintf(err_string, ERR_STRLEN, "Enable to resolve %s ", func_name);	\
		return GC_FAILURE;							\
	}										\
}

#define GC_MALLOC(blk, len, type)			\
{							\
	blk = (type *)gtm_malloc_fptr(len);		\
	assert (blk);					\
}

#define GC_FREE(blk)	gtm_free_fptr(blk)


#define GC_COPY_TO_XC_STRING(X, STR, N)	\
{					\
	memcpy((X)->address, STR, N);	\
	(X)->length = N;		\
}


/* Following makes sure that at no point we are in the encryption library without gtmcrypt_init getting called
 * prior to the current call
 */
#define GC_VERIFY_INITED													\
{																\
	if (!gtmcrypt_inited)													\
	{															\
		snprintf(err_string, ERR_STRLEN, "%s", "Encryption library has not been initialized");				\
		return GC_FAILURE;												\
	}															\
}

#define GC_IF_INITED_RETURN				\
{							\
	/* Check if init has happened already */	\
	if (gtmcrypt_inited)				\
		return GC_SUCCESS;			\
}

#define GC_SET_INITED	gtmcrypt_inited = TRUE;

#define GC_CLEAR_INITED	gtmcrypt_inited = FALSE;

#define GC_INT(H) ((H >= 'A' && H <= 'F') ? ((H - 'A') + 10) : (H - '0'))

#define GC_UNHEX(a, b, len)							\
{										\
	int i;									\
	for (i = 0; i < len; i+=2)						\
		b[i/2] = (unsigned char)(GC_INT(a[i]) * 16 + GC_INT(a[i + 1]));	\
}

#define GC_HEX(a, b, len)					\
{								\
	int i;							\
	for (i = 0; i < len; i+=2)				\
		sprintf(b + i, "%02X", (unsigned char)a[i/2]);	\
}

#define GC_GETENV(ptr, key, RC)									\
{												\
	RC = GC_SUCCESS;									\
	if (NULL == (ptr = (char *)getenv(key)))						\
		RC = GC_FAILURE;								\
}

#define GC_ENV_UNSET_ERROR(key)									\
{												\
	snprintf(err_string, ERR_STRLEN, "Environment variable %s not set", key);		\
}

/* Allocate a single block, and try reusing the same everytime this macro is called */
#ifdef USE_OPENSSL
#define GC_GET_STATIC_BLOCK(out, block_len)						\
{											\
	static char *blk = (char *)NULL;						\
	static int allocated_len = GC_MIN_STATIC_BLOCK_SIZE;				\
	if (blk == NULL || (block_len > allocated_len))					\
	{										\
		if (blk)								\
			GC_FREE(blk);							\
		allocated_len = (block_len > allocated_len) ?				\
				GC_ROUNDUP(block_len, GC_MIN_STATIC_BLOCK_SIZE) : 	\
				allocated_len; 						\
		GC_MALLOC(blk, allocated_len, char);					\
	}										\
	out = blk;									\
}
#endif
#endif /* GTMCRYPT_REF_H */
