/****************************************************************
 *								*
 *	Copyright 2009, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <dlfcn.h>
#include <errno.h>
#ifdef _AIX
#include <sys/ldr.h>
#endif

#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_limits.h"		/* for GTM_PATH_MAX */

#include "lv_val.h"		/* needed for "fgncal.h" */
#include "real_len.h"
#include "fgncal.h"		/* needed for COPY_DLLERR_MSG() */
#include "gtmmsg.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "trans_log_name.h"
#include "gdsroot.h"
#include "is_file_identical.h"

#define	GTMCRYPT_LIBNAME		"libgtmcrypt.so"
#define MAX_GTMCRYPT_PLUGIN_STR_LEN	(SIZEOF(GTMCRYPT_LIBNAME) * 4)

#define GTM_CRYPT_PLUGIN		"$gtm_crypt_plugin"

typedef void (*gtmcrypt_func_t)();	/* A generic pointer type to the gtmcrypt functions exposed by the plugin */

#define GTMCRYPT_DEF(x) x##_,
enum
{
#include "gtmcrypt_funclist.h"	/* BYPASSOK */
gtmcrypt_func_n			/* total number of gtmcrypt functions that needs to be dlsym()ed */
};
#undef GTMCRYPT_DEF

#define GTMCRYPT_DEF(x) GBLDEF gtmcrypt_func_t x##_fnptr;
#include "gtmcrypt_funclist.h"
#undef GTMCRYPT_DEF

GBLREF	char		dl_err[MAX_ERRSTR_LEN];
GBLREF	char		gtm_dist[GTM_PATH_MAX];
GBLREF	boolean_t	gtm_dist_ok_to_use;

error_def(ERR_CRYPTDLNOOPEN);

/* Including gtmcrypt.h in this module results in conflicting GBLDEF/GBLREFs. So, re-define the function prototype here to
 * silent the compiler.
 */
uint4 gtmcrypt_entry(void);
boolean_t verify_lib_loadpath(const char *libname, char *loadpath);

#ifdef _AIX
/* On AIX, there is no known way to specify that dependent libraries (in this case "libgtmcryptutil.so") should also be searched in
 * the same directory from which the parent library is loaded ($ORIGIN on Linux, HP-UX and Solaris). To work-around that, we
 * explicitly prefix LIBPATH with "$gtm_dist/plugin" before invoking dlopen. But, to ensure that "libgtmcryptutil.so" was indeed
 * loaded from "$gtm_dist/plugin", we use loadquery to get the list of loaded modules along with the path from which they are loaded
 * from and verify against it.
 */
boolean_t verify_lib_loadpath(const char *libname, char *loadpath)
{
	struct ld_xinfo		*ldxinfo;
	char			*bufptr, *ptr, cmpptr[GTM_PATH_MAX], buf[GTM_PATH_MAX];
	int			ret, cmplen, buflen, save_errno;

	buflen = GTM_PATH_MAX;
	bufptr = &buf[0];
	while (-1 == loadquery(L_GETXINFO, bufptr, buflen))
	{
		save_errno = errno;
		if (ENOMEM != save_errno)
		{
			assert(FALSE);
			SNPRINTF(dl_err, MAX_ERRSTR_LEN, "System call `loadquery' failed. %s", STRERROR(save_errno));
			return FALSE;
		}
		if (bufptr != &buf[0])
			free(bufptr);
		buflen *= 2;
		bufptr = malloc(buflen);
	}
	ldxinfo = (struct ld_xinfo *)bufptr;
	ret = FALSE;
	SNPRINTF(cmpptr, GTM_PATH_MAX, "%s/%s", loadpath, libname);
	while (ldxinfo->ldinfo_next)
	{
		/* Point to the offset at which the path of the loaded module is present. */
		ptr = (char *)ldxinfo + ldxinfo->ldinfo_filename;
		if (is_file_identical(cmpptr, ptr))
		{
			ret = TRUE;
			break;
		}
		ldxinfo = (struct ld_xinfo *)((sm_long_t)ldxinfo + ldxinfo->ldinfo_next);
	}
	if (bufptr != &buf[0])
		free(bufptr);
	if (!ret)
		SNPRINTF(dl_err, MAX_ERRSTR_LEN, "Dependent shared library %s was not loaded relative to %s.", libname, loadpath);
	return ret;
}
#endif

