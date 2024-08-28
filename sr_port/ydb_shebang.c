/****************************************************************
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_limits.h"
#include "gtm_stat.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"

#include "invocation_mode.h"
#include "ydb_shebang.h"
#include "ydb_getenv.h"
#include "eintr_wrappers.h"
#include "zroutines.h"
#include "is_file_identical.h"
#include "gtmio.h"
#include "cenable.h"
#include "io.h"
#include "ydb_trans_log_name.h"
#include "iosp.h"
#include "toktyp.h"
#include "valid_mname.h"

#ifdef DEBUG
GBLREF	unsigned int	invocation_mode;
GBLREF	int		terminal_settings_changed_fd;
GBLREF boolean_t	shebang_invocation;	/* TRUE if yottadb is invoked through the "ydbsh" soft link */
#endif

/* Given a null-terminated M file name ("m_file_name") invoked through shebang (for example as [ydbsh xxx.m],
 * this function does some processing and returns the routine name (xxx) to the caller in the normal case.
 * In the error cases, it returns the full file name [e.g. xxx.m] as is so caller will later issue RUNPARAMERR error.
 * It also sets the "*created_tmpdir" parameter to TRUE if a temporary object directory was created and FALSE otherwise.
 */
char *ydb_shebang(char *m_file_name, boolean_t *created_tmpdir)
{
	char		path_buff[YDB_PATH_MAX];
	char		realpath[YDB_PATH_MAX + MAX_MIDENT_LEN + 3]; /* + 3 is for ".m" and '\0' byte. */
	char		*rtn_path;
	int		ret_stat;
	struct stat	sb;
	char		*rtn_name;
	int		len;
	int		rtn_name_len;
	mstr		srcstr, objstr, mname;
	zro_ent		*srcdir, *objdir;
	char		srcname[MAX_MIDENT_LEN + 3];	/* + 3 for ".m" and '\0' byte */
	char		objname[MAX_MIDENT_LEN + 3];	/* + 3 for ".o" and '\0' byte */
	boolean_t	nopath;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(shebang_invocation);
	assert(MUMPS_RUN == invocation_mode);
	*created_tmpdir = FALSE;
	rtn_name = strrchr(m_file_name, '/');
	if (NULL != rtn_name)	/* slash found, then routine name is the base file name excluding the path to the file */
	{
		rtn_name++;	/* Go past '/' to skip the path and get to the routine name */
		len = STRLEN(rtn_name);
		nopath = FALSE;
	} else
	{
		rtn_name = m_file_name;
		len = STRLEN(m_file_name);
		nopath = TRUE;
	}
	if ((2 > len) || ('.' != rtn_name[len - 2]) || ('m' != rtn_name[len - 1])) /* script name should have a ".m" suffix */
		RTS_ERROR_CSA_ABT(CSA_ARG(NULL) VARLSTCNT(3) ERR_SHEBANGMEXT, 1, rtn_name);
	mname.addr = rtn_name;
	mname.len = len - 2;	/* remove the ".m" extension for "valid_mname()" check */
	if (!valid_mname(&mname))
		RTS_ERROR_CSA_ABT(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOTMNAME, 2, mname.len, mname.addr);
	if (nopath)	/* no slash found, then routine name is the file name */
	{
		char	*path;
		int	path_len;

		/* Find out which directory in PATH env var the .m program was found in and set
		 * "rtn_path" to it. Most of this logic is copied from similar PATH env var
		 * processing code in "iopi_open.c".
		 */
		path = ydb_getenv(YDBENVINDX_GENERIC_PATH, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH);
		if (NULL == path)
			rtn_path = NULL;
		else
		{
			char	*str, *saveptr, *token;

			path_len = STRLEN(path);
			if (YDB_PATH_MAX <= path_len)
				path_len = YDB_PATH_MAX - 1;
			memcpy(path_buff, path, path_len);
			path_buff[path_len] = '\0';
			path = path_buff;
			rtn_path = NULL;
			for (str = path; ; str = NULL)
			{
				token = STRTOK_R(str, ":", &saveptr);
				if (NULL == token)
					break;
				SNPRINTF(realpath, SIZEOF(realpath), "%s/%s", token, m_file_name);
				/* The .m program must be a regular file and with execute permissions
				 * in order for it to be invoked by the shebang approach.
				 */
				STAT_FILE(realpath, &sb, ret_stat);
				if ((0 == ret_stat) && S_ISREG(sb.st_mode) && (sb.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)))
				{
					rtn_path = token;
					break;
				}
			}
		}
	} else
	{	/* The .m program must be a regular file and with execute permissions
		 * in order for it to be invoked by the shebang approach.
		 */
		STAT_FILE(m_file_name, &sb, ret_stat);
		if ((0 == ret_stat) && S_ISREG(sb.st_mode) && (sb.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)))
			rtn_path = m_file_name;	/* Take just the path (not the base file name of the M program) */
		else
			rtn_path = NULL;	/* This program was not invoked by shebang for sure. */
	}
	if (NULL == rtn_path)
	{	/* This program was not invoked by shebang. So return file name as is. */
		return m_file_name;
	}
	rtn_name[len - 2] = '\0';	/* Remove the ".m" extension before doing STRLEN(rtn_name) */
	assert(STRLEN(rtn_name) <= MAX_MIDENT_LEN);
	rtn_name_len = STRLEN(rtn_name);
	memcpy(srcname, rtn_name, rtn_name_len);
	STRCPY(srcname + rtn_name_len, ".m");
	memcpy(objname, rtn_name, rtn_name_len);
	STRCPY(objname + rtn_name_len, ".o");
	srcstr.addr = srcname;
	srcstr.len = rtn_name_len + 2;
	srcdir = (zro_ent *)NULL;
	objstr.addr = objname;
	objstr.len = rtn_name_len + 2;
	objdir = (zro_ent *)NULL;
	zro_search(&objstr, &objdir, &srcstr, &srcdir, FALSE);
	if (!nopath)
		rtn_name[-1] = '\0';	/* Null terminate path so "rtn_path" only contains the path and not the .m file name */
	if ((NULL == srcdir) || !is_file_identical(srcdir->str.addr, rtn_path))
	{	/* The M program invoked through shebang will NOT be found through "$zroutines".
		 * Insert the path of this M program at the start of "$zroutines" so it can be found.
		 * Also create a temporary object directory (under "$ydb_tmp") and add it to "$zroutines".
		 */
		mstr	trans;
		char	buf[YDB_PATH_MAX];
		char	*tempdir, *new_zro;
		int	space_needed;

		if ((SS_NORMAL != ydb_trans_log_name(YDBENVINDX_TMP, &trans, buf, SIZEOF(buf), IGNORE_ERRORS_TRUE, NULL))
			|| (0 == trans.len))
		{	/* $ydb_tmp or $gtm_tmp env var is not defined. Use DEFAULT_GTM_TMP which is already a string */
			MEMCPY_LIT(buf, DEFAULT_GTM_TMP);
			trans.addr = buf;
			trans.len = SIZEOF(DEFAULT_GTM_TMP) - 1;
		}
		SNPRINTF(buf + trans.len, SIZEOF(buf) - trans.len, "/%sXXXXXX", YDBSH);
		tempdir = MKDTEMP(buf);
		if (NULL == tempdir)
		{
			int	save_errno;
			char	buf2[256];

			save_errno = errno;
			SNPRINTF(buf2, SIZEOF(buf2), "mkdtemp(%s)", buf);
			RTS_ERROR_CSA_ABT(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_STR(buf2), CALLFROM, save_errno);
		}
		*created_tmpdir = TRUE;
		space_needed = STRLEN(buf) + 4 + STRLEN(rtn_path) + (TREF(dollar_zroutines)).len; /* 4 for '(', ')', ' ' and '\0 */
		new_zro = malloc(space_needed);
		SNPRINTF(new_zro, space_needed, "%s(%s) %.*s", buf, rtn_path,
			(TREF(dollar_zroutines)).len, (TREF(dollar_zroutines)).addr);
		trans.addr = new_zro;
		trans.len = space_needed - 1;	/* do not take '\0' into account */
		zro_load(&trans);
		free(new_zro);
	}
	return rtn_name;
}

