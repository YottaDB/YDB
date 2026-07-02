/****************************************************************
 *								*
 * Copyright (c) 2006-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* NOTE: THIS MODULE MUST NEVER INCLUDE HEADER fILE gtm_icu_api.h */

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_strings.h"
#include "gtm_stdlib.h"		/* needed for realpath */
#include <gtm_limits.h>		/* needed for GTM_PATH_MAX */
#include "gtm_stat.h"

#include <wctype.h>
#include <dlfcn.h>
#include <locale.h>		/* needed for setlocale() */
#include <langinfo.h>		/* needed for nl_langinfo() */
#ifdef _AIX
#include <sys/ldr.h>		/* needed for loadquery */
#include <libgen.h>		/* needed for basename */
#include <errno.h>
#endif

#define GTMIO_MINIMAL		/* If gtmio.h is pulled in for tracing, don't include stuff that causes redefined errors */
#include "error.h"
#include "io.h"
#include "iosp.h"
#include "gtm_logicals.h"	/* needed for GTM_ICU_VERSION */
#include "trans_log_name.h"
#include "real_len.h"		/* for COPY_DLERR_MSG */
#include "fgncal.h"		/* needed for COPY_DLLERR_MSG() */
#include "restrict.h"		/* Needed for restrictions */
#include "have_crit.h"		/* Defer interrupts */

#ifdef _AIX
# define DELIM			":"
# define MAX_SEARCH_PATH_LEN	GTM_PATH_MAX * 4 /* Give enough room for loadquery to give the search paths in the first time */
# define ICU_LIBNAME_LEN	GTM_PATH_MAX
# define ICU_NOT_FOUND_ERR	"Cannot find ICU library in the standard system library path (/usr/lib, /usr/lib64 etc.) or LIBPATH"
#endif

ZOS_ONLY(GBLREF	char	*gtm_utf8_locale_object;)
GBLREF boolean_t		gtm_dist_ok_to_use;
GBLREF char			gtm_dist[GTM_PATH_MAX];
GBLREF	volatile boolean_t	timer_in_handler;
GBLREF mstr			dollar_zicuver;

/* For now the minimum ICU version supported is 3.6 */
#define	ICU_MINIMUM_SUPPORTED_VER	"3.6"
#define IS_ICU_VER_GREATER_THAN_MIN_VER(major_ver, minor_ver) ((3 < major_ver) || ((3 == major_ver) && (6 <= minor_ver)))

/* ICU function to query for the ICU version. Used only if GTM_ICU_VERSION is not set. */
#define GET_ICU_VERSION_FNAME	"u_getVersion"

/* Indicates the maximum size of the array that is given as input to u_getVersion. The corresponding MACRO
 * in unicode/uversion.h is U_MAX_VERSION_LENGTH and is tagged as @stable 2.4.
 */
#define MAX_ICU_VERSION_LENGTH	4

/* Indicates the maximum length of the complete ICU version in string form. The corresponding MACRO in
 * unicode/uversion.h is U_MAX_VERSION_STRING_LENGTH and is tagged as @stable 2.4.
 */
#define MAX_ICU_VERSION_STRLEN	20

/* Maintain max length of all the ICU function names that GT.M uses. This is currently 20 so we keep max at 24 to be safe.
 * In case a new function that has a name longer than 24 chars gets added, an assert below will fail so we are protected.
 */
#define MAX_ICU_FNAME_LEN	24

/* The function u_getVersion which queries for the version number of installed ICU library expects
 * UVersionInfo which is typefed to uint8_t. To avoid including unicode/uversion.h that defines UVersionInfo
 * do explicit typedef of uint8_t to UVersionInfo here. We don't expect UVersionInfo type to change as it is
 * tagged as @stable ICU 2.4 and GT.M will only support ICU versions >= 3.6
 */
typedef uint8_t UVersionInfo[MAX_ICU_VERSION_LENGTH];

typedef void (*icu_func_t)(UVersionInfo);	/* a generic pointer type to the ICU function */

/* The first parameter to ICUVERLT36 can be either ICU_LIBNAME or $gtm_icu_version. Depending on the choice
 * of the first parameter, different suffix is chosen.
 */
#define ICU_LIBNAME_SUFFIX		" has version"
#define GTM_ICU_VERSION_SUFFIX		" is"

