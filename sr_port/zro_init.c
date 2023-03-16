/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
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

#include "iosp.h"
#include "io.h"
#include "zroutines.h"
#include "ydb_trans_log_name.h"
#include "gtm_file_stat.h"

GBLREF boolean_t		is_ydb_chset_utf8;

error_def(ERR_LOGTOOLONG);

#define MAX_NUMBER_FILENAMES	(256 * MAX_TRANS_NAME_LEN)

/* At entry into this function, "ydb_dist" env var would have been defined (either by the user before YottaDB
 * process startup OR by "dlopen_libyottadb" through a "setenv" at image startup. Therefore it is okay to use
 * "$ydb_dist" in the literals below. Those will be expanded by "gtm_file_stat" below.
 */
#define	ZROUTINES_DEFAULT1	"$ydb_dist/libyottadbutil.so"
#define	ZROUTINES_DEFAULT1UTF8	"$ydb_dist/utf8/libyottadbutil.so"
#define	ZROUTINES_DEFAULT2	"$ydb_dist"
#define	ZROUTINES_DEFAULT2UTF8  "$ydb_dist/utf8/"

void zro_init(void)
{
	int4		status;
	mstr		val, tn;
	char		buf1[MAX_NUMBER_FILENAMES]; /* buffer to hold translated name */
	boolean_t	is_ydb_env_match;
	uint4		ustatus;
	mstr		def1, def2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TREF(dollar_zroutines)).addr)
		free((TREF(dollar_zroutines)).addr);
	status = ydb_trans_log_name(YDBENVINDX_ROUTINES, &tn, buf1, SIZEOF(buf1), IGNORE_ERRORS_FALSE, NULL);
	assert((SS_NORMAL == status) || (SS_NOLOGNAM == status));
	if ((0 == tn.len) || (SS_NOLOGNAM == status))
	{	/* "ydb_routines" env var is defined and set to "" OR undefined */
		tn.len = 1;
		tn.addr = buf1;
		MSTR_CONST(ext1, "");
		if (!is_ydb_chset_utf8) /* M Mode */
		{
			MSTR_CONST(def1, ZROUTINES_DEFAULT1);
			if (FILE_PRESENT == gtm_file_stat(&def1, &ext1, NULL, FALSE, &ustatus))
			{	/* "$ydb_dist/libyottadbutil.so" is present. So use it as $zroutines. */
				tn.len = def1.len;
				tn.addr = def1.addr;
			} else
			{	/* "$ydb_dist/libyottadbutil.so" is NOT present. So use "$ydb_dist" as $zroutines. */
				MSTR_CONST(def2, ZROUTINES_DEFAULT2);
				tn.len = def2.len;
				tn.addr = def2.addr;
			}
		} else /* UTF-8 mode */
		{
			MSTR_CONST(def1, ZROUTINES_DEFAULT1UTF8);
			if (FILE_PRESENT == gtm_file_stat(&def1, &ext1, NULL, FALSE, &ustatus))
			{	/* "$ydb_dist/utf8/libyottadbutil.so" is present. So use it as $zroutines. */
				tn.len = def1.len;
				tn.addr = def1.addr;
			} else
			{	/* Try "$ydb_dist/utf8/" */
				MSTR_CONST(def2, ZROUTINES_DEFAULT2UTF8);
				if (FILE_PRESENT == gtm_file_stat(&def2, &ext1, NULL, FALSE, &ustatus))
				{	/* "$ydb_dist/utf8/" is present. So use it as $zroutines. */
					tn.len = def2.len;
					tn.addr = def2.addr;
				} else
				{       /* "$ydb_dist/utf8/" does not exist. Can't use $ydb_dist since it doesn't have UTF-8 objects. */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UTF8NOTINSTALLED) ;
				}
			}
		}
	}
	zro_load(&tn);
}
