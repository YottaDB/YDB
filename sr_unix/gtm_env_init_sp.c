/****************************************************************
 *								*
 * Copyright (c) 2004-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef __MVS__
#include <env.h>
#endif
#include <errno.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <locale.h>
#include "gtm_stat.h"
#include "gtm_string.h"
#include "gtm_strings.h"
#include "gtm_ctype.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"

#include "gtmio.h"
#include "gtmimagename.h"
#include "ydb_logicals.h"
#include "ydb_trans_numeric.h"
#include "ydb_trans_log_name.h"
#include "ydb_logical_truth_value.h"
#include "iosp.h"		/* for SS_ */
#include "nametabtyp.h"		/* for namelook */
#include "namelook.h"
#include "io.h"
#include "iottdef.h"
#include "gtm_env_init.h"	/* for gtm_env_init() and gtm_env_init_sp() prototype */
#include "gtm_utf8.h"		/* UTF8_NAME */
#include "gtm_zlib.h"
#include "error.h"
#include "gtm_limits.h"
#include "compiler.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "replgbl.h"
#include "gtm_semutils.h"
#include "gtmlink.h"
#include "send_msg.h"
#include "eintr_wrappers.h"
#include "utfcgr.h"
#include "gtm_reservedDB.h"

#define	DEFAULT_NON_BLOCKED_WRITE_RETRIES	10	/* default number of retries */
#ifdef __MVS__
#  define PUTENV_BPXK_MDUMP_PREFIX 		"_BPXK_MDUMP="
#endif

/* Remove trailing '/' from path (unless only '/') */
#define	REMOVE_TRAILING_SLASH_FROM_MSTR(TRANS)				\
{									\
	while ((1 < TRANS.len) && ('/' == TRANS.addr[TRANS.len - 1]))	\
		TRANS.len--;						\
}

GBLREF	uint4			ydb_principal_editing_defaults;	/* ext_cap flags if tt */
GBLREF	boolean_t		is_ydb_chset_utf8;
GBLREF	boolean_t		utf8_patnumeric;
GBLREF	boolean_t		badchar_inhibit;
GBLREF	boolean_t		ydb_quiet_halt;
GBLREF	int			ydb_non_blocked_write_retries;	/* number for retries for non_blocked write to pipe */
GBLREF	char			*gtm_core_file;
GBLREF	char			*gtm_core_putenv;
GBLREF	boolean_t		dmterm_default;
GBLREF	boolean_t		ipv4_only;		/* If TRUE, only use AF_INET. */
ZOS_ONLY(GBLREF	char		*gtm_utf8_locale_object;)
ZOS_ONLY(GBLREF	boolean_t	gtm_tag_utf8_as_ascii;)
GBLREF	volatile boolean_t	timer_in_handler;
#ifdef USE_LIBAIO
GBLREF	char			io_setup_errstr[IO_SETUP_ERRSTR_ARRAYSIZE];
#endif

LITREF	mstr	relink_allowed_mstr[];
LITREF	char	*ydbenvname[YDBENVINDX_MAX_INDEX];
LITREF	char	*gtmenvname[YDBENVINDX_MAX_INDEX];

static readonly nametabent editing_params[] =
{
	{7, "EDITING"},
	{7, "EMPTERM"},
	{6, "INSERT"},
	{9, "NOEDITING"},
	{9, "NOEMPTERM"},
	{8, "NOINSERT"}
};
static readonly unsigned char editing_index[27] =
{
	0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 3,
	3, 3, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6
};

error_def(ERR_INVLOCALE);
error_def(ERR_INVLINKTMPDIR);
error_def(ERR_INVTMPDIR);
error_def(ERR_ARCTLMAXHIGH);
error_def(ERR_ARCTLMAXLOW);

