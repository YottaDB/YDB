/****************************************************************
 *								*
 * Copyright (c) 2017-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <grp.h>

#include "gtmio.h"
#include "gtm_common_defs.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_limits.h"
#include "gtm_strings.h"
#include "gtm_malloc.h"
#include "gtm_permissions.h"
#include "gtm_file_remove.h"
#include "gtm_time.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_un.h"
#include "gtm_unistd.h"
#ifdef GTM_TLS
#include "gtm_tls.h"
#endif
#include "send_msg.h"
#include "restrict.h"
#include "iotimer.h"
#include "io.h"
#include "dm_audit_log.h"

#define RESTRICT_FILENAME		"restrict.txt"
#define RESTRICT_BREAK			"BREAK"
#define RESTRICT_ZBREAK			"ZBREAK"
#define RESTRICT_ZEDIT			"ZEDIT"
#define RESTRICT_ZSYSTEM		"ZSYSTEM"
#define RESTRICT_PIPEOPEN		"PIPE_OPEN"
#define RESTRICT_TRIGMOD		"TRIGGER_MOD"
#define RESTRICT_CENABLE		"CENABLE"
#define RESTRICT_DSE			"DSE"
#define RESTRICT_DIRECT_MODE		"DIRECT_MODE"
#define RESTRICT_ZCMDLINE		"ZCMDLINE"
#define RESTRICT_HALT			"HALT"
#define RESTRICT_ZHALT			"ZHALT"
#define ZSYSTEM_FILTER			"ZSYSTEM_FILTER"
#define PIPE_FILTER			"PIPE_FILTER"
#define	RESTRICT_LIBRARY		"LIBRARY"
#define	RESTRICT_LOGDENIALS		"LOGDENIALS"
#define APD_ENABLE			"APD_ENABLE"
#define MAX_READ_SZ			1024	/* Restrict Mnemonic shouldn't exceed this limit */
#define MAX_FACILITY_LEN		64
#define MAX_GROUP_LEN			64
#define	MAX_AUDIT_OPT_LEN		256	/* Maximum length of comma-separated string of direct mode auditing options */
#define	MAX_LOGGER_INFO_LEN		(MAX_AUDIT_OPT_LEN + SA_MAXLEN + NI_MAXSERV + MAX_TLSID_LEN + 3)/* The + 3 accounts for
													 * the 3 ':' (delimiters)
													 */
#define STRINGIFY(S)			#S
#define BOUNDED_FMT(LIMIT,TYPE)		"%" STRINGIFY(LIMIT) TYPE
#define FACILITY_FMTSTR			BOUNDED_FMT(MAX_FACILITY_LEN, "[A-Za-z0-9_]")
#define GROUP_FMTSTR			BOUNDED_FMT(MAX_GROUP_LEN, "[A-Za-z0-9_^%.-]")
/* Direct Mode Auditing options */
#define	AUDIT_OPT_TLS			"TLS"
#define	AUDIT_OPT_TLS_LEN		STR_LIT_LEN(AUDIT_OPT_TLS)
#define AUDIT_OPT_OPREAD		"RD"
#define AUDIT_OPT_OPREAD_LEN		STR_LIT_LEN(AUDIT_OPT_OPREAD)

GBLDEF	struct restrict_facilities	restrictions;
GBLDEF	boolean_t			restrict_initialized;

#ifdef DEBUG
GBLREF	boolean_t			ydb_dist_ok_to_use;
#endif
GBLREF	char				ydb_dist[YDB_PATH_MAX];
GBLREF	boolean_t			dollar_zaudit;

<<<<<<< HEAD
STATICFNDCL void append_filter(char *fpath, char *c_call_name, char *m_ref_name);

=======
>>>>>>> 451ab477 (GT.M V7.0-000)
error_def(ERR_RESTRICTSYNTAX);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_APDINITFAIL);