/* Declare enumerators for all functions */
#define ICU_DEF(x) x##_,
enum {
#include "gtm_icu.h"	/* BYPASSOK */
icu_func_n		/* total number of ICU functions used */
};
#undef ICU_DEF

/* Initialize the table of symbol names to be used in dlsym() */
#define ICU_DEF(x) #x,
LITDEF char* icu_fname[] =
{
#include "gtm_icu.h"	/* BYPASSOK */
NULL
};
#undef ICU_DEF

#define ICU_DEF(x) GBLDEF icu_func_t x##_ptr;
#include "gtm_icu.h"
#undef ICU_DEF

/* initialize the table of locations of function pointers that are set by dlsym()
 * Note that though the table type is of icu_func_t pointers, in fact each ICU function
 * has its own signature and not that of icu_func_t (except for u_getVersion()) */
#define ICU_DEF(x) &x##_ptr,
GBLDEF icu_func_t *icu_fptr[] =
{
#include "gtm_icu.h"	/* BYPASSOK */
NULL
};
#undef ICU_DEF

/* duplicated prototypes needed to avoid including header file gtm_icu_api.h */
boolean_t gtm_icu_init(boolean_t called_by_zconvert);
void gtm_conv_init(void);

error_def(ERR_DLLNOOPEN);
error_def(ERR_ICUSYMNOTFOUND);
error_def(ERR_ICUVERLT36);
error_def(ERR_NONUTF8LOCALE);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_TEXT);

/*
 * The ICU project has used two different formats to specify the version. These format are visible to us
 * through icu-config, the library name and renamed symbols.
 *
 * icu-config reported the version in the following format from ICU 3.6 until ICU 4.8
 * 	<major> . <minor> . <sub minor vers...>
 * As of ICU 49 (aka 4.9), the format returned is:
 * 	<major><minor> . <sub minor vers...>
 *
 *
 * The version numbers used in the ICU library file names from ICU 3.6 until ICU 4.8 were
 * 	<major> . <minor> . <sub minor vers...>	(symlinked to the below name)
 * 	<major> . <minor>
 *
 * As of ICU 49 (aka 4.9) the version used in the library file names is (note the missing dot between major and minor)
 * 	<major><minor> . <sub minor vers...>	(symlinked to the below name)
 * 	<major><minor>
 *
 *
 * The version number extension for renamed symbols from ICU 3.6 until ICU 4.2 was (confirmed on
 * http://icu-project.org/apiref/icu4c/uvernum_8h.html, search for U_ICU_ENTRY_POINT_RENAME)
 * 	_<major> _ <minor>
 * As of ICU 4.4, the version number extension changed to (note the missing underscore)
 * 	_<major><minor>
 *
 * The function below parses gtm_icu_version to determine the strings used in the library and symbol versions
 */
static boolean_t parse_gtm_icu_version(char *icu_ver_buf, int len, char *icusymver, char *iculibver)
{
	char		*ptr;
	char		tmp_errstr[SIZEOF(GTM_ICU_VERSION) + STR_LIT_LEN(GTM_ICU_VERSION_SUFFIX)]; /* "$gtm_icu_version is" */
	int4		major_ver, minor_ver;
	int		i;

	if ((NULL == icu_ver_buf) || (0 == len))
		return FALSE;	/* empty string */

	/* Deconstruct the two known forms of gtm_icu_version "[0-9].[0-9]" and "[0-9][0-9]" ignoring trailing values */
	ptr = icu_ver_buf;
	if (-1 == (major_ver = asc2i((uchar_ptr_t)ptr++, 1)))
		return FALSE;
	if ('.' == *ptr)
		ptr++;
	if (-1 == (minor_ver = asc2i((uchar_ptr_t)ptr, 1)))
		return FALSE;

	/* Generate the ICU symbol renaming string */
	i = 0;
	icusymver[i++] = '_';
	icusymver[i++] = *icu_ver_buf;
	if ( 44 > ((major_ver * 10) + minor_ver))
		icusymver[i++] = '_';
	icusymver[i++] = *ptr;
	icusymver[i++] = '\0';

	/* Generate the ICU library name string */
	i = 0;
	iculibver[i++] = *icu_ver_buf;
	iculibver[i++] = *ptr;
	iculibver[i++] = '\0';

	/* Check if the formatted version is greater than or equal to 3.6 */
	if (!(IS_ICU_VER_GREATER_THAN_MIN_VER(major_ver, minor_ver)))
	{
		/* Construct the first part of the ICUVERLT36 error message. */
		SNPRINTF(tmp_errstr, SIZEOF(GTM_ICU_VERSION) + STR_LIT_LEN(GTM_ICU_VERSION_SUFFIX), "%s%s", GTM_ICU_VERSION,
			GTM_ICU_VERSION_SUFFIX);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_ICUVERLT36, 4, LEN_AND_STR(tmp_errstr), major_ver, minor_ver);
	}
	return TRUE;
}

