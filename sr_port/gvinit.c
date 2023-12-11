/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dpgbldir.h"
#include "gvt_inline.h"
#include "ydb_getenv.h"
#include "op.h"

#define	NOISOLATION_LITERAL	"NOISOLATION"

GBLREF	gd_addr		*gd_header;

void gvinit(void)
{
	mval	v;

	/* if gd_header is null then get the current one */
	if (!gd_header)
	{
		v.mvtype = MV_STR;
		v.str.len = 0;
		v.str.addr = NULL;
		gd_header = zgbldir(&v);
		/* Now that the default global directory has been opened by the process, check if "ydb_app_ensures_isolation"
		 * env var was specified. If so do needed initialization for that here. Cannot do this initialization before
		 * here as it can cause STATSDB related issues (YDB#1047).
		 */
		char	*ptr;
		mval	noiso_lit, gbllist;

		if (NULL != (ptr = ydb_getenv(YDBENVINDX_APP_ENSURES_ISOLATION, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH)))
		{	/* Call the VIEW "NOISOLATION" command with the contents of the <ydb_app_ensures_isolation> env var */
			noiso_lit.mvtype = MV_STR;
			noiso_lit.str.len = STR_LIT_LEN(NOISOLATION_LITERAL);
			noiso_lit.str.addr = NOISOLATION_LITERAL;
			gbllist.mvtype = MV_STR;
			gbllist.str.len = STRLEN(ptr);
			gbllist.str.addr = ptr;
			op_view(VARLSTCNT(2) &noiso_lit, &gbllist);
		}
	}
	/* May get in here after an extended ref call OR in mupip journal recover forward processing (with
	 * function call graph "mur_output_record/gvcst_put/gvtr_init/gvtr_db_tpwrap/op_tstart").
	 * In either case it is possible that gv_currkey has already been set up, so dont lose any preexisting keys.
	 */
	GVKEYSIZE_INIT_IF_NEEDED;	/* sets "gv_keysize", "gv_currkey" and "gv_altkey" (if not already done) */
}
