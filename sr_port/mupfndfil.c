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
#if defined(UNIX)
#include "parse_file.h"
#include "is_raw_dev.h"
#endif
#include "gtmmsg.h"
#include "gtm_file_stat.h"

GBLREF 	jnl_gbls_t	jgbl;

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
boolean_t mupfndfil(gd_region *reg, mstr *filestr)
{
	uint4	ustatus;
	char 	filename[MAX_FN_LEN];
	mstr 	file, def, ret, *retptr;

	switch(reg->dyn.addr->acc_meth)
	{
	case dba_mm:
	case dba_bg:
		break;
	case dba_usr:
		util_out_print("REGION !AD maps to a non-GDS database.  Specified function does not apply to a non-GDS database.",
			TRUE, REG_LEN_STR(reg));
		return FALSE;
	default:
		util_out_print("REGION !AD has an unrecognized access method.", TRUE, REG_LEN_STR(reg));
		return FALSE;
	}
	file.addr = (char *)reg->dyn.addr->fname;
	file.len = reg->dyn.addr->fname_len;
#if defined(UNIX)
	file.addr[file.len] = 0;
	if (is_raw_dev(file.addr))
	{
		def.addr = DEF_NODBEXT;
		def.len = SIZEOF(DEF_NODBEXT) - 1;
	}
	else
	{
		def.addr = DEF_DBEXT;	/* UNIX need to pass "*.dat" but reg->dyn.addr->defext has "DAT" */
		def.len = SIZEOF(DEF_DBEXT) - 1;
	}
#elif defined(VMS)
	def.addr = (char *)reg->dyn.addr->defext;
	def.len = SIZEOF(reg->dyn.addr->defext);
#endif
	if (NULL == filestr)
	{
		ret.len = SIZEOF(filename);
		ret.addr = filename;
		retptr = &ret;
	} else
		retptr = filestr;
	if (FILE_PRESENT != gtm_file_stat(&file, &def, retptr, FALSE, &ustatus))
	{
		if (!jgbl.mupip_journal)
		{	/* Do not print error messages in case of call from mur_open_files().
			 * Currently we use "jgbl.mupip_journal" to identify a call from mupip_recover code */
			util_out_print("REGION !AD's file !AD cannot be found.", TRUE, REG_LEN_STR(reg), LEN_AND_STR(file.addr));
			gtm_putmsg(VARLSTCNT(1) ustatus);
		}
		return FALSE;
	}
	reg->dyn.addr->fname_len = retptr->len;
	memcpy(reg->dyn.addr->fname, retptr->addr, retptr->len + 1);
	return TRUE;
}
