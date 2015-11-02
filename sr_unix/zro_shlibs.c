/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include <dlfcn.h>

#include "lv_val.h"
#include "fgncal.h"
#include "parse_file.h"
#include "zro_shlibs.h"
#include "zroutines.h"
#include "error.h"

error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

/* Routine to lookup given shlib_name to see if we already have it open. If yes, just
 * return its handle. Else, dlopen the shared library and return its handle.
 */
void *zro_shlibs_find(char *shlib_name)
{
	open_shlib	*oshlb;
	void		*handle;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (oshlb = TREF(open_shlib_root); oshlb; oshlb = oshlb->next)
	{	/* Brute force lookup of library - infrequent activity plus few libs mean we
		 * won't make the effort to hash this (typically 0-5 libs max.
		 */
		if (0 == strcmp(shlib_name, oshlb->shlib_name))
		{
			assert(oshlb->shlib_handle);
			return oshlb->shlib_handle;
		}
	}
	/* Library was not found. Open it and create a new entry */
	handle = fgn_getpak(shlib_name, ERROR);
	oshlb = malloc(SIZEOF(open_shlib));
	oshlb->shlib_handle = handle;
	strcpy(oshlb->shlib_name, shlib_name);
	oshlb->next = TREF(open_shlib_root);
	TREF(open_shlib_root) = oshlb;
	return handle;
}


/* Routine called to dlclose() all of the known libraries in our list so they are
 * detached allowing potentially new(er) versions to be linked in.
 */
void zro_shlibs_unlink_all(void)
{
	open_shlib	*oshlb, *oshlb_next;
	char		*dlerr;
	int		status, len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (oshlb = TREF(open_shlib_root); oshlb; oshlb = oshlb_next)
	{	/* Cycle through list close all libraries and releasing the elements */
		oshlb_next = oshlb->next;
		status = dlclose(oshlb->shlib_handle);
		if (0 != status)
		{
			dlerr = dlerror();
			len = STRLEN(dlerr);
			rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("dlclose()"), CALLFROM, ERR_TEXT, 2, len, dlerr);
		}
		free(oshlb);
	}
	TREF(open_shlib_root) = NULL;
	zro_load(TADR(dollar_zroutines));	/* Reloads the shared libraries we need */
}