void	gtm_env_init_sp(void)
{	/* Unix only environment initializations */
	mstr		trans;
	int4		status, index, len, hrtbt_cntr_delta, stat_res;
	size_t		cwdlen;
	boolean_t	ret, is_defined, is_ydb_env_match, novalidate;
	char		buf[MAX_SRCLINE + 1], *token, cwd[YDB_PATH_MAX];
	char		*cwdptr, *c, *end, *strtokptr;
	struct stat	outbuf;
	int		ydb_autorelink_shm_min;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef __MVS__
	/* For now OS/390 only. Eventually, this could be added to all UNIX platforms along with the
	 * capability to specify the desired directory to put a core file in. Needs to be setup before
	 * much of anything else is.
	 */
	if (NULL == gtm_core_file)
	{
		token = getcwd(cwd, SIZEOF(cwd));
		if (NULL != token)
		{
			cwdlen = strlen(cwd);
			gtm_core_putenv = malloc(cwdlen + ('/' == cwd[cwdlen - 1] ? 0 : 1) + SIZEOF(GTMCORENAME)
						 + strlen(PUTENV_BPXK_MDUMP_PREFIX));
			MEMCPY_LIT(gtm_core_putenv, PUTENV_BPXK_MDUMP_PREFIX);
			gtm_core_file = cwdptr = gtm_core_putenv + strlen(PUTENV_BPXK_MDUMP_PREFIX);
			memcpy(cwdptr, &cwd, cwdlen);
			cwdptr += cwdlen;
			if ('/' != cwd[cwdlen - 1])
				*cwdptr++ = '/';
			memcpy(cwdptr, GTMCORENAME, SIZEOF(GTMCORENAME));       /* Also copys in trailing null */
		} /* else gtm_core_file/gtm_core_putenv remain null and we likely cannot generate proper core files */
	}
#	endif
	assert(YDB_PATH_MAX <= MAX_SRCLINE);
	/* Validate $ydb_tmp if specified, else that default is available */
	if ((SS_NORMAL != (status = ydb_trans_log_name(YDBENVINDX_TMP, &trans, buf, YDB_PATH_MAX, IGNORE_ERRORS_TRUE, NULL)))
		|| (0 == trans.len))
	{	/* Nothing for $ydb_tmp either - use DEFAULT_GTM_TMP which is already a string */
		MEMCPY_LIT(buf, DEFAULT_GTM_TMP);
		trans.addr = buf;
		trans.len = SIZEOF(DEFAULT_GTM_TMP) - 1;
	}
	assert(YDB_PATH_MAX > trans.len);
	REMOVE_TRAILING_SLASH_FROM_MSTR(trans); /* Remove trailing '/' from trans.addr */
	trans.addr[trans.len] = '\0';
	STAT_FILE(trans.addr, &outbuf, stat_res);
	if ((-1 == stat_res) || !S_ISDIR(outbuf.st_mode))
	{
		/* Either the directory doesn't exist or the specified or defaulted entity is not a directory */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4)	ERR_INVTMPDIR, 2, trans.len, trans.addr);
	}
	/* Check for and and setup ydb_quiet_halt if specified */
	ret = ydb_logical_truth_value(YDBENVINDX_QUIET_HALT, FALSE, &is_defined);
	if (is_defined)
		ydb_quiet_halt = ret;
	/* Initialize local variable null subscripts allowed flag */
	ret = ydb_trans_numeric(YDBENVINDX_LVNULLSUBS, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		/* Not initialized enuf for errors yet so silent rejection of invalid vals */
	TREF(lv_null_subs) = ((is_defined && (LVNULLSUBS_FIRST < ret) && (LVNULLSUBS_LAST > ret)) ? ret : LVNULLSUBS_OK);
	/* ZLIB library compression level */
	ydb_zlib_cmp_level = ydb_trans_numeric(YDBENVINDX_ZLIB_CMP_LEVEL, &is_defined, IGNORE_ERRORS_TRUE, NULL);
	if (YDB_CMPLVL_OUT_OF_RANGE(ydb_zlib_cmp_level))
		ydb_zlib_cmp_level = ZLIB_CMPLVL_MIN;	/* no compression in this case */
	ydb_principal_editing_defaults = 0;
	if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_PRINCIPAL_EDITING, &trans, buf, YDB_PATH_MAX,
												IGNORE_ERRORS_TRUE, NULL)))
	{
		assert(trans.len < YDB_PATH_MAX);
		trans.addr[trans.len] = '\0';
		token = STRTOK_R(trans.addr, ":", &strtokptr);
		while (NULL != token)
		{
			if (ISALPHA_ASCII(token[0]))
				index = namelook(editing_index, editing_params, STR_AND_LEN(token));
			else
				index = -1;	/* ignore this token */
			if (0 <= index)
			{
				switch (index)
				{
					case 0:	/* EDITING */
						ydb_principal_editing_defaults |= TT_EDITING;
						break;
					case 1:	/* EMPTERM */
						ydb_principal_editing_defaults |= TT_EMPTERM;
						break;
					case 2:	/* INSERT */
						ydb_principal_editing_defaults &= ~TT_NOINSERT;
						break;
					case 3:	/* NOEDITING */
						ydb_principal_editing_defaults &= ~TT_EDITING;
						break;
					case 4:	/* NOEMPTERM */
						ydb_principal_editing_defaults &= ~TT_EMPTERM;
						break;
					case 5:	/* NOINSERT */
						ydb_principal_editing_defaults |= TT_NOINSERT;
						break;
				}
			}
			token = STRTOK_R(NULL, ":", &strtokptr);
		}
	}
	if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_CHSET, &trans, buf, YDB_PATH_MAX, IGNORE_ERRORS_TRUE, NULL))
	    && STR_LIT_LEN(UTF8_NAME) == trans.len)
	{
		if (!strncasecmp(buf, UTF8_NAME, STR_LIT_LEN(UTF8_NAME)))
		{
			is_ydb_chset_utf8 = TRUE;
#			ifdef __MVS__
			if ((SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_CHSET_LOCALE, &trans, buf, YDB_PATH_MAX,
													IGNORE_ERRORS_TRUE, NULL)))
				&& (0 < trans.len))
			{	/* full path to 64 bit ASCII UTF-8 locale object */
				gtm_utf8_locale_object = malloc(trans.len + 1);
				STRNCPY_STR(gtm_utf8_locale_object, buf, trans.len);
				gtm_utf8_locale_object[trans.len] = '\0';
			}
			if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_DONT_TAG_UTF8_ASCII, &trans, buf, YDB_PATH_MAX,
													IGNORE_ERRORS_TRUE, NULL)))
			{	/* We to tag UTF8 files as ASCII so we can read them, this var disables that */
				if (status = ydb_logical_truth_value(YDBENVINDX_DONT_TAG_UTF8_ASCII, FALSE, &is_defined)
						&& is_defined)
					gtm_tag_utf8_as_ascii = FALSE;
			}
