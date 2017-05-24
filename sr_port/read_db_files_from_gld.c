/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmmsg.h"
#include "read_db_files_from_gld.h"
#include "gtm_file_stat.h"
#include "parse_file.h"
#include "is_raw_dev.h"

error_def(ERR_FILENOTFND);

gld_dbname_list *read_db_files_from_gld(gd_addr *addr)
{
	gd_segment 		*seg;
	uint4			ustatus;
	gd_region		*reg;
	gd_region		*reg_top;
	gld_dbname_list		head, *dblist = &head;
	char 			filename[MAX_FN_LEN];
	mstr 			file, def, ret, *retptr;

	head.next = NULL;
	for (reg = addr->regions, reg_top = reg + addr->n_regions; reg < reg_top; reg++)
	{
		assert(reg < reg_top);
		if (IS_STATSDB_REG(reg))
			continue;	/* Do not open statsdb regions directly. They will get opened as needed */
		seg = (gd_segment *)reg->dyn.addr;
		FILE_CNTL_INIT_IF_NULL(seg);
		ret.len = SIZEOF(filename);
		ret.addr = filename;
		retptr = &ret;
		file.addr = (char *)seg->fname;
		file.len = seg->fname_len;
		file.addr[file.len] = 0;
		if (is_raw_dev(file.addr))
		{
			def.addr = DEF_NODBEXT;
			def.len = SIZEOF(DEF_NODBEXT) - 1;
		} else
		{
			def.addr = DEF_DBEXT;	/* UNIX need to pass "*.dat" but reg->dyn.addr->defext has "DAT" */
			def.len = SIZEOF(DEF_DBEXT) - 1;
		}
		if (FILE_PRESENT != gtm_file_stat(&file, &def, retptr, FALSE, &ustatus))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILENOTFND, 2, file.len, file.addr, ustatus);
			return NULL;
		}
		assert(0 == filename[retptr->len]);
		seg->fname_len = retptr->len;
		memcpy(seg->fname, filename, retptr->len + 1);
		dblist = dblist->next
		    = (gld_dbname_list *)malloc(SIZEOF(gld_dbname_list));
	        memset(dblist, 0, SIZEOF(gld_dbname_list));
		reg->stat.addr = (void *)dblist;
		dblist->gd = reg;
	}
	return head.next;
}
