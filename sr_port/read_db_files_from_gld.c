/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.                                     *
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

/* This function returns a list of structures corresponding to regions whose database files were found.
 * In that case, if "max_reg_total" is non-NULL, "*max_reg_total" is initialized to the count of those regions.
 * This function can return NULL if no database files were found. In that case, "*max_reg_total" is left untouched.
 */
gld_dbname_list *read_db_files_from_gld(gd_addr *addr, int *max_reg_total)
{
	gd_segment 		*seg;
	uint4			ustatus;
	gd_region		*reg;
	gd_region		*reg_top;
	gld_dbname_list		head, *dblist = &head;
	char 			filename[MAX_FN_LEN];
	mstr 			file, def, ret, *retptr;
	int			reg_total;

	head.next = NULL;
	reg_total = 0;
	for (reg = addr->regions, reg_top = reg + addr->n_regions; reg < reg_top; reg++)
	{
		boolean_t	is_autoDB;

		assert(reg < reg_top);
		if (IS_STATSDB_REG(reg))
			continue;	/* Do not open statsdb regions directly. They will get opened as needed */
		is_autoDB = IS_AUTODB_REG(reg);
		/* See if region is AutoDB and if corresponding database file actually exists to know whether to include
		 * it in the list or not. If db file does not exist, skip this region and move on to the next region.
		 * LOG_ERROR_FALSE usage below as we do not want error message displayed if file does not exist.
		 * Note: This logic is similar to that in "sr_unix/mu_getlst.c".
		 */
		if (is_autoDB && !mupfndfil(reg, NULL, LOG_ERROR_FALSE))
			continue;	/* autoDB does not exist - do not include */
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
		reg_total++;
	}
	if (NULL == head.next)
	{	/* All regions in the gld are AutoDB regions none of whose database files exist.
		 * Issue error like what "sr_unix/mu_getlst.c" does in this situation.
		 */
		assert(0 == reg_total);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION, 2, LEN_AND_LIT("*"));
	}
	if (NULL != max_reg_total)
		*max_reg_total = reg_total;
	return head.next;
}