/**
 * @brief Check if ICU symbols are renamed, and if so, how.
 *
 * If environment variable gtm_icu_version was set at process start,
 * then parse_gtm_icu_version() will have already set our ICU symbol renaming suffix.
 * If that is not already set, and if renaming is detected, we use the modern ICU
 * renaming convention to check versions out to version 20 (as of 2026, we are only at version 7).
 *
 * We have the advantage that parse_gtm_icu_version() does not have in that we have a dlopen()
 * handle which we can quickly query with dlsym().
 *
 * Doing this once up-front considerably simplifies the later symbol processing.
 *
 * @param handle Handle from dlopen() for the ICU library
 * @param icusmver The renaming string to use (may already be set)
 * @param icusymver_lenp Pointer to the length of icusmver
 *
 * @return TRUE if symbols are renamed, FALSE if they are not
 *
 */
boolean_t icu_symbols_renamed(void *handle, char *icusymver, int *icusymver_lenp)
{
	const char *version_func_name = icu_fname[u_getVersion_];
	char rename_buf[strlen(version_func_name) + MAX_ICU_VERSION_STRLEN + 1];
	int i,j;
	boolean_t new_suf;

	/* Found it, undecorated */
	if (dlsym(handle, version_func_name))
		return FALSE;

	/*
	 * Renaming is apparently in force.  If we already have a rename string from
	 * parse_gtm_icu_version() just go with that and return
	 */
	if (0 < *icusymver_lenp)
		return TRUE;
	/*
	 * We do need do rename, and we do not have an existing rename string.  Find the
	 * answer from dlsym().  Supposedly from ICU 3.6 until ICU 4.2 the rename string
	 * was "major_minor" & after that "_majorminor.
	 */
	for(i = 3; i <= 20; i++)
	{
		for (j = 0; j <= 9; j++)
		{
			new_suf = TRUE;

			if ((4 > i) || ((4 == i) && (2 >= j)))
				new_suf = FALSE;

			if (new_suf)
				snprintf(rename_buf, sizeof(rename_buf), "%s_%d%d", version_func_name, i, j);
			else
				snprintf(rename_buf, sizeof(rename_buf), "%s%d_%d", version_func_name, i, j);

			if (dlsym(handle, rename_buf))
			{
				if (new_suf)
					snprintf(icusymver, MAX_ICU_VERSION_STRLEN, "_%d%d", i, j);
				else
					snprintf(icusymver, MAX_ICU_VERSION_STRLEN, "%d_%d", i, j);

				*icusymver_lenp = strlen(icusymver);
				return TRUE;
			}
		}
	}

	/* If we get here, something went wrong.  We return FALSE for no rename
	 * and let the symbol lookup loop abort for us later
	 */
	return FALSE;
}

/**
 * @brief Direct ICU to use GT.M's memory allocation functions
 *
 * This function uses ICU's u_setMemoryFunctions() routine to
 * direct ICU to use GTM's memory allocation and free functions,
 * enabling the tools for displaying and debugging GTM memory
 * to be aware of ICU memory.
 *
 * This setup must be done before any other ICU function (including
 * (u_getVersion()) is called.
 *
 * @return true on success, false on error
 */
