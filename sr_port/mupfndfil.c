/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "util.h"
#include "parse_file.h"
#include "is_raw_dev.h"
#include "gtmmsg.h"
#include "gtm_file_stat.h"

error_def(ERR_NOUSERDB);
error_def(ERR_REGFILENOTFOUND);

/* mupfndfil.c
 * Description:
 *	For a region find if the corresponding database is present.
 * Arguments:
 *	reg: Region's pointer
 *	filestr: Sent as allocated memory, if returned full path is needed in this mstr
 *	Returns: TRUE if region's database file is found
 *		 FALSE, otherwise
 * Side Effects:
 *	reg->dyn.addr->fname_len and reg->dyn.addr->fname are updated
 */
boolean_t mupfndfil(gd_region *reg, mstr *filestr, boolean_t log_error)
{
	char 	filename[MAX_FN_LEN];
	mstr 	file, def, ret, *retptr;
	uint4	ustatus;

	switch(reg->dyn.addr->acc_meth)
	{
		case dba_mm:
		case dba_bg:
			break;
		default:
			util_out_print("REGION !AD has an unrecognized access method.", TRUE, REG_LEN_STR(reg));
			return FALSE;
	}
	file.addr = (char *)reg->dyn.addr->fname;
	file.len = reg->dyn.addr->fname_len;
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
	if (NULL == filestr)
	{
		ret.len = SIZEOF(filename);
		ret.addr = filename;
		retptr = &ret;
	} else
		retptr = filestr;
	if (FILE_PRESENT != gtm_file_stat(&file, &def, retptr, FALSE, &ustatus))
	{
		if (log_error)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7)
						ERR_REGFILENOTFOUND, 4, LEN_AND_STR(file.addr), REG_LEN_STR(reg), ustatus);
		return FALSE;
	}
	reg->dyn.addr->fname_len = retptr->len;
	memcpy(reg->dyn.addr->fname, retptr->addr, retptr->len + 1);
	return TRUE;
}