<<<<<<< HEAD
#define	PUT_FLNAME_IN_MAPPING_FILE(RPATH, FPATH, C_CALL_NAME, M_REF_NAME, CREATED_NOW, CREATED_NOW_INITIALIZED)		\
{															\
	uint4		dummy_stat_rm;											\
	int		status;												\
	struct stat 	rTime, fTime;											\
	struct timespec	rmtime, fmtime;											\
															\
	if (CREATED_NOW_INITIALIZED)											\
	{														\
		/* Note: "CREATED_NOW" is initialized/usable only if "CREATED_NOW_INITIALIZED" is TRUE */		\
		if (CREATED_NOW)											\
		{	/* We created this file in a prior invocation of PUT_FLNAME_IN_MAPPING_FILE			\
			 * so append all future macro invocations into the same file.					\
			 */												\
			append_filter(FPATH, C_CALL_NAME, M_REF_NAME);							\
		}													\
		/* else : We have already determined restrict.txt and filter_commands.tab are in sync time wise.	\
		 *        No need to do any more file modification checks.						\
		 */													\
	} else if (ACCESS(FPATH, F_OK))											\
	{	/* File does not exist, create and write mapping */							\
		CREATED_NOW = TRUE;											\
		CREATED_NOW_INITIALIZED = TRUE;										\
		append_filter(FPATH, C_CALL_NAME, M_REF_NAME);								\
	} else														\
	{	/* Filter file exists. Check modified time */								\
		if (-1 == Stat(RPATH, &rTime))										\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,					\
				LEN_AND_LIT("PUT_FLNAME_IN_MAPPING_FILE() stat() for " RESTRICT_FILENAME), CALLFROM,	\
				errno);											\
		rmtime = rTime.st_mtim;											\
		if (-1 == Stat(FPATH, &fTime))										\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,					\
				LEN_AND_LIT("PUT_FLNAME_IN_MAPPING_FILE() stat() for " COMM_FILTER_FILENAME), CALLFROM,	\
				errno);											\
		fmtime = fTime.st_mtim;											\
		/* Check if restrict.txt file modification time (rmtime) is newer than					\
		 * filter_commands.tab file modification time (fmtime). If so, recreate filter_commands.tab.		\
		 */													\
		if ((rmtime.tv_sec > fmtime.tv_sec)									\
			|| ((rmtime.tv_sec == fmtime.tv_sec) && (rmtime.tv_nsec >= fmtime.tv_nsec)))			\
		{	/* Delete the older mapping file and recreate new if required */				\
			CREATED_NOW = TRUE;										\
			status = gtm_file_remove(FPATH, strlen(FPATH), &dummy_stat_rm);					\
			if (SS_NORMAL != status)									\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,				\
					LEN_AND_LIT("PUT_FLNAME_IN_MAPPING_FILE() gtm_file_remove() for "		\
						    COMM_FILTER_FILENAME), CALLFROM, status);				\
			append_filter(FPATH, C_CALL_NAME, M_REF_NAME);							\
		} else													\
			CREATED_NOW = FALSE;										\
		CREATED_NOW_INITIALIZED = TRUE;										\
	}														\
}

void	append_filter(char *fpath, char *c_call_name, char *m_ref_name)
{
	FILE	*fp;
	char	errstr[MAX_FN_LEN + 1];
	int	fclose_res;

	Fopen(fp, fpath, "a+");
	if (NULL == fp)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				LEN_AND_LIT("append_filter() Fopen() for" COMM_FILTER_FILENAME), CALLFROM, errno);
	FPRINTF(fp, "%s : gtm_long_t* %s(I:gtm_char_t*, O:gtm_string_t*)\n", c_call_name, m_ref_name);
	FCLOSE(fp, fclose_res);
	if (0 != fclose_res)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
				LEN_AND_LIT("append_filter() fclose() for" COMM_FILTER_FILENAME), CALLFROM, errno);