uint4 gtmcrypt_entry()
{
	/* Initialize the table of symbol names to be used in dlsym() */
#	define GTMCRYPT_DEF(x) #x,
	char			*gtmcrypt_fname[] = {
#							include "gtmcrypt_funclist.h"
							NULL
				};
#	undef GTMCRYPT_DEF
	/* Initialize the table of locations of function pointers that are set by dlsym() */
#	define GTMCRYPT_DEF(x) &x##_fnptr,
	gtmcrypt_func_t		*gtmcrypt_fnptr[] = {
#							include "gtmcrypt_funclist.h"
							NULL
				};
#	undef GTMCRYPT_DEF
	void_ptr_t		handle;
	char_ptr_t		err_str, libname_ptr;
	gtmcrypt_func_t		fptr;
	int			findx, plugin_dir_len, save_errno;
#	ifdef _AIX
	char			new_libpath_env[GTM_PATH_MAX], *save_libpath_ptr, save_libpath[GTM_PATH_MAX];
#	endif
	char			libpath[GTM_PATH_MAX], buf[MAX_GTMCRYPT_PLUGIN_STR_LEN], plugin_dir_path[GTM_PATH_MAX];
	char			resolved_libpath[GTM_PATH_MAX], resolved_plugin_dir_path[GTM_PATH_MAX];
	mstr			trans, env_var = {0, LEN_AND_LIT(GTM_CRYPT_PLUGIN)};

	if(!gtm_dist_ok_to_use)
	{
		SNPRINTF(dl_err, MAX_ERRSTR_LEN, "%%GTM-E-GTMDISTUNVERIF, Environment variable $gtm_dist (%s) "
				"could not be verified against the executables path", gtm_dist);
		return ERR_CRYPTDLNOOPEN;
	}
	SNPRINTF(plugin_dir_path, GTM_PATH_MAX, "%s/%s", gtm_dist, GTMCRYPT_PLUGIN_DIR_NAME);
	if (NULL == realpath(plugin_dir_path, resolved_plugin_dir_path))
	{
		save_errno = errno;
		SNPRINTF(dl_err, MAX_ERRSTR_LEN, "Failed to find symbolic link for %s. %s", plugin_dir_path, STRERROR(save_errno));
		return ERR_CRYPTDLNOOPEN;
	}
	plugin_dir_len = STRLEN(resolved_plugin_dir_path);
	if ((SS_NORMAL != TRANS_LOG_NAME(&env_var, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)) || (0 >= trans.len))
	{	/* Either $gtm_crypt_plugin is not defined in the environment variable OR it is set to null-string. Fall-back to
		 * using libgtmcrypt.so
		 */
		libname_ptr = GTMCRYPT_LIBNAME;
	} else
		libname_ptr = &buf[0];		/* value of $gtm_crypt_plugin */
	SNPRINTF(libpath, GTM_PATH_MAX, "%s/%s", plugin_dir_path, libname_ptr);
	if (NULL == realpath(libpath, resolved_libpath))
	{
		save_errno = errno;
		SNPRINTF(dl_err, MAX_ERRSTR_LEN, "Failed to find symbolic link for %s. %s", libpath, STRERROR(save_errno));
		return ERR_CRYPTDLNOOPEN;
	}
	/* Symbolic link found. dlopen resolved_libpath */
	if (0 != memcmp(resolved_libpath, resolved_plugin_dir_path, plugin_dir_len))
	{	/* resolved_path doesn't contain $gtm_dist/plugin as the prefix */
		SNPRINTF(dl_err, MAX_ERRSTR_LEN, "Resolved path for %s must be relative to the resolved path for %s",
			libpath, plugin_dir_path);
		return ERR_CRYPTDLNOOPEN;
	}
#	ifdef _AIX
	if (NULL == (save_libpath_ptr = getenv(LIBPATH_ENV)))
		SNPRINTF(new_libpath_env, GTM_PATH_MAX, "%s", plugin_dir_path);
	else
	{
		/* Since the setenv below can potentially thrash the save_libpath_ptr, take a copy of it for later restore. */
		strncpy(save_libpath, save_libpath_ptr, SIZEOF(save_libpath));
		save_libpath[SIZEOF(save_libpath) - 1] = '\0';
		SNPRINTF(new_libpath_env, GTM_PATH_MAX, "%s:%s", plugin_dir_path, save_libpath_ptr);
	}
	setenv(LIBPATH_ENV, new_libpath_env, TRUE);
#	endif
	handle = dlopen(&resolved_libpath[0], RTLD_NOW | RTLD_GLOBAL);
	if (NULL == handle)
	{
		COPY_DLLERR_MSG(err_str, dl_err);
		return ERR_CRYPTDLNOOPEN;
	}
#	ifdef _AIX
	if (NULL == save_libpath_ptr)
		unsetenv(LIBPATH_ENV);
	else
		setenv(LIBPATH_ENV, save_libpath, TRUE);
	if (!verify_lib_loadpath(GTMCRYPT_UTIL_LIBNAME, plugin_dir_path))
		return ERR_CRYPTDLNOOPEN;
#	endif
	for (findx = 0; findx < gtmcrypt_func_n; ++findx)
	{
		fptr = (gtmcrypt_func_t)dlsym(handle, gtmcrypt_fname[findx]);
		if (NULL == fptr)
		{
			COPY_DLLERR_MSG(err_str, dl_err);
			return ERR_CRYPTDLNOOPEN;
		}
		*gtmcrypt_fnptr[findx] = fptr;
	}
	return 0;
}