#			endif
			/* Initialize $ZPATNUMERIC only if $ZCHSET is "UTF-8" */
			if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_PATNUMERIC, &trans, buf, YDB_PATH_MAX,
													IGNORE_ERRORS_TRUE, NULL))
			    && STR_LIT_LEN(UTF8_NAME) == trans.len
			    && !strncasecmp(buf, UTF8_NAME, STR_LIT_LEN(UTF8_NAME)))
			{
				utf8_patnumeric = TRUE;
			}
			status = ydb_logical_truth_value(YDBENVINDX_BADCHAR, TRUE, &is_defined);
			if (is_defined)
				badchar_inhibit = status ? TRUE : FALSE;
		}
	}
	/* Initialize variable that controls number of retries for non-blocked writes to a pipe on unix */
	ydb_non_blocked_write_retries = ydb_trans_numeric(YDBENVINDX_NON_BLOCKED_WRITE_RETRIES, &is_defined,
												IGNORE_ERRORS_TRUE, NULL);
	if (!is_defined)
		ydb_non_blocked_write_retries = DEFAULT_NON_BLOCKED_WRITE_RETRIES;
	/* Initialize variable that controls the behavior on journal error */
	TREF(error_on_jnl_file_lost) = ydb_trans_numeric(YDBENVINDX_ERROR_ON_JNL_FILE_LOST, &is_defined,
											IGNORE_ERRORS_FALSE, NULL);
	if (MAX_JNL_FILE_LOST_OPT < TREF(error_on_jnl_file_lost))
		TREF(error_on_jnl_file_lost) = JNL_FILE_LOST_TURN_OFF; /* default behavior */
	/* Initialize variable that controls jnl release timeout */
	(TREF(replgbl)).jnl_release_timeout = ydb_trans_numeric(YDBENVINDX_JNL_RELEASE_TIMEOUT, &is_defined,
												IGNORE_ERRORS_TRUE, NULL);
	if (!is_defined)
		(TREF(replgbl)).jnl_release_timeout = DEFAULT_JNL_RELEASE_TIMEOUT;
	else if (0 > (TREF(replgbl)).jnl_release_timeout) /* consider negative timeout value as zero */
		(TREF(replgbl)).jnl_release_timeout = 0;
	else if (MAXPOSINT4 / MILLISECS_IN_SEC < (TREF(replgbl)).jnl_release_timeout) /* max value supported for timers */
		(TREF(replgbl)).jnl_release_timeout = MAXPOSINT4 / MILLISECS_IN_SEC;
	/* Initialize variable that controls the maximum time that a process should spend while waiting for semaphores in db_init */
	hrtbt_cntr_delta = ydb_trans_numeric(YDBENVINDX_DB_STARTUP_MAX_WAIT, &is_defined, IGNORE_ERRORS_FALSE, NULL);
	if (!is_defined)
		TREF(dbinit_max_delta_secs) = DEFAULT_DBINIT_MAX_DELTA_SECS;
	else
		TREF(dbinit_max_delta_secs) = hrtbt_cntr_delta;
	/* Initialize variable that controls the location of GT.M custom errors file (used for anticipatory freeze) */
	if ((SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_CUSTOM_ERRORS, &trans, buf, YDB_PATH_MAX,
											IGNORE_ERRORS_TRUE, NULL)))
		&& (0 < trans.len))
	{
		assert(YDB_PATH_MAX > trans.len);
		(TREF(ydb_custom_errors)).addr = malloc(trans.len + 1); /* +1 for '\0'; This memory is never freed */
		(TREF(ydb_custom_errors)).len = trans.len;
		/* For now, we assume that if the environment variable is defined to NULL, anticipatory freeze is NOT in effect */
		if (0 < trans.len)
		{
			memcpy((TREF(ydb_custom_errors)).addr, buf, trans.len);
			((TREF(ydb_custom_errors)).addr)[trans.len] = '\0';
		}
	}
	/* See if ydb_link is set */
	TREF(relink_allowed) = LINK_NORECURSIVE; /* default */
	if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_LINK, &trans, buf, YDB_PATH_MAX, IGNORE_ERRORS_TRUE, NULL)))
	{
		init_relink_allowed(&trans); /* set TREF(relink_allowed) */
	}
