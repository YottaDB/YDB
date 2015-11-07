/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * mubexpfilnam.c
 *
 * Description:	expand the filename with the global mstr directory, save the result in list->backup_file
 *		and set list->backup_to to backup_to_file.
 *
 * Input:	directory		-- where directory name is kept.
 *		list->reg		-- used in the call to mupfndfil() to get the filename
 *		is_directory		-- must have already been set, don't even bother to check
 * Output:	list->backup_to		-- should be set to backup_to_file
 *		list->backup_file	-- should have directory + name of the database file
 */
#include "mdef.h"

#include "gtm_string.h"

#include <rms.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mupipbckup.h"

GBLREF	mstr	directory;
GBLREF	bool	error_mupip;

void mubexpfilnam (backup_reg_list *list)
{
	int		status, len;
	struct FAB	fab;
	struct NAM	nam;
	unsigned char	es[MAX_FN_LEN];
	mstr		file;
	char		*ptr, filename[MAX_FN_LEN];

	file.len = MAX_FN_LEN;
	file.addr = filename;
	if (!mupfndfil(list->reg, &file)) /* mupfndfil prints the error message for non-runtime */
	{
		error_mupip = TRUE;
		return;
	}
	*(file.addr + file.len) = ';';
	file.len++;
	fab = cc$rms_fab;
	nam = cc$rms_nam;
	fab.fab$l_nam = &nam;
	fab.fab$l_fna = file.addr;
	fab.fab$b_fns = file.len;
	nam.nam$b_nop = NAM$M_SYNCHK;
	nam.nam$l_esa = es;
	nam.nam$b_ess = SIZEOF(es);
	status = sys$parse(&fab);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	list->backup_to = backup_to_file;
	len = nam.nam$l_ver + 1 - nam.nam$l_name;
	list->backup_file.len = directory.len + len;
	list->backup_file.addr = malloc(list->backup_file.len+1);
	memcpy (list->backup_file.addr, directory.addr, directory.len);
	memcpy (list->backup_file.addr + directory.len, nam.nam$l_name, len);
	return;
}