=======
static inline boolean_t OPEN_MAPPING_FILE(struct stat *stat, char *fpath, FILE **fp, int *save_errno, char *errstr, int errstrlen)
{
	int		status;
	struct stat	filtercmdStat;
	boolean_t	createnow = FALSE;

	/* File must not already be open */
	assert(NULL == *fp);
	Fopen(*fp, fpath, "a");
	if (*fp)
	{	/* If this created the file, current position will be zero */
		createnow = (0 == ftell(*fp));
		/* Check if restriction file timestamp changed and refresh as needed */
		FSTAT_FILE(fileno(*fp), &filtercmdStat, status);
		if (stat->st_mtime > filtercmdStat.st_mtime)
		{
			freopen(fpath, "w", *fp);
			if (NULL == *fp)
			{
				*save_errno = errno;
				SNPRINTF(errstr, errstrlen, "freopen() : %s", COMM_FILTER_FILENAME);
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							 LEN_AND_STR(errstr), CALLFROM, *save_errno);
			} else
				createnow = TRUE;
		}
	} else	/* (NULL == *fp) */
	{	/* File append/create failed */
		*save_errno = errno;
		if ((EACCES != *save_errno) && (EROFS != *save_errno))
		{	/* Not having permissions to write the file is common, avoid flooding syslog */
			SNPRINTF(errstr, errstrlen, "fopen() : %s", COMM_FILTER_FILENAME);
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, *save_errno);
		}
	}

	return createnow;
}

#define	UPDATE_FILTCMDMAP_FILE(FP, CREATENOW, C_CALL_NAME, M_REF_NAME)						\
{														\
	if (CREATENOW)	/* Append entry to newly created file */						\
		fprintf(FP, "%s : gtm_long_t* %s(I:gtm_char_t*, O:gtm_string_t*)\n", C_CALL_NAME, M_REF_NAME);	\
>>>>>>> 451ab477 (GT.M V7.0-000)
}

