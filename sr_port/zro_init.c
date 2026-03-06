/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	*
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
#define CHECK_LIBYDBUTIL	"$ydb_dist/libyottadbutil.so"
#define CHECK_PLUGIN_DIR	"$ydb_dist/plugin/o"

#define UTIL_PLUGIN		"$ydb_dist/plugin/o/*.so $ydb_dist/libyottadbutil.so"
#define NO_UTIL_PLUGIN		"$ydb_dist/plugin/o/*.so $ydb_dist"
#define UTIL_NO_PLUGIN		CHECK_LIBYDBUTIL
#define NO_UTIL_NO_PLUGIN	"$ydb_dist"
/* And the UTF-8 versions. */
#define UTF8_CHECK_LIBYDBUTIL	"$ydb_dist/utf8/libyottadbutil.so"
#define UTF8_CHECK_PLUGIN_DIR	"$ydb_dist/plugin/o/utf8"

#define UTF8_UTIL_PLUGIN	"$ydb_dist/plugin/o/utf8/*.so $ydb_dist/utf8/libyottadbutil.so"
#define UTF8_NO_UTIL_PLUGIN	"$ydb_dist/plugin/o/utf8/*.so $ydb_dist/utf8/"
#define UTF8_UTIL_NO_PLUGIN	UTF8_CHECK_LIBYDBUTIL
#define UTF8_NO_UTIL_NO_PLUGIN	"$ydb_dist/utf8/"
/* sets the value of an mstr str from a const char *value. */
#define SET_MSTR_FROM_CONST(string, value)	\
MBSTART {					\
	string.len = SIZEOF(value) -1;		\
	string.addr = value;			\
} MBEND

void zro_init(void)
{
	int4		status;
	mstr		val, tn;
	char		buf1[MAX_NUMBER_FILENAMES]; /* buffer to hold translated name */
	boolean_t	is_ydb_env_match, plugin_exists, libyottadbutil_exists;
	uint4		ustatus;
	mstr		plugin, libyottadbutil, full_default;
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
			SET_MSTR_FROM_CONST(plugin, CHECK_PLUGIN_DIR);
			SET_MSTR_FROM_CONST(libyottadbutil, CHECK_LIBYDBUTIL);
			/* Use "$ydb_dist/plugin/o" in $zroutines if present. */
			plugin_exists = (FILE_PRESENT == gtm_file_stat(&plugin, &ext1, NULL, FALSE, &ustatus));
			/* Use "$ydb_dist/libyottadbutil.so" in $zroutines if present or $ydb_dist if not. */
			libyottadbutil_exists = (FILE_PRESENT == gtm_file_stat(&libyottadbutil, &ext1, NULL, FALSE, &ustatus));
			if (plugin_exists && libyottadbutil_exists)
			{
				SET_MSTR_FROM_CONST(full_default, UTIL_PLUGIN);
			} else if (plugin_exists)
			{
				SET_MSTR_FROM_CONST(full_default, NO_UTIL_PLUGIN);
			} else if (libyottadbutil_exists)
			{
				SET_MSTR_FROM_CONST(full_default, UTIL_NO_PLUGIN);
			} else
			{	/* "$ydb_dist/libyottadbutil.so" and "$ydb_dist/plugin/o" are NOT present.
				 * So use "$ydb_dist" as $zroutines.
				 */
				SET_MSTR_FROM_CONST(full_default, NO_UTIL_NO_PLUGIN);
			}
			tn.len = full_default.len;
			tn.addr = full_default.addr;
		} else /* UTF-8 mode */
		{
			SET_MSTR_FROM_CONST(plugin, UTF8_CHECK_PLUGIN_DIR);
			SET_MSTR_FROM_CONST(libyottadbutil, UTF8_CHECK_LIBYDBUTIL);
			/* Use "$ydb_dist/plugin/o/utf8" in $zroutines if present. */
			plugin_exists = (FILE_PRESENT == gtm_file_stat(&plugin, &ext1, NULL, FALSE, &ustatus));
			/* Use "$ydb_dist/utf8/libyottadbutil.so" in $zroutines if present or $ydb_dist if not. */
			libyottadbutil_exists = (FILE_PRESENT == gtm_file_stat(&libyottadbutil, &ext1, NULL, FALSE, &ustatus));
			if (!libyottadbutil_exists)
			{
				SET_MSTR_FROM_CONST(libyottadbutil, UTF8_NO_UTIL_NO_PLUGIN);
				if (FILE_PRESENT != gtm_file_stat(&libyottadbutil, &ext1, NULL, FALSE, &ustatus))
				{	/* "$ydb_dist/utf8/" does not exist.
					 * Can't use $ydb_dist since it doesn't have UTF-8 objects.
					 */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UTF8NOTINSTALLED) ;
				}
			}
			if (plugin_exists && libyottadbutil_exists)
			{
				SET_MSTR_FROM_CONST(full_default, UTF8_UTIL_PLUGIN);
			} else if (plugin_exists)
			{
				SET_MSTR_FROM_CONST(full_default, UTF8_NO_UTIL_PLUGIN);
			} else if (libyottadbutil_exists)
			{
				SET_MSTR_FROM_CONST(full_default, UTF8_UTIL_NO_PLUGIN);
			} else
			{	/* "$ydb_dist/utf8/libyottadbutil.so" and "$ydb_dist/plugin/o/utf8" are NOT present.
				 * So use "$ydb_dist/utf8/" as $zroutines.
				 */
				SET_MSTR_FROM_CONST(full_default, UTF8_NO_UTIL_NO_PLUGIN);
			}
			tn.len = full_default.len;
			tn.addr = full_default.addr;
		}
	}
	zro_load(&tn);
}