boolean_t gtm_icu_setmemfunc()
{
	/* Typedefs from unicode/uclean.h to avoid pulling it in here */
	typedef void *UMemAllocFn(const void *context, size_t size);
	typedef void *UMemReallocFn(const void *context, void *mem, size_t size);
	typedef void UMemFreeFn(const void *context, void *mem);
	typedef void setmemfunc_t(const void *context, UMemAllocFn *a, UMemReallocFn *r, UMemFreeFn *f,  int *status);

	int		icu_error = 0;
	int 		i;
	setmemfunc_t    *fptr = (setmemfunc_t *)u_setMemoryFunctions_ptr;

	/* Do the actual reassignment of ICU's memfuncs */
	(*fptr)(ICU_MEM_CONTEXT, gtm_malloc_icu, gtm_realloc_icu, gtm_free_icu, &icu_error);
	if (0 != icu_error)
		return FALSE;	/* we were not able to assign the memory functions for some reason */

	return TRUE;	/* All is well */
}

boolean_t gtm_icu_init(boolean_t called_by_zconvert)
{
	char		*locale, *chset, *libname, err_msg[MAX_ERRSTR_LEN];
	char		icu_final_fname[MAX_ICU_FNAME_LEN + 1 + MAX_ICU_VERSION_STRLEN];	/* 1 for '_' in between */
	char		icu_ver_buf[MAX_ICU_VERSION_STRLEN], icusymver[MAX_ICU_VERSION_STRLEN], iculibver[MAX_ICU_VERSION_STRLEN];
	char		tmp_errstr[SIZEOF(ICU_LIBNAME) + STR_LIT_LEN(ICU_LIBNAME_SUFFIX)]; /* "libicuio.so has version" */
	char		icu_libname[SIZEOF(ICU_LIBNAME) + MAX_ICU_VERSION_STRLEN];
	char		*strtokptr;
	int		icu_final_fname_len, icu_libname_len, len = 0, save_fname_len, icusymver_len = 0, iculibver_len;
	void_ptr_t	handle;
	char_ptr_t	err_str;
	void		*fptr;
	int		findx, ver;
	boolean_t	icu_getversion_found = FALSE, gtm_icu_ver_defined, symbols_renamed;
	UVersionInfo	icu_version;
	mstr		trans;
	UMSTR_CONST(icu_ver, GTM_ICU_VERSION);
	int		iculdflags = ICU_LIBFLAGS;
	struct stat	libpath_stat;
	char		real_path[GTM_PATH_MAX], librarypath[GTM_PATH_MAX];
	char		*pieceptr, *cptr;
#	ifdef _AIX
	int		buflen, prev_dyn_size;
	char		buf[ICU_LIBNAME_LEN], temp_path[GTM_PATH_MAX], search_paths[MAX_SEARCH_PATH_LEN];
	char		*ptr, *each_libpath, *dyn_search_paths = NULL, *search_path_ptr;
	struct stat	real_path_stat;		/* To see if the resolved real_path exists or not */
#	endif
	intrpt_state_t  prev_intrpt_state;

	assert(!gtm_utf8_mode);
	dollar_zicuver.addr = (char *) malloc(MAX_ICU_VERSION_STRLEN);
	memset(dollar_zicuver.addr, 0, MAX_ICU_VERSION_STRLEN);
#	ifdef __MVS__
	if (gtm_utf8_locale_object)
		locale = setlocale(LC_CTYPE, gtm_utf8_locale_object);
	else
#	endif
#	ifdef _AIX
	iculdflags |= RTLD_MEMBER;
#	endif
	DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
	locale = setlocale(LC_CTYPE, "");
	ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
	chset = nl_langinfo(CODESET);
	if (NULL == chset)	/* 4SCA : null return nl_langinfo not possible */
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_NONUTF8LOCALE, 2, LEN_AND_LIT("<undefined>"));
	if ((NULL == locale) || ((0 != strcasecmp(chset, "utf-8")) && (0 != strcasecmp(chset, "utf8"))))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_NONUTF8LOCALE, 2, LEN_AND_STR(chset));
	/* By default, GT.M will henceforth expect that ICU has been built with symbol renaming disabled. If that is not the case,
	 * GT.M can be notified of this through an environment variable (macro GTM_ICU_VERSION).  The variable should contain the
	 * icu version number formatted as "<major-ver>.<minor-ver>". Example would be "3.6" to indicate ICU 3.6.
	 * Given the above environment variable, GT.M will try to dlopen the version specific icu library directly
	 * (i.e. libicuio.so.36 instead of libicuio.so). In that case, it will try both renamed and non-renamed symbols and
	 * whichever works it will use that. But if the environment variable is not specified, GT.M will assume that ICU has
	 * been built with no renaming of symbols. Any value of the environment variable not conforming to the above formatting
	 * will be treated as if this environment variable was not set at all and the default behavior (which is to query for
	 * symbols without appended version numbers) will be used.
	 */
	gtm_icu_ver_defined = FALSE;
	if (SS_NORMAL == TRANS_LOG_NAME(&icu_ver, &trans, icu_ver_buf, SIZEOF(icu_ver_buf), do_sendmsg_on_log2long))
	{	/* GTM_ICU_VERSION is defined. Do edit check on the value before considering it really defined */
		gtm_icu_ver_defined = parse_gtm_icu_version(trans.addr, trans.len, icusymver, iculibver);
	}
	if (gtm_icu_ver_defined)
	{	/* User explicitly specified an ICU version. So load version specific icu file (e.g. libicuio.so.36) */
		icu_libname_len = 0;
		iculibver_len = STRLEN(iculibver);
		icusymver_len = STRLEN(icusymver);
#		if defined(_AIX) || defined(__MVS__) || defined(__CYGWIN__)
		/* Transform (e.g. libicuio.a  -> libicuio36.a  ) */
		len = STR_LIT_LEN(ICU_LIBNAME_ROOT);
		memcpy(&icu_libname[icu_libname_len], ICU_LIBNAME_ROOT, len);
		icu_libname_len += len;
		memcpy(&icu_libname[icu_libname_len], iculibver, iculibver_len);
		icu_libname_len += iculibver_len;
		icu_libname[icu_libname_len++] = '.';
		len = STR_LIT_LEN(ICU_LIBNAME_EXT);
		memcpy(&icu_libname[icu_libname_len], ICU_LIBNAME_EXT, len);
		icu_libname_len += len;
#		else
		/* Transform (e.g. libicuio.so -> libicuio.so.36) */
		len = STR_LIT_LEN(ICU_LIBNAME);
		memcpy(&icu_libname[icu_libname_len], ICU_LIBNAME, len);
		icu_libname_len += len;
		icu_libname[icu_libname_len++] = '.';
		memcpy(&icu_libname[icu_libname_len], iculibver, iculibver_len);
		icu_libname_len += iculibver_len;
#		endif
		icu_libname[icu_libname_len] = '\0';
		assert(SIZEOF(icu_libname) > icu_libname_len);
		libname = icu_libname;
	} else
		libname = ICU_LIBNAME;  /* go with default name */
	DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
	if (RESTRICTED(library_load_path))
	{
		/* Try the version named symlink */
		if (gtm_icu_ver_defined)
		{
			SNPRINTF(librarypath, LIBRARY_PATH_MAX, GTM_PLUGIN_FMT_FULL, gtm_dist, libname);
			if (0 != Stat(librarypath, &libpath_stat)) /* Try the default named symlink */
				SNPRINTF(librarypath, LIBRARY_PATH_MAX, GTM_PLUGIN_FMT_SHORT ICU_LIBNAME, gtm_dist);
		} else	/* Try the default named symlink */
			SNPRINTF(librarypath, LIBRARY_PATH_MAX, GTM_PLUGIN_FMT_SHORT ICU_LIBNAME, gtm_dist);
#		ifdef _AIX
		STRNCAT(librarypath, AIX_SHR_64, SIZEOF(AIX_SHR_64)); /* Append "(shr_64.o)" to library path */
#		endif
		libname = librarypath;
		if (NULL == (handle = dlopen(libname, iculdflags)))
		{
			ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
			SNPRINTF(err_msg, MAX_ERRSTR_LEN, "dlopen(%s)", libname);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_RESTRICTEDOP, 1, err_msg);
		}
