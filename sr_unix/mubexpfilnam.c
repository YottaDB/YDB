/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "repl_instance.h"

GBLREF	bool		error_mupip;
GBLREF	backup_reg_list	*mu_repl_inst_reg_list;
GBLREF	gd_addr		*gd_header;

void mubexpfilnam(char *dirname, unsigned int dirlen, backup_reg_list *list)
{
	char	*c1;
	mstr	file;
	char	tmp_mstr_addr[MAX_FN_LEN + 1];


	file.len = MAX_FN_LEN;
	file.addr = tmp_mstr_addr;
	if (list != mu_repl_inst_reg_list)
	{	/* Database region */
		if (!mupfndfil(list->reg, &file, LOG_ERROR_TRUE))
		{
			util_out_print("Backup not finished because of the above error.", TRUE);
			error_mupip = TRUE;
			return;
		}
	} else
	{	/* Replication instance region */
		if (!repl_inst_get_name(file.addr, (unsigned int *)&file.len, MAX_FN_LEN, issue_rts_error, gd_header))
			assertpro(FALSE);	/* rts_error should have been issued by repl_inst_get_name */
	}
	for (c1 = file.addr + file.len; (*c1 != '/') && (c1 != file.addr); c1--)
		;
	list->backup_file.len = INTCAST(dirlen + (file.len - (c1 - file.addr)));
	list->backup_file.addr = (char *)malloc(list->backup_file.len + 1);
	memcpy(list->backup_file.addr, dirname, dirlen);
	memcpy(list->backup_file.addr + dirlen, c1, (file.len - (c1 - file.addr)));
	list->backup_file.addr[list->backup_file.len] = '\0';
	return;
}