#	ifdef AUTORELINK_SUPPORTED
	if (!IS_GTMSECSHR_IMAGE)
	{	/* Set default or supplied value for $ydb_linktmpdir */
		if (SS_NORMAL != (status = ydb_trans_log_name(YDBENVINDX_LINKTMPDIR, &trans, buf, YDB_PATH_MAX,
												IGNORE_ERRORS_TRUE, NULL)))
		{	/* Else use default $ydb_tmp value or its default */
			if ((SS_NORMAL != (status = ydb_trans_log_name(YDBENVINDX_TMP, &trans, buf, YDB_PATH_MAX,
												IGNORE_ERRORS_TRUE, NULL)))
					|| (0 == trans.len))
			{	/* Nothing for $ydb_tmp either - use DEFAULT_GTM_TMP which is already a string */
				trans.addr = DEFAULT_GTM_TMP;
				trans.len = SIZEOF(DEFAULT_GTM_TMP) - 1;
			}
			novalidate = TRUE;		/* Don't validate ydb_linktmpdir if is defaulting to $ydb_tmp */
		} else
			novalidate = FALSE;
		assert(YDB_PATH_MAX > trans.len);
		REMOVE_TRAILING_SLASH_FROM_MSTR(trans); /* Remove trailing '/' from trans.addr */
		(TREF(ydb_linktmpdir)).addr = malloc(trans.len + 1); /* +1 for '\0'; This memory is never freed */
		(TREF(ydb_linktmpdir)).len = trans.len;
		/* For now, we assume that if the environment variable is defined to NULL, anticipatory freeze is NOT in effect */
		if (0 < trans.len)
			memcpy((TREF(ydb_linktmpdir)).addr, trans.addr, trans.len);
		((TREF(ydb_linktmpdir)).addr)[trans.len] = '\0';
		if (!novalidate)
		{	/* $ydb_linktmpdir was specified - validate it (bypass if using $ydb_tmp or its default as that was
			 * already checked earlier.
			 */
			STAT_FILE((TREF(ydb_linktmpdir)).addr, &outbuf, stat_res);
			if ((-1 == stat_res) || !S_ISDIR(outbuf.st_mode))
			{	/* Either the directory doesn't exist or the entity is not a directory */
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4)
					     ERR_INVLINKTMPDIR, 2, (TREF(ydb_linktmpdir)).len, (TREF(ydb_linktmpdir)).addr);
				free((TREF(ydb_linktmpdir)).addr);
				trans.len = SIZEOF(DEFAULT_GTM_TMP) - 1;
				trans.addr = DEFAULT_GTM_TMP;
				REMOVE_TRAILING_SLASH_FROM_MSTR(trans); /* Remove trailing '/' from trans.addr */
				(TREF(ydb_linktmpdir)) = trans;
			}
		}
	}
	/* See if ydb_autorelink_shm is set */
	ydb_autorelink_shm_min = ydb_trans_numeric(YDBENVINDX_AUTORELINK_SHM, &is_defined, IGNORE_ERRORS_TRUE, NULL);
	if (!is_defined || !ydb_autorelink_shm_min)
		TREF(relinkctl_shm_min_index) = 0;
	else
	{
		ydb_autorelink_shm_min = (2 <= ydb_autorelink_shm_min) ? ceil_log2_32bit(ydb_autorelink_shm_min) : 0;
		TREF(relinkctl_shm_min_index) = ydb_autorelink_shm_min;
	}
	/* See if ydb_autorelink_keeprtn is set */
	TREF(ydb_autorelink_keeprtn) = ydb_logical_truth_value(YDBENVINDX_AUTORELINK_KEEPRTN, FALSE, &is_defined);
	if (!is_defined)
		TREF(ydb_autorelink_keeprtn) = FALSE;
	/* See if ydb_autorelink_ctlmax is set */
	TREF(ydb_autorelink_ctlmax) = ydb_trans_numeric(YDBENVINDX_AUTORELINK_CTLMAX, &is_defined,
										IGNORE_ERRORS_TRUE, &is_ydb_env_match);
	if (!is_defined)
		TREF(ydb_autorelink_ctlmax) = RELINKCTL_DEFAULT_ENTRIES;
	else if (TREF(ydb_autorelink_ctlmax) > RELINKCTL_MAX_ENTRIES)
	{
		if (is_ydb_env_match)
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ARCTLMAXHIGH, 4,
				LEN_AND_STR(ydbenvname[YDBENVINDX_AUTORELINK_CTLMAX]),
				TREF(ydb_autorelink_ctlmax), RELINKCTL_MAX_ENTRIES);
		else
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ARCTLMAXHIGH, 4,
				LEN_AND_STR(gtmenvname[YDBENVINDX_AUTORELINK_CTLMAX]),
				TREF(ydb_autorelink_ctlmax), RELINKCTL_MAX_ENTRIES);
		TREF(ydb_autorelink_ctlmax) = RELINKCTL_MAX_ENTRIES;
	} else if (TREF(ydb_autorelink_ctlmax) < RELINKCTL_MIN_ENTRIES)
	{
		if (is_ydb_env_match)
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ARCTLMAXLOW, 4,
				LEN_AND_STR(ydbenvname[YDBENVINDX_AUTORELINK_CTLMAX]),
				TREF(ydb_autorelink_ctlmax), RELINKCTL_MIN_ENTRIES);
		else
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ARCTLMAXLOW, 4,
				LEN_AND_STR(gtmenvname[YDBENVINDX_AUTORELINK_CTLMAX]),
				TREF(ydb_autorelink_ctlmax), RELINKCTL_MIN_ENTRIES);
		TREF(ydb_autorelink_ctlmax) = RELINKCTL_MIN_ENTRIES;
	}