void restrict_init(void)
{
<<<<<<< HEAD
	char		rfpath[YDB_PATH_MAX], rcfpath[YDB_PATH_MAX];
=======
	char		restrictpath[GTM_PATH_MAX], filtcmdpath[GTM_PATH_MAX];
>>>>>>> 451ab477 (GT.M V7.0-000)
	char		logger_info[MAX_LOGGER_INFO_LEN + 1];
	char		linebuf[MAX_READ_SZ + 1], *lbp, facility[MAX_FACILITY_LEN + 1], group_or_flname[MAX_GROUP_LEN + 1];
	char		*host_info, *opt_strt, *apd_opts_strt, *apd_opts_end;
	int		save_errno, fields, status, lineno, logger_info_len, opt_len;
<<<<<<< HEAD
	FILE		*rfp;
	boolean_t	restrict_one, restrict_all = FALSE;
	struct group	grp, *grpres;
	char		*grpbuf = NULL;
	size_t		grpbufsz;
	boolean_t	created_now = FALSE, created_now_initialized = FALSE, tls = FALSE, audit_opread = FALSE;

	assert(!restrict_initialized);
	assert(ydb_dist_ok_to_use);
	SNPRINTF(rfpath, YDB_PATH_MAX, "%s/%s", ydb_dist, RESTRICT_FILENAME);
	SNPRINTF(rcfpath, YDB_PATH_MAX, "%s/%s", ydb_dist, COMM_FILTER_FILENAME);
	if (-1 == ACCESS(rfpath, W_OK))
	{	/* Write access implies no restrictions. Otherwise try reading the file for facilities to restrict. */
		save_errno = errno;
		if ((EACCES == save_errno) || (EROFS == save_errno))
		{	/* The file exists, but we don't have write permissions. Try to read it to determine restricted facilities.
			 * Other errors indicate that the file is unavailable for some reason not associated with permissions,
			 * e.g. missing, so no restrictions.
			 */
			Fopen(rfp, rfpath, "r");
			if (NULL == rfp)
			{	/* Normally, this could mean that the file didn't exist, but since ACCESS() gave us either
				 * EACCES or EROFS above, the file exists, and the current error means we have no read
				 * access. No read access means restrict everything.
				 */
				restrict_all = TRUE;
			} else
			{	/* Read the file, line by line. */
				lineno = 0;
				do
				{
					FGETS_FILE(linebuf, MAX_READ_SZ, rfp, lbp);
					if (NULL != lbp)
					{
						lineno++;
						fields = SSCANF(linebuf, FACILITY_FMTSTR " : " GROUP_FMTSTR,
								facility, group_or_flname);
						if (0 == fields)
							continue;	/* Ignore blank lines */
						else if (0 == STRNCASECMP(facility, APD_ENABLE, STRLEN(APD_ENABLE)))
						{	/* Direct mode auditing entry found */
							if (!IS_MUMPS_IMAGE)
								continue;	/* Skip direct mode auditing entry
										 * when mumps image isn't running
										 */
							fields = SSCANF(linebuf, FACILITY_FMTSTR " : %s",
												facility, logger_info);
							logger_info_len = STRLEN(logger_info);
							if (MAX_LOGGER_INFO_LEN <= logger_info_len)
							{	/* The line is longer than we can store - treat as parse error */
								send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9)
										ERR_RESTRICTSYNTAX, 3,
										LEN_AND_STR(rfpath), lineno,
										ERR_TEXT, 2,
										LEN_AND_LIT("Line too long"));
							}
							apd_opts_strt = (char *)logger_info;	/* Head of comma-separated string */
							/* Look for end (':') of comma-separated options string */
							apd_opts_end = apd_opts_strt;
							while ((apd_opts_strt + logger_info_len > apd_opts_end)
									&& (':' != *apd_opts_end))
								apd_opts_end++;	/* Ignore everything until ':' found */
							if ((apd_opts_strt + logger_info_len <= apd_opts_end)
									|| (':' != *apd_opts_end)
									|| ('\0' == *(apd_opts_end + 1)))
							{	/* End of options list was not found
								 * or reached end of line - parse error
=======
	FILE		*restrictfp, *filtcmdfp = NULL;
	boolean_t	restrict_one, restrict_default = FALSE, restrict_all = FALSE;
	struct group	grp, *grpres;
	char		*grpbuf = NULL;
	size_t		grpbufsz, audit_prefix_len = 0;
	boolean_t	created_now = FALSE, tls = FALSE, audit_opread = FALSE;
	struct stat 	restrictStat;
	uid_t		euid, fuid;
	gid_t		egid, fgid;
	int		rest_owner_perms, rest_group_perms, rest_other_perms;

	assert(!restrict_initialized);
	assert(gtm_dist_ok_to_use);
	SNPRINTF(restrictpath, GTM_PATH_MAX, "%s/%s", gtm_dist, RESTRICT_FILENAME);
	SNPRINTF(filtcmdpath, GTM_PATH_MAX, "%s/%s", gtm_dist, COMM_FILTER_FILENAME);

	Fopen(restrictfp, restrictpath, "r");
	if (NULL == restrictfp)
	{	/* No read access or the file does not exist */
		save_errno = errno;
		if (ENOENT == save_errno)	/* No file implies unrestricted */
			restrict_all = FALSE;
		else if (EACCES == save_errno)	/* Can't read implies no access */
			restrict_all = TRUE;
		else if (ENAMETOOLONG == save_errno)	/* Also a hinky "Can't read" situation */
			restrict_all = TRUE;
		else 				/* Some other reason which we don't expect */
		{
			/* Catch-22: Can't call assert (which is not libc assert()) without setting restrict_initialized */
			restrict_initialized = TRUE;
			restrict_all = TRUE;
			assert(FALSE);
		}
	} else
	{	/* The file exists, check permissions */
		euid = GETEUID();
		egid = GETEGID();
		FSTAT_FILE(fileno(restrictfp), &restrictStat, status);
		if (-1 != status)
		{
			rest_owner_perms = (0200 & restrictStat.st_mode);
			rest_group_perms = (0020 & restrictStat.st_mode);
			rest_other_perms = (0002 & restrictStat.st_mode);
			/* Anyone with write permissions sets the restriction default to FALSE */
			restrict_default = !(rest_other_perms || (rest_owner_perms && (euid == restrictStat.st_uid))
					|| (rest_group_perms
					&& ((egid == restrictStat.st_gid)
						|| (gtm_member_group_id(euid, restrictStat.st_gid, NULL)))));
		} else	/* Treat a stat failure as full restrictions */
			restrict_default = TRUE;
		/* Read the file, line by line. */
		lineno = 0;
		do
		{
			FGETS_FILE(linebuf, MAX_READ_SZ, restrictfp, lbp);
			if (NULL != lbp)
			{
				lineno++;
				fields = SSCANF(linebuf, FACILITY_FMTSTR " : " GROUP_FMTSTR,
									facility, group_or_flname);
				if (0 == fields)
					continue;	/* Ignore blank lines */
				else if (0 == STRNCASECMP(facility, APD_ENABLE, STRLEN(APD_ENABLE)))
				{	/* Direct mode auditing entry found */
					if (!IS_MUMPS_IMAGE)
						continue;	/* Skip direct mode auditing entry
								 * when mumps image isn't running
>>>>>>> 451ab477 (GT.M V7.0-000)
								 */
					fields = SSCANF(linebuf, FACILITY_FMTSTR " : %s",
										facility, logger_info);
					logger_info_len = STRLEN(logger_info);
					if (MAX_LOGGER_INFO_LEN <= logger_info_len)
					{	/* The line is longer than we can store - treat as parse error */
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9)
								ERR_RESTRICTSYNTAX, 3,
								LEN_AND_STR(restrictpath), lineno,
								ERR_TEXT, 2,
								LEN_AND_LIT("Line too long"));
					}
					apd_opts_strt = (char *)logger_info;	/* Head of comma-separated string */
					/* Look for end (':') of comma-separated options string */
					apd_opts_end = apd_opts_strt;
					while ((apd_opts_strt + logger_info_len > apd_opts_end)
							&& (':' != *apd_opts_end))
						apd_opts_end++;	/* Ignore everything until ':' found */
					if ((apd_opts_strt + logger_info_len <= apd_opts_end)
							|| (':' != *apd_opts_end)
							|| ('\0' == *(apd_opts_end + 1)))
					{	/* End of options list was not found
						 * or reached end of line - parse error
						 */
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5)
								ERR_RESTRICTSYNTAX, 3,
								LEN_AND_STR(restrictpath), lineno);
					}
					host_info = apd_opts_end + 1;	/* Head of logger connection info */
					/* Iterate through comma-separated string of options for auditing */
					while (apd_opts_strt < apd_opts_end)
					{
						while ((apd_opts_strt < apd_opts_end) && ((',' == *apd_opts_strt)
									|| (' ' == *apd_opts_strt)
									|| ('\t' == *apd_opts_strt)))
							apd_opts_strt++;	/* Ignore white space and commas */
						/* Now have start of a direct mode auditing option token */
						opt_strt = apd_opts_strt;
						/* Look for token terminator */
						while ((apd_opts_strt < apd_opts_end) && (',' != *apd_opts_strt)
								&& ('\t' != *apd_opts_strt)
								&& (' ' != *apd_opts_strt))
							apd_opts_strt++;	/* Skip char not space or comma */
						/* Have end of token */
						opt_len = apd_opts_strt - opt_strt;
						if (0 == opt_len)
							break;	/* Can happen if value had trailing
								 * space(s) and/or comma(s)
								 */
						if ((apd_opts_strt < apd_opts_end) && (',' == *apd_opts_strt))
							apd_opts_strt++;           /* forward space past comma */
						/* See if this is a valid dmode auditing option */
						if ((AUDIT_OPT_OPREAD_LEN == opt_len)
								&& (0 == STRNCASECMP(opt_strt,
											AUDIT_OPT_OPREAD,
											AUDIT_OPT_OPREAD_LEN)))
							audit_opread = TRUE;
#								ifdef GTM_TLS
						else if ((AUDIT_OPT_TLS_LEN == opt_len)
								&& (0 == STRNCASECMP(opt_strt, AUDIT_OPT_TLS,
											AUDIT_OPT_TLS_LEN)))
							tls = TRUE;
#								endif
<<<<<<< HEAD
								else
								{	/* Invalid option - parse error and restrict everything */
									send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9)
											ERR_RESTRICTSYNTAX, 3,
											LEN_AND_STR(rfpath), lineno,
											ERR_TEXT, 2,
											LEN_AND_LIT("Invalid auditing option"));
									restrict_all = TRUE;
									break;
								}
							}
							if (restrict_all)
								break;
							/* Initialize Direct Mode Auditing with the provided info */
							if (0 > dm_audit_init(host_info, tls))
							{	/* Treat error in auditing initialization as parse error. */
								send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RESTRICTSYNTAX, 3,
										LEN_AND_STR(rfpath), lineno, ERR_APDINITFAIL);
								restrict_all = TRUE;
								break;
							}
							if (audit_opread)
								restrictions.dm_audit_enable = AUDIT_ENABLE_DMRDMODE;
							else
								restrictions.dm_audit_enable = AUDIT_ENABLE_DMODE;
							dollar_zaudit = TRUE;
							continue;
						}
						else if (1 == fields)
							restrict_one = TRUE;
						else if (2 == fields)
						{
							if (NULL == grpbuf)
							{
								SYSCONF(_SC_GETGR_R_SIZE_MAX, grpbufsz);
								grpbuf = malloc(grpbufsz);
							}
							assert(NULL != grpbuf);
							if (0 == STRNCASECMP(facility, ZSYSTEM_FILTER, SIZEOF(ZSYSTEM_FILTER)))
							{
								restrictions.zsy_filter = TRUE;
								PUT_FLNAME_IN_MAPPING_FILE(rfpath, rcfpath,
									ZSY_C_CALL_NAME, group_or_flname,
									created_now, created_now_initialized);
								continue;
							}
							if (0 == STRNCASECMP(facility, PIPE_FILTER, SIZEOF(PIPE_FILTER)))
							{
								restrictions.pipe_filter = TRUE;
								PUT_FLNAME_IN_MAPPING_FILE(rfpath, rcfpath,
									PIPE_C_CALL_NAME, group_or_flname,
									created_now, created_now_initialized);
								continue;
							}
							status = getgrnam_r(group_or_flname, &grp, grpbuf, grpbufsz, &grpres);
							if (0 == status)
							{
								if (NULL == grpres)
								{	/* Treat error in group lookup as parse error. */
									send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9)
											ERR_RESTRICTSYNTAX, 3,
											LEN_AND_STR(rfpath), lineno, ERR_TEXT, 2,
											LEN_AND_LIT("Unknown group"));
									restrict_all = TRUE;
									break;
								} else if ((GETEGID() == grp.gr_gid) || GID_IN_GID_LIST(grp.gr_gid))
									restrict_one = FALSE;
								else
									restrict_one = TRUE;
							} else
							{	/* Treat error in group lookup as parse error. */
								send_msg_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_RESTRICTSYNTAX, 3,
										LEN_AND_STR(rfpath), lineno, ERR_SYSCALL, 5,
										RTS_ERROR_LITERAL("getgrnam_r"), CALLFROM, status);
								restrict_all = TRUE;
								break;
							}
						} else
						{
							restrict_all = TRUE;	/* Parse error - restrict everything */
							break;
						}
						if (0 == STRNCASECMP(facility, RESTRICT_BREAK, SIZEOF(RESTRICT_BREAK)))
							restrictions.break_op = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_ZBREAK, SIZEOF(RESTRICT_ZBREAK)))
							restrictions.zbreak_op = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_ZEDIT, SIZEOF(RESTRICT_ZEDIT)))
							restrictions.zedit_op = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_ZSYSTEM, SIZEOF(RESTRICT_ZSYSTEM)))
							restrictions.zsystem_op = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_PIPEOPEN, SIZEOF(RESTRICT_PIPEOPEN)))
							restrictions.pipe_open = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_TRIGMOD, SIZEOF(RESTRICT_TRIGMOD)))
							restrictions.trigger_mod = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_CENABLE, SIZEOF(RESTRICT_CENABLE)))
							restrictions.cenable = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_DSE, SIZEOF(RESTRICT_DSE)))
							restrictions.dse = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_DIRECT_MODE,
								SIZEOF(RESTRICT_DIRECT_MODE)))
							restrictions.dmode = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_ZCMDLINE, SIZEOF(RESTRICT_ZCMDLINE)))
							restrictions.zcmdline = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_HALT, SIZEOF(RESTRICT_HALT)))
							restrictions.halt_op = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_ZHALT, SIZEOF(RESTRICT_ZHALT)))
							restrictions.zhalt_op = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_LIBRARY, SIZEOF(RESTRICT_LIBRARY)))
							restrictions.library_load_path = restrict_one;
						else if (0 == STRNCASECMP(facility, RESTRICT_LOGDENIALS,
								SIZEOF(RESTRICT_LOGDENIALS)))
							restrictions.logdenials = restrict_one;
