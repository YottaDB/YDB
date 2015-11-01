/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "rtnhdr.h"
#include "fgncal.h"
#include <dlfcn.h>
#include "util.h"
#include "gtmmsg.h"

/* below comments applicable only to tru64 */
/* dlsym() is bound to return short pointer because of -taso loader flag. GTMASSERT on this assumption.
 * dlopen() returns a long pointer. All the callers of fgn_getpak() should take care of this.
 * dlclose() uses the handle generated above. So, the same semantics apply.
 * dlerror() returns a char pointer, which is again long.
 */
/* --- end of tru64 comments --- */

/* fgn_getinfo.c - external call lookup package
 *
 * Version Common to AIX, Solaris and DUX
 * Note :- There is another HP-specific version of the same file.
 */

/* Lookup package. Return package handle if success, zero otherwise */

void_ptr_t fgn_getpak(char *package_name)
{
	void_ptr_t 	ret_handle;
	char_ptr_t	dummy_err_str;
	char		*err_str;
	error_def(ERR_TEXT);
	error_def(ERR_DLLNOOPEN);

	if (!(ret_handle = dlopen(package_name, RTLD_LAZY)))
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DLLNOOPEN, 2, RTS_ERROR_STRING(package_name));
		PRN_DLERROR;
	}
	return ret_handle;
}

/* Lookup an external function. Return function address if success, zero otherwise */

fgnfnc fgn_getrtn(void_ptr_t package_handle, mstr *entry_name)
{
	void_ptr_t	sym_addr;
	char_ptr_t	dummy_err_str;
	void		*short_sym_addr;
	char		*err_str;
	error_def(ERR_DLLNORTN);
	error_def(ERR_TEXT);

	if (!(sym_addr = dlsym(package_handle, entry_name->addr)))
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DLLNORTN, 2, RTS_ERROR_STRING(entry_name->addr));
		PRN_DLERROR;
	} else
	{
#ifdef	__osf__
		short_sym_addr = sym_addr;
		if (short_sym_addr != sym_addr)
		{
			util_out_print("Symbol !AD loaded above the lower 31 bit address space", TRUE,
					LEN_AND_STR(entry_name));
					/* once we get the capability of 64 bit pointers for util_out_print,
					 * we can print the actual symbol address */
			GTMASSERT;
		}
#endif
	}
	return (fgnfnc)sym_addr;
}

void fgn_closepak(void_ptr_t package_handle)
{
#ifdef __osf__
	dlclose(package_handle);
#else
  	int 	status;
	char	*err_str;
	error_def(ERR_TEXT);
	error_def(ERR_DLLNOCLOSE);

	status = dlclose(package_handle);
	if (status)
	{
		err_str = dlerror();
		if (err_str)
			gtm_putmsg(VARLSTCNT(6) ERR_DLLNOCLOSE, 0, ERR_TEXT, 2, RTS_ERROR_STRING(err_str));
		else
			gtm_putmsg(VARLSTCNT(1) ERR_DLLNOCLOSE);
	}
#endif
}