#	endif /* AUTORELINK_SUPPORTED */
#	ifdef DEBUG
	/* DEBUG-only option to bypass 'easy' methods of things and always use gtmsecshr for IPC cleanups, wakeups, file removal,
	 * etc. Basically use gtmsecshr for anything where it is an option - helps with testing gtmsecshr for proper operation.
	 */
	TREF(ydb_usesecshr) = ydb_logical_truth_value(YDBENVINDX_USESECSHR, FALSE, &is_defined);
	if (!is_defined)
		TREF(ydb_usesecshr) = FALSE;
	/* DEBUG-only option to enable/disable anticipatory freeze fake ENOSPC testing */
	TREF(ydb_test_fake_enospc) = ydb_logical_truth_value(YDBENVINDX_TEST_FAKE_ENOSPC, FALSE, &is_defined);
	if (!is_defined)
		TREF(ydb_test_fake_enospc) = FALSE;
	/* DEBUG-only option to enable autorelink on all directories in $zroutines (except for shlib directories) */
	TREF(ydb_test_autorelink_always) = ydb_logical_truth_value(YDBENVINDX_TEST_AUTORELINK_ALWAYS, FALSE, &is_defined);
	if (!is_defined)
		TREF(ydb_test_autorelink_always) = FALSE;
	/* DEBUG-only option to enable counter semaphore to be incremented by more than the default value of 1 */
	ydb_db_counter_sem_incr = ydb_trans_numeric(YDBENVINDX_DB_COUNTER_SEM_INCR, &is_defined, IGNORE_ERRORS_TRUE, NULL);
	if (!is_defined || !ydb_db_counter_sem_incr)
		ydb_db_counter_sem_incr = DEFAULT_DB_COUNTER_SEM_INCR;
	/* DEBUG-only option to force the journal pool accounting out of sync every n transactions. */
	TREF(ydb_test_jnlpool_sync) = ydb_trans_numeric(YDBENVINDX_TEST_JNLPOOL_SYNC, &is_defined, IGNORE_ERRORS_TRUE, NULL);
	if (!is_defined)
		TREF(ydb_test_jnlpool_sync) = 0;