=======
>>>>>>> 451ab477 (GT.M V7.0-000)
						else
						{	/* Invalid option - parse error and restrict everything */
							send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9)
									ERR_RESTRICTSYNTAX, 3,
									LEN_AND_STR(restrictpath), lineno,
									ERR_TEXT, 2,
									LEN_AND_LIT("Invalid auditing option"));
							restrict_all = TRUE;
							break;
						}
					}
					if (restrict_all)
						break;
					/* Initialize Direct Mode Auditing with the provided info */
					if (0 > dm_audit_init(host_info, tls))
					{	/* Treat error in auditing initialization as parse error. */
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_RESTRICTSYNTAX, 3,
								LEN_AND_STR(restrictpath), lineno, ERR_APDINITFAIL);
						restrict_all = TRUE;
						break;
					}
					if (audit_opread)
						restrictions.dm_audit_enable = AUDIT_ENABLE_DMRDMODE;
					else
						restrictions.dm_audit_enable = AUDIT_ENABLE_DMODE;
					dollar_zaudit = TRUE;
					continue;
				}
				else if (1 == fields)
					restrict_one = restrict_default;
				else if (2 == fields)
				{
					if (NULL == grpbuf)
					{
						SYSCONF(_SC_GETGR_R_SIZE_MAX, grpbufsz);
						grpbuf = malloc(grpbufsz);
					}
					assert(NULL != grpbuf);
					if (0 == STRNCASECMP(facility, ZSYSTEM_FILTER, SIZEOF(ZSYSTEM_FILTER)))
					{
						restrictions.zsy_filter = TRUE;
						if (NULL == filtcmdfp)
							created_now = OPEN_MAPPING_FILE(&restrictStat, filtcmdpath, &filtcmdfp,
									&save_errno, errstr, MAX_FN_LEN);
						UPDATE_FILTCMDMAP_FILE(filtcmdfp, created_now, ZSY_C_CALL_NAME, group_or_flname);
						continue;
					}
					if (0 == STRNCASECMP(facility, PIPE_FILTER, SIZEOF(PIPE_FILTER)))
					{
						restrictions.pipe_filter = TRUE;
						if (NULL == filtcmdfp)
							created_now = OPEN_MAPPING_FILE(&restrictStat, filtcmdpath, &filtcmdfp,
									&save_errno, errstr, MAX_FN_LEN);
						UPDATE_FILTCMDMAP_FILE(filtcmdfp, created_now, PIPE_C_CALL_NAME, group_or_flname);
						continue;
					}
					status = getgrnam_r(group_or_flname, &grp, grpbuf, grpbufsz, &grpres);
					if (0 == status)
					{
						if (NULL == grpres)
						{	/* Treat error in group lookup as parse error. */
							send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9)
									ERR_RESTRICTSYNTAX, 3,
									LEN_AND_STR(restrictpath), lineno, ERR_TEXT, 2,
									LEN_AND_LIT("Unknown group"));
							restrict_all = TRUE;
							break;
						} else if ((GETEGID() == grp.gr_gid) || GID_IN_GID_LIST(grp.gr_gid))
							restrict_one = FALSE;
						else
							restrict_one = restrict_default;
					} else
					{	/* Treat error in group lookup as parse error. */
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_RESTRICTSYNTAX, 3,
								LEN_AND_STR(restrictpath), lineno, ERR_SYSCALL, 5,
								RTS_ERROR_LITERAL("getgrnam_r"), CALLFROM, status);
						restrict_all = TRUE;
						break;
					}
				} else
				{
					restrict_all = TRUE;	/* Parse error - restrict everything */
					break;
				}
				if (0 == STRNCASECMP(facility, RESTRICT_BREAK, SIZEOF(RESTRICT_BREAK)))
					restrictions.break_op = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_ZBREAK, SIZEOF(RESTRICT_ZBREAK)))
					restrictions.zbreak_op = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_ZEDIT, SIZEOF(RESTRICT_ZEDIT)))
					restrictions.zedit_op = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_ZSYSTEM, SIZEOF(RESTRICT_ZSYSTEM)))
					restrictions.zsystem_op = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_PIPEOPEN, SIZEOF(RESTRICT_PIPEOPEN)))
					restrictions.pipe_open = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_TRIGMOD, SIZEOF(RESTRICT_TRIGMOD)))
					restrictions.trigger_mod = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_CENABLE, SIZEOF(RESTRICT_CENABLE)))
					restrictions.cenable = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_DSE, SIZEOF(RESTRICT_DSE)))
					restrictions.dse = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_DIRECT_MODE,
						SIZEOF(RESTRICT_DIRECT_MODE)))
					restrictions.dmode = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_ZCMDLINE, SIZEOF(RESTRICT_ZCMDLINE)))
					restrictions.zcmdline = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_HALT, SIZEOF(RESTRICT_HALT)))
					restrictions.halt_op = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_ZHALT, SIZEOF(RESTRICT_ZHALT)))
					restrictions.zhalt_op = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_LIBRARY, SIZEOF(RESTRICT_LIBRARY)))
					restrictions.library_load_path = restrict_one;
				else if (0 == STRNCASECMP(facility, RESTRICT_LOGDENIALS,
						SIZEOF(RESTRICT_LOGDENIALS)))
					restrictions.logdenials = restrict_one;
				else
				{	/* Parse error - restrict everything */
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_RESTRICTSYNTAX, 3,
							LEN_AND_STR(restrictpath), lineno);
					restrict_all = TRUE;
					break;
				}
			}
		} while (!restrict_all && !feof(restrictfp));
		if (NULL != grpbuf)
			free(grpbuf);
		/* Do we have write perms for unrestricted access? */
		if ((0x1 & rest_other_perms)	/* Other has write access */
			|| ((0x1 & rest_owner_perms) && (euid == restrictStat.st_uid)) /* euid is owner with write access */
			|| ((0x1 & rest_group_perms) &&
				(gtm_member_group_id(euid, restrictStat.st_gid, NULL)))) /* euid in file group with write access */
			restrict_all = FALSE;
	}
	if (restrict_all)
	{
		restrictions.break_op = TRUE;
		restrictions.zbreak_op = TRUE;
		restrictions.zedit_op = TRUE;
		restrictions.zsystem_op = TRUE;
		restrictions.pipe_open = TRUE;
		restrictions.trigger_mod = TRUE;
		restrictions.cenable = TRUE;
		restrictions.dse = TRUE;
		restrictions.dmode = TRUE;
		restrictions.zcmdline = TRUE;
		restrictions.halt_op = TRUE;
		restrictions.zhalt_op = TRUE;
		restrictions.library_load_path = TRUE;
		restrictions.logdenials = TRUE;
	}
	if (restrictfp)
		FCLOSE(restrictfp, status);
	if (filtcmdfp)
		FCLOSE(filtcmdfp, status);
	restrict_initialized = TRUE;
}