#		ifndef _AIX
		if (NULL == realpath(librarypath, real_path))
			assert(FALSE); /* Just opened that library */
		pieceptr = STRTOK_R(real_path, DOT_CHAR, &strtokptr);
		while (NULL != pieceptr)
		{
			if ((2 == strlen(pieceptr)) && (('0' <= pieceptr[0]) && ('9' >= pieceptr[0]))
					&& (('0' <= pieceptr[1]) && ('9' >= pieceptr[1])))
			{
				icusymver_len = ver = 0;
				cptr = pieceptr;
				A2I(cptr, pieceptr+2, ver);
				assert(0 <= ver);	/* Already validated above */
				/* Generate the ICU symbol renaming string */
				icusymver[icusymver_len++] = '_';
				icusymver[icusymver_len++] = pieceptr[0];
				if (44 > ver)
					icusymver[icusymver_len++] = '_';
				icusymver[icusymver_len++] = pieceptr[1];
				icusymver[icusymver_len++] = '\0';
				gtm_icu_ver_defined = TRUE;
				break;
			}
			pieceptr = STRTOK_R(NULL, DOT_CHAR, &strtokptr);
		}
#		endif
	} else	/* Note: the "else" clause spans an ifdef which calls dlopen() twice on AIX */
#	ifdef _AIX
	if (gtm_icu_ver_defined || /* Use the AIX system default when no ICU version specified */
			NULL == (handle = dlopen(ICU_LIBNAME_DEF, iculdflags)))
	{
		/* AIX has a unique packaging convention in that shared objects are conventionally
		 * archived into a static (.a) library. To resolve the shared library name at runtime
		 * in a version independent way we use loadquery to fetch the paths that might contain
		 * the ICU shared library. By running through each of these paths, we see if any of them
		 * contains the libicuio.a. If so, we use realpath to find the versioned ICU library
		 * that is symbolically linked from libicuio.a. This realpath can then be used to construct
		 * the fully qualified archive + member combination that will be finally dlopen'ed. */
		prev_dyn_size = MAX_SEARCH_PATH_LEN;
		search_path_ptr = search_paths;
		while(-1 == loadquery(L_GETLIBPATH, search_path_ptr, prev_dyn_size))
		{
			/* We don't expect loadquery to fail for reason other than ENOMEM */
			assertpro(ENOMEM == errno);
			/* If the previous call to loadquery fails and if it's because the input buffer's length was not
			 * enough for loadquery to fill the library search paths, then do a malloc equal to double the prior
			 * size and call loadquery again. It's relatively unlikely that this condition would be reached
			 */
			if (NULL != dyn_search_paths)
				free(dyn_search_paths);
			prev_dyn_size *= 2;
			dyn_search_paths = (char *)malloc(prev_dyn_size);
			search_path_ptr = dyn_search_paths;
		}
		/* At this point we have all the library search paths pointed by search_path_ptr seperated by ":". */
		each_libpath = STRTOK_R(search_path_ptr, DELIM, &strtokptr);
		while (NULL != each_libpath)
		{
			SNPRINTF(temp_path, GTM_PATH_MAX, "%s/%s", each_libpath, libname);
			if (NULL == realpath(temp_path, real_path) && (0 != Stat(real_path, &real_path_stat)))
			{
				each_libpath = STRTOK_R(NULL, DELIM, &strtokptr);
				continue;
			}
			/* At this point we would have in real_path the fully qualified path to the version'ed libicuio archive.
			 * ICU_LIBNAME - libicuio.a would have resulted in libicuio36.0.a
			 * Now, we need to construct the archive library name along with it's shared object member. This is done
			 * below.
			 */
			buflen = 0;
			/* real_path = /usr/local/lib64/libicuio36.0.a */
			ptr = basename(real_path);
			/* buf = /usr/local/lib64/libicuio36.0.a(libicuio36.0.a */
			SNPRINTF(buf, ICU_LIBNAME_LEN, "%s(%s", real_path, ptr);
			buflen += (STRLEN(real_path) + STRLEN(ptr) + 1);
			ptr = strrchr(buf, '.');
			strcpy(ptr, ".so)");			/* buf = /usr/local/lib64/libicuio36.0.a(libicuio36.0.so) */
			buflen += STR_LIT_LEN(".so)");
			buf[buflen] = '\0';			/* NULL termination */
			break;
		}
		if (NULL != dyn_search_paths)
			free(dyn_search_paths);
		/* If each_libpath is NULL then we were not able to look for libicuio.a in the loader search path */
		if (NULL == each_libpath)
		{
			ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_DLLNOOPEN, 2, LEN_AND_STR(libname),
				ERR_TEXT, 2, LEN_AND_LIT(ICU_NOT_FOUND_ERR));
		}
		libname = buf;
		handle = dlopen(libname, iculdflags);
	}