#	endif
#	ifdef GTMDBGFLAGS_ENABLED
	TREF(ydb_dbgflags) = ydb_trans_numeric(YDBENVINDX_DBGFLAGS, &is_defined, IGNORE_ERRORS_TRUE, NULL);
	TREF(ydb_dbgflags_freq) = ydb_trans_numeric(YDBENVINDX_DBGFLAGS_FREQ, &is_defined, IGNORE_ERRORS_TRUE, NULL);
#	endif
	/* See if ydb_ipv4_only is set */
	ipv4_only = ydb_logical_truth_value(YDBENVINDX_IPV4_ONLY, FALSE, NULL);
	/* See if ydb_dmterm is set */
	dmterm_default = ydb_logical_truth_value(YDBENVINDX_DMTERM, FALSE, NULL);
	/* Set values for ydb_utfcgr_strings and ydb_utfcgr_string_groups */
	TREF(ydb_utfcgr_strings) = ydb_trans_numeric(YDBENVINDX_UTFCGR_STRINGS, &is_defined, IGNORE_ERRORS_TRUE, NULL);
	if (!is_defined)
	{
		assert(GTM_UTFCGR_STRINGS_DEFAULT <= GTM_UTFCGR_STRINGS_MAX);
		TREF(ydb_utfcgr_strings) = GTM_UTFCGR_STRINGS_DEFAULT;
	} else if (GTM_UTFCGR_STRINGS_MAX < TREF(ydb_utfcgr_strings))
		TREF(ydb_utfcgr_strings) = GTM_UTFCGR_STRINGS_MAX;
	TREF(ydb_utfcgr_string_groups) = ydb_trans_numeric(YDBENVINDX_UTFCGR_STRING_GROUPS, &is_defined, IGNORE_ERRORS_TRUE, NULL);
	if (!is_defined)
		TREF(ydb_utfcgr_string_groups) = GTM_UTFCGR_STRING_GROUPS_DEFAULT;
	/* If ydb_locale is defined, reset the locale for this process - but only for UTF8 mode */
	if (is_ydb_chset_utf8)
	{
		if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_LOCALE, &trans, buf, YDB_PATH_MAX,
											IGNORE_ERRORS_TRUE, &is_ydb_env_match)))
		{
			if ((0 < trans.len) && (YDB_PATH_MAX > trans.len))
			{	/* Something was specified - need to clear LC_ALL and set LC_CTYPE but need room in buf[]
				 * for string-ending null.
				 */
				putenv("LC_ALL");			/* Clear LC_ALL before LC_CTYPE can take effect */
				buf[trans.len] = '\0';
				status = setenv("LC_CTYPE", buf, TRUE);
				if (0 != status)
				{
					if (is_ydb_env_match)
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_INVLOCALE, 2,
							LEN_AND_STR(ydbenvname[YDBENVINDX_LOCALE]),
							status);
					else
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_INVLOCALE, 2,
							LEN_AND_STR(gtmenvname[YDBENVINDX_LOCALE]),
							status);
				}
			}
		}
	}
