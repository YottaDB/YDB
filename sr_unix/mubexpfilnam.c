/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mupipbckup.h"
#include "util.h"

GBLREF	bool	error_mupip;

void mubexpfilnam(char *dirname, unsigned int dirlen, backup_reg_list *list)
{
	char	*c1;
	mstr	file;
	char	tmp_mstr_addr[MAX_FN_LEN];

	file.len = MAX_FN_LEN;
	file.addr = tmp_mstr_addr;
	if (!mupfndfil(list->reg, &file))
	{
		util_out_print("Backup not finished because of the above error.", TRUE);
		error_mupip = TRUE;
		return;
	}
	for (c1 = file.addr + file.len; (*c1 != '/') && (c1 != file.addr); c1--)
                ;
	list->backup_file.len = dirlen + (file.len - (c1 - file.addr));
	list->backup_file.addr = (char *)malloc(list->backup_file.len + 1);
	memcpy(list->backup_file.addr, dirname, dirlen);
	memcpy(list->backup_file.addr + dirlen, c1, (file.len - (c1 - file.addr)));
	return;
}