#	else
	handle = dlopen(libname, iculdflags);
#	endif
	if (NULL == handle)
	{
		COPY_DLLERR_MSG(err_str, err_msg);
#		ifdef _AIX
		/* On AIX, ICU is sometimes packaged differently where the archived shared library is named as libicuio.so
		 * instead of libicuio36.0.so. Try dlopen with this new naming scheme as well.
		 * Below SNPRINTF converts /usr/local/lib64/libicuio36.0.a to /usr/local/lib64/libicuio36.0.a(libicuio.so)
		 */
		SNPRINTF(temp_path, ICU_LIBNAME_LEN, "%s(%s.so)", real_path, ICU_LIBNAME_ROOT);
		libname = temp_path;
		handle = dlopen(libname, iculdflags);
		if (NULL == handle)
		{
			ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
			libname = buf;
#		endif
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_DLLNOOPEN, 2, LEN_AND_STR(libname),
				ERR_TEXT, 2, LEN_AND_STR(err_msg));
#		ifdef _AIX
		}
#		endif
	}

	/*
	 * Now that we have an ICU library handle, we can check to see if our ICU uses symbol renaming.
	 * And then look up all our required ICU function pointers
	 */
	symbols_renamed = icu_symbols_renamed(handle, icusymver, &icusymver_len);

	for (findx = 0; findx < icu_func_n; ++findx)
	{
		*icu_fptr[findx] = NULL;

		snprintf(icu_final_fname, sizeof(icu_final_fname), "%s%s", icu_fname[findx], symbols_renamed ? icusymver : "");
		fptr = dlsym(handle, icu_final_fname);
		if (NULL == fptr)
		{
			/* we can't find the function at all, either return failure or abort depending on caller */
			if (called_by_zconvert)
				return FALSE;
			COPY_DLLERR_MSG(err_str, err_msg);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ICUSYMNOTFOUND, 2, LEN_AND_STR(icu_final_fname),
				ERR_TEXT, 2, LEN_AND_STR(err_msg));
		}

		/* If we get here, we have a valid ICU function pointer to store */
		*icu_fptr[findx] = (icu_func_t) fptr;
	}

	/* All the ICU function pointers are now available (we would have returned or aborted were that not the case)
	 * First we direct ICU to use the GT.M memory allocation functions.  Next, get & check the ICU version.
	 */
	assert(u_getVersion_ptr && u_setMemoryFunctions_ptr);

	/* Set up memory handling */
	if (!gtm_icu_setmemfunc())
	{
		if (called_by_zconvert)
			return FALSE;

		SNPRINTF(err_msg, sizeof(err_msg), "Could not establish custom memory handling for libicu");
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_DLLNOOPEN, 2, LEN_AND_STR(libname), ERR_TEXT, 2, LEN_AND_STR(err_msg));
	}

	/* Get the ICU version information */
	memset(icu_version, 0, MAX_ICU_VERSION_LENGTH);
	(*u_getVersion_ptr)(icu_version);

	/* Check the ICU version.  We used to accept whatever was set in gtm_icu_version if set,
	 * but we just called the version function, and it is definitive, so use that
	 */
	if (!(IS_ICU_VER_GREATER_THAN_MIN_VER(icu_version[0], icu_version[1])))
	{
		if (called_by_zconvert)
			return FALSE;

		/* Construct the first part of the ICUVERLT36 error message. */
		SNPRINTF(tmp_errstr, SIZEOF(ICU_LIBNAME) + STR_LIT_LEN(ICU_LIBNAME_SUFFIX), "%s%s",
			ICU_LIBNAME, ICU_LIBNAME_SUFFIX);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_ICUVERLT36, 4, LEN_AND_STR(tmp_errstr), icu_version[0], icu_version[1]);
	}

	dollar_zicuver.len = SNPRINTF(dollar_zicuver.addr, MAX_ICU_VERSION_STRLEN, "%hu.%hu", icu_version[0], icu_version[1]);
	if (MAX_ICU_VERSION_STRLEN <= dollar_zicuver.len)
		dollar_zicuver.len = MAX_ICU_VERSION_STRLEN - 1;

	ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
	gtm_utf8_mode = TRUE;
	/* gtm_wcswidth()/U_ISPRINT() in util_format() can henceforth be safely called now that ICU initialization is complete */
	gtm_conv_init();
	return TRUE;
}