#	ifdef 	USE_LIBAIO
	/* Initialize variable that controls the nr_events parameter to io_setup() for linux AIO. */
	TREF(ydb_aio_nr_events) = ydb_trans_numeric(YDBENVINDX_AIO_NR_EVENTS, &is_defined, IGNORE_ERRORS_FALSE, NULL);
	if (!is_defined || (TREF(ydb_aio_nr_events) == 0))
		TREF(ydb_aio_nr_events) = GTM_AIO_NR_EVENTS_DEFAULT;
	/* Populate the io_setup() error string. */
	SNPRINTF(io_setup_errstr, ARRAYSIZE(io_setup_errstr), IO_SETUP_FMT, TREF(ydb_aio_nr_events));
#	endif
	/* Check if ydb_statshare is enabled */
	ret = ydb_logical_truth_value(YDBENVINDX_STATSHARE, FALSE, &is_defined);
	TREF(statshare_opted_in) = (!is_defined) ? NO_STATS_OPTIN : ret ? ALL_STATS_OPTIN : NO_STATS_OPTIN;
	/* Pull in specified ydb_statsdir if specified, else default to ydb_tmp or its default. Note we don't validate the directory
	 * here. It need not exist until a database is opened. If ydb_statsdir does not exist, find an appropriate default and
	 * set it so it is always resolvable.
	 */
	/* Using MAX_FN_LEN below instead of YDB_PATH_MAX because csa->nl->statsdb_fname[] size is MAX_FN_LEN + 1 */
	if ((SS_NORMAL != (status = ydb_trans_log_name(YDBENVINDX_STATSDIR, &trans, buf, MAX_STATSDIR_LEN,
											IGNORE_ERRORS_TRUE, NULL)))
			|| (0 == trans.len))
	{	/* Either no translation for $ydb_statsdir or the current and/or expanded value of $ydb_statsdir exceeds the
		 * max path length. For either case $ydb_statsdir needs to be (re)set so try to use $ydb_tmp instead - note
		 * from here down we'll (re)set $ydb_statsdir so it ALWAYS has a (valid) value for mu_cre_file() to later use.
		 */
		if ((SS_NORMAL != (status = ydb_trans_log_name(YDBENVINDX_TMP, &trans, buf, MAX_STATSDIR_LEN,
											IGNORE_ERRORS_TRUE, NULL)))
				|| (0 == trans.len))
		{	/* Nothing for $ydb_tmp - use DEFAULT_GTM_TMP instead */
			trans.addr = DEFAULT_GTM_TMP;
			trans.len = SIZEOF(DEFAULT_GTM_TMP) - 1;
		}
		/* In the setenv() call below, trans.addr always points to a double quoted string so has a NULL terminator */
		c = (char *)ydbenvname[YDBENVINDX_STATSDIR];
		c++;				/* Bump past the '$' to get to the actual envvar name needed by setenv */
		status = setenv(c, trans.addr, 1);
		assert(0 == status);
	}
	assert(MAX_STATSDIR_LEN >= trans.len);
}
