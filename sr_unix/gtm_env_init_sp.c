/****************************************************************
 *								*
 * Copyright (c) 2004-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_logicals.h"
#include "trans_numeric.h"
#include "trans_log_name.h"
#include "logical_truth_value.h"
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
#ifdef __linux__
#include "hugetlbfs_overrides.h"
#endif

#define	DEFAULT_NON_BLOCKED_WRITE_RETRIES	10	/* default number of retries */
#ifdef __MVS__
#  define PUTENV_BPXK_MDUMP_PREFIX 		"_BPXK_MDUMP="
#endif
#ifdef DEBUG
/* Note the var below is NOT located in gtm_logicals because it is DEBUG-only which would screw-up
 * regresion test v53003/D9I10002703.
 */
# define GTM_USESECSHR				"$gtm_usesecshr"
/* GTM_TEST_FAKE_ENOSPC is used only in debug code so it does not have to go in gtm_logicals.h */
# define GTM_TEST_FAKE_ENOSPC			"$gtm_test_fake_enospc"
/* GTM_TEST_AUTORELINK_ALWAYS is used only in debug code so it does not have to go in gtm_logicals.h */
# define GTM_TEST_AUTORELINK_ALWAYS		"$gtm_test_autorelink_always"
/* GTM_DB_COUNTER_SEM_INCR is used only in debug code so it does not have to go in gtm_logicals.h */
# define GTM_DB_COUNTER_SEM_INCR		"$gtm_db_counter_sem_incr"
/* GTM_TEST_JNLPOOL_SYNC is used only in debug code so it does not have to go in gtm_logicals.h */
# define GTM_TEST_JNLPOOL_SYNC			"$gtm_test_jnlpool_sync"
#endif

/* Remove trailing '/' from path (unless only '/') */
#define	REMOVE_TRAILING_SLASH_FROM_MSTR(TRANS)				\
{									\
	while ((1 < TRANS.len) && ('/' == TRANS.addr[TRANS.len - 1]))	\
		TRANS.len--;						\
}

GBLREF	uint4			gtm_principal_editing_defaults;	/* ext_cap flags if tt */
GBLREF	boolean_t		is_gtm_chset_utf8;
GBLREF	boolean_t		utf8_patnumeric;
GBLREF	boolean_t		badchar_inhibit;
GBLREF	boolean_t		gtm_quiet_halt;
GBLREF	int			gtm_non_blocked_write_retries;	/* number for retries for non_blocked write to pipe */
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

LITREF mstr relink_allowed_mstr[];

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
	mstr		val, trans;
	int4		status, index, len, hrtbt_cntr_delta, stat_res;
	size_t		cwdlen;
	boolean_t	ret, is_defined, novalidate;
	char		buf[MAX_SRCLINE + 1], *token, cwd[GTM_PATH_MAX];
	char		*cwdptr, *c, *end, *strtokptr;
	struct stat	outbuf;
	int		gtm_autorelink_shm_min;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef HUGETLB_SUPPORTED
	libhugetlbfs_init();
#	endif
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
	assert(GTM_PATH_MAX <= MAX_SRCLINE);
	/* Validate $gtm_tmp if specified, else that default is available */
	val.addr = GTM_TMP_ENV;
	val.len = SIZEOF(GTM_TMP_ENV) - 1;
	if ((SS_NORMAL != (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long))) || (0 == trans.len))
	{	/* Nothing for $gtm_tmp either - use DEFAULT_GTM_TMP which is already a string */
		MEMCPY_LIT(buf, DEFAULT_GTM_TMP);
		trans.addr = buf;
		trans.len = SIZEOF(DEFAULT_GTM_TMP) - 1;
	}
	assert(GTM_PATH_MAX > trans.len);
	REMOVE_TRAILING_SLASH_FROM_MSTR(trans); /* Remove trailing '/' from trans.addr */
	trans.addr[trans.len] = '\0';
	STAT_FILE(trans.addr, &outbuf, stat_res);
	if ((-1 == stat_res) || !S_ISDIR(outbuf.st_mode))
	{
		/* Either the directory doesn't exist or the specified or defaulted entity is not a directory */
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4)	ERR_INVTMPDIR, 2, trans.len, trans.addr);
	}
	/* Check for and and setup gtm_quiet_halt if specified */
	val.addr = GTM_QUIET_HALT;
	val.len = SIZEOF(GTM_QUIET_HALT) - 1;
	ret = logical_truth_value(&val, FALSE, &is_defined);
	if (is_defined)
		gtm_quiet_halt = ret;
	/* Initialize local variable null subscripts allowed flag */
	val.addr = GTM_LVNULLSUBS;
	val.len = SIZEOF(GTM_LVNULLSUBS) - 1;
	ret = trans_numeric(&val, &is_defined, TRUE); /* Not initialized enuf for errors yet so silent rejection of invalid vals */
	TREF(lv_null_subs) = ((is_defined && (LVNULLSUBS_FIRST < ret) && (LVNULLSUBS_LAST > ret)) ? ret : LVNULLSUBS_OK);
	/* ZLIB library compression level */
	val.addr = GTM_ZLIB_CMP_LEVEL;
	val.len = SIZEOF(GTM_ZLIB_CMP_LEVEL) - 1;
	gtm_zlib_cmp_level = trans_numeric(&val, &is_defined, TRUE);
	if (GTM_CMPLVL_OUT_OF_RANGE(gtm_zlib_cmp_level))
		gtm_zlib_cmp_level = ZLIB_CMPLVL_MIN;	/* no compression in this case */
	gtm_principal_editing_defaults = 0;
	val.addr = GTM_PRINCIPAL_EDITING;
	val.len = SIZEOF(GTM_PRINCIPAL_EDITING) - 1;
	if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long)))
	{
		assert(trans.len < GTM_PATH_MAX);
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
						gtm_principal_editing_defaults |= TT_EDITING;
						break;
					case 1:	/* EMPTERM */
						gtm_principal_editing_defaults |= TT_EMPTERM;
						break;
					case 2:	/* INSERT */
						gtm_principal_editing_defaults &= ~TT_NOINSERT;
						break;
					case 3:	/* NOEDITING */
						gtm_principal_editing_defaults &= ~TT_EDITING;
						break;
					case 4:	/* NOEMPTERM */
						gtm_principal_editing_defaults &= ~TT_EMPTERM;
						break;
					case 5:	/* NOINSERT */
						gtm_principal_editing_defaults |= TT_NOINSERT;
						break;
				}
			}
			token = STRTOK_R(NULL, ":", &strtokptr);
		}
	}
	val.addr = GTM_CHSET_ENV;
	val.len = STR_LIT_LEN(GTM_CHSET_ENV);
	if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long))
	    && STR_LIT_LEN(UTF8_NAME) == trans.len)
	{
		if (!strncasecmp(buf, UTF8_NAME, STR_LIT_LEN(UTF8_NAME)))
		{
			is_gtm_chset_utf8 = TRUE;
#			ifdef __MVS__
			val.addr = GTM_CHSET_LOCALE_ENV;
			val.len = STR_LIT_LEN(GTM_CHSET_LOCALE_ENV);
			if ((SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long))) &&
			    (0 < trans.len))
			{	/* full path to 64 bit ASCII UTF-8 locale object */
				gtm_utf8_locale_object = malloc(trans.len + 1);
				STRNCPY_STR(gtm_utf8_locale_object, buf, trans.len);
				gtm_utf8_locale_object[trans.len] = '\0';
			}
			val.addr = GTM_TAG_UTF8_AS_ASCII;
			val.len = STR_LIT_LEN(GTM_TAG_UTF8_AS_ASCII);
			if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long)))
			{	/* We to tag UTF8 files as ASCII so we can read them, this var disables that */
				if (status = logical_truth_value(&val, FALSE, &is_defined) && is_defined)
					gtm_tag_utf8_as_ascii = FALSE;
			}
#			endif
			/* Initialize $ZPATNUMERIC only if $ZCHSET is "UTF-8" */
			val.addr = GTM_PATNUMERIC_ENV;
			val.len = STR_LIT_LEN(GTM_PATNUMERIC_ENV);
			if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long))
			    && STR_LIT_LEN(UTF8_NAME) == trans.len
			    && !strncasecmp(buf, UTF8_NAME, STR_LIT_LEN(UTF8_NAME)))
			{
				utf8_patnumeric = TRUE;
			}
			val.addr = GTM_BADCHAR_ENV;
			val.len = STR_LIT_LEN(GTM_BADCHAR_ENV);
			status = logical_truth_value(&val, TRUE, &is_defined);
			if (is_defined)
				badchar_inhibit = status ? TRUE : FALSE;
		}
	}
	/* Initialize variable that controls number of retries for non-blocked writes to a pipe on unix */
	val.addr = GTM_NON_BLOCKED_WRITE_RETRIES;
	val.len = SIZEOF(GTM_NON_BLOCKED_WRITE_RETRIES) - 1;
	gtm_non_blocked_write_retries = trans_numeric(&val, &is_defined, TRUE);
	if (!is_defined)
		gtm_non_blocked_write_retries = DEFAULT_NON_BLOCKED_WRITE_RETRIES;
	/* Initialize variable that controls the behavior on journal error */
	val.addr = GTM_ERROR_ON_JNL_FILE_LOST;
	val.len = SIZEOF(GTM_ERROR_ON_JNL_FILE_LOST) - 1;
	TREF(error_on_jnl_file_lost) = trans_numeric(&val, &is_defined, FALSE);
	if (MAX_JNL_FILE_LOST_OPT < TREF(error_on_jnl_file_lost))
		TREF(error_on_jnl_file_lost) = JNL_FILE_LOST_TURN_OFF; /* default behavior */
	/* Initialize variable that controls jnl release timeout */
	val.addr = GTM_JNL_RELEASE_TIMEOUT;
	val.len = SIZEOF(GTM_JNL_RELEASE_TIMEOUT) - 1;
	(TREF(replgbl)).jnl_release_timeout = trans_numeric(&val, &is_defined, TRUE);
	if (!is_defined)
		(TREF(replgbl)).jnl_release_timeout = DEFAULT_JNL_RELEASE_TIMEOUT;
	else if (0 > (TREF(replgbl)).jnl_release_timeout) /* consider negative timeout value as zero */
		(TREF(replgbl)).jnl_release_timeout = 0;
	else if (MAXPOSINT4 / MILLISECS_IN_SEC < (TREF(replgbl)).jnl_release_timeout) /* max value supported for timers */
		(TREF(replgbl)).jnl_release_timeout = MAXPOSINT4 / MILLISECS_IN_SEC;
	/* Initialize variable that controls the maximum time that a process should spend while waiting for semaphores in db_init */
	val.addr = GTM_DB_STARTUP_MAX_WAIT;
	val.len = SIZEOF(GTM_DB_STARTUP_MAX_WAIT) - 1;
	hrtbt_cntr_delta = trans_numeric(&val, &is_defined, FALSE);
	if (!is_defined)
		TREF(dbinit_max_delta_secs) = DEFAULT_DBINIT_MAX_DELTA_SECS;
	else
		TREF(dbinit_max_delta_secs) = hrtbt_cntr_delta;
	/* Initialize variable that controls the location of GT.M custom errors file (used for anticipatory freeze) */
	val.addr = GTM_CUSTOM_ERRORS;
	val.len = SIZEOF(GTM_CUSTOM_ERRORS) - 1;
	if ((SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long))) && (0 < trans.len))
	{
		assert(GTM_PATH_MAX > trans.len);
		(TREF(gtm_custom_errors)).addr = malloc(trans.len + 1); /* +1 for '\0'; This memory is never freed */
		(TREF(gtm_custom_errors)).len = trans.len;
		/* For now, we assume that if the environment variable is defined to NULL, anticipatory freeze is NOT in effect */
		if (0 < trans.len)
		{
			memcpy((TREF(gtm_custom_errors)).addr, buf, trans.len);
			((TREF(gtm_custom_errors)).addr)[trans.len] = '\0';
		}
	}
	/* See if gtm_link is set */
	val.addr = GTM_LINK;
	val.len = SIZEOF(GTM_LINK) - 1;
	TREF(relink_allowed) = LINK_NORECURSIVE; /* default */
	if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long)))
	{
		init_relink_allowed(&trans); /* set TREF(relink_allowed) */
	}
#	ifdef AUTORELINK_SUPPORTED
	if (!IS_GTMSECSHR_IMAGE)
	{	/* Set default or supplied value for $gtm_linktmpdir */
		val.addr = GTM_LINKTMPDIR;
		val.len = SIZEOF(GTM_LINKTMPDIR) - 1;
		if (SS_NORMAL != (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long)))
		{	/* Else use default $gtm_tmp value or its default */
			val.addr = GTM_TMP_ENV;
			val.len = SIZEOF(GTM_TMP_ENV) - 1;
			if ((SS_NORMAL != (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long)))
					|| (0 < trans.len))
			{	/* Nothing for $gtm_tmp either - use DEFAULT_GTM_TMP which is already a string */
				trans.addr = DEFAULT_GTM_TMP;
				trans.len = SIZEOF(DEFAULT_GTM_TMP) - 1;
			}
			novalidate = TRUE;		/* Don't validate gtm_linktmpdir if is defaulting to $gtm_tmp */
		} else
			novalidate = FALSE;
		assert(GTM_PATH_MAX > trans.len);
		REMOVE_TRAILING_SLASH_FROM_MSTR(trans); /* Remove trailing '/' from trans.addr */
		(TREF(gtm_linktmpdir)).addr = malloc(trans.len + 1); /* +1 for '\0'; This memory is never freed */
		(TREF(gtm_linktmpdir)).len = trans.len;
		/* For now, we assume that if the environment variable is defined to NULL, anticipatory freeze is NOT in effect */
		if (0 < trans.len)
			memcpy((TREF(gtm_linktmpdir)).addr, trans.addr, trans.len);
		((TREF(gtm_linktmpdir)).addr)[trans.len] = '\0';
		if (!novalidate)
		{	/* $gtm_linktmpdir was specified - validate it (bypass if using $gtm_tmp or its default as that was
			 * already checked earlier.
			 */
			STAT_FILE((TREF(gtm_linktmpdir)).addr, &outbuf, stat_res);
			if ((-1 == stat_res) || !S_ISDIR(outbuf.st_mode))
			{	/* Either the directory doesn't exist or the entity is not a directory */
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4)
					     ERR_INVLINKTMPDIR, 2, (TREF(gtm_linktmpdir)).len, (TREF(gtm_linktmpdir)).addr);
				free((TREF(gtm_linktmpdir)).addr);
				trans.len = SIZEOF(DEFAULT_GTM_TMP) - 1;
				trans.addr = DEFAULT_GTM_TMP;
				REMOVE_TRAILING_SLASH_FROM_MSTR(trans); /* Remove trailing '/' from trans.addr */
				(TREF(gtm_linktmpdir)) = trans;
			}
		}
	}
	/* See if gtm_autorelink_shm is set */
	val.addr = GTM_AUTORELINK_SHM;
	val.len = SIZEOF(GTM_AUTORELINK_SHM) - 1;
	gtm_autorelink_shm_min = trans_numeric(&val, &is_defined, TRUE);
	if (!is_defined || !gtm_autorelink_shm_min)
		TREF(relinkctl_shm_min_index) = 0;
	else
	{
		gtm_autorelink_shm_min = (2 <= gtm_autorelink_shm_min) ? ceil_log2_32bit(gtm_autorelink_shm_min) : 0;
		TREF(relinkctl_shm_min_index) = gtm_autorelink_shm_min;
	}
	/* See if gtm_autorelink_keeprtn is set */
	val.addr = GTM_AUTORELINK_KEEPRTN;
	val.len = SIZEOF(GTM_AUTORELINK_KEEPRTN) - 1;
	TREF(gtm_autorelink_keeprtn) = logical_truth_value(&val, FALSE, &is_defined);
	if (!is_defined)
		TREF(gtm_autorelink_keeprtn) = FALSE;
	/* See if gtm_autorelink_ctlmax is set */
	val.addr = GTM_AUTORELINK_CTLMAX;
	val.len = SIZEOF(GTM_AUTORELINK_CTLMAX) - 1;
	TREF(gtm_autorelink_ctlmax) = trans_numeric(&val, &is_defined, TRUE);
	if (!is_defined)
		TREF(gtm_autorelink_ctlmax) = RELINKCTL_DEFAULT_ENTRIES;
	else if (TREF(gtm_autorelink_ctlmax) > RELINKCTL_MAX_ENTRIES)
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ARCTLMAXHIGH, 4, LEN_AND_LIT(GTM_AUTORELINK_CTLMAX),
			     TREF(gtm_autorelink_ctlmax), RELINKCTL_MAX_ENTRIES);
		TREF(gtm_autorelink_ctlmax) = RELINKCTL_MAX_ENTRIES;
	} else if (TREF(gtm_autorelink_ctlmax) < RELINKCTL_MIN_ENTRIES)
	{
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ARCTLMAXLOW, 4, LEN_AND_LIT(GTM_AUTORELINK_CTLMAX),
			     TREF(gtm_autorelink_ctlmax), RELINKCTL_MIN_ENTRIES);
		TREF(gtm_autorelink_ctlmax) = RELINKCTL_MIN_ENTRIES;
	}
#	endif /* AUTORELINK_SUPPORTED */
#	ifdef DEBUG
	/* DEBUG-only option to bypass 'easy' methods of things and always use gtmsecshr for IPC cleanups, wakeups, file removal,
	 * etc. Basically use gtmsecshr for anything where it is an option - helps with testing gtmsecshr for proper operation.
	 */
	val.addr = GTM_USESECSHR;
	val.len = SIZEOF(GTM_USESECSHR) - 1;
	TREF(gtm_usesecshr) = logical_truth_value(&val, FALSE, &is_defined);
	if (!is_defined)
		TREF(gtm_usesecshr) = FALSE;
	/* DEBUG-only option to enable/disable anticipatory freeze fake ENOSPC testing */
	val.addr = GTM_TEST_FAKE_ENOSPC;
	val.len = SIZEOF(GTM_TEST_FAKE_ENOSPC) - 1;
	TREF(gtm_test_fake_enospc) = logical_truth_value(&val, FALSE, &is_defined);
	if (!is_defined)
		TREF(gtm_test_fake_enospc) = FALSE;
	/* DEBUG-only option to enable autorelink on all directories in $zroutines (except for shlib directories) */
	val.addr = GTM_TEST_AUTORELINK_ALWAYS;
	val.len = SIZEOF(GTM_TEST_AUTORELINK_ALWAYS) - 1;
	TREF(gtm_test_autorelink_always) = logical_truth_value(&val, FALSE, &is_defined);
	if (!is_defined)
		TREF(gtm_test_autorelink_always) = FALSE;
	/* DEBUG-only option to enable counter semaphore to be incremented by more than the default value of 1 */
	val.addr = GTM_DB_COUNTER_SEM_INCR;
	val.len = SIZEOF(GTM_DB_COUNTER_SEM_INCR) - 1;
	gtm_db_counter_sem_incr = trans_numeric(&val, &is_defined, TRUE);
	if (!is_defined || !gtm_db_counter_sem_incr)
		gtm_db_counter_sem_incr = DEFAULT_DB_COUNTER_SEM_INCR;
	/* DEBUG-only option to force the journal pool accounting out of sync every n transactions. */
	val.addr = GTM_TEST_JNLPOOL_SYNC;
	val.len = SIZEOF(GTM_TEST_JNLPOOL_SYNC) - 1;
	TREF(gtm_test_jnlpool_sync) = trans_numeric(&val, &is_defined, TRUE);
	if (!is_defined)
		TREF(gtm_test_jnlpool_sync) = 0;
#	endif
#	ifdef GTMDBGFLAGS_ENABLED
	val.addr = GTMDBGFLAGS;
	val.len = SIZEOF(GTMDBGFLAGS) - 1;
	TREF(gtmdbgflags) = trans_numeric(&val, &is_defined, TRUE);
	val.addr = GTMDBGFLAGS_FREQ;
	val.len = SIZEOF(GTMDBGFLAGS_FREQ) - 1;
	TREF(gtmdbgflags_freq) = trans_numeric(&val, &is_defined, TRUE);
#	endif
	/* See if gtm_ipv4_only is set */
	val.addr = GTM_IPV4_ONLY;
	val.len = SIZEOF(GTM_IPV4_ONLY) - 1;
	ipv4_only = logical_truth_value(&val, FALSE, NULL);
	/* See if gtm_dmterm is set */
	val.addr = GTM_DMTERM;
	val.len = SIZEOF(GTM_DMTERM) - 1;
	dmterm_default = logical_truth_value(&val, FALSE, NULL);
	/* Set values for gtm_utfcgr_strings and gtm_utfcgr_string_groups */
	val.addr = GTM_UTFCGR_STRINGS;
	val.len = SIZEOF(GTM_UTFCGR_STRINGS) - 1;
	TREF(gtm_utfcgr_strings) = trans_numeric(&val, &is_defined, TRUE);
	if (!is_defined)
	{
		assert(GTM_UTFCGR_STRINGS_DEFAULT <= GTM_UTFCGR_STRINGS_MAX);
		TREF(gtm_utfcgr_strings) = GTM_UTFCGR_STRINGS_DEFAULT;
	} else if (GTM_UTFCGR_STRINGS_MAX < TREF(gtm_utfcgr_strings))
		TREF(gtm_utfcgr_strings) = GTM_UTFCGR_STRINGS_MAX;
	val.addr = GTM_UTFCGR_STRING_GROUPS;
	val.len = SIZEOF(GTM_UTFCGR_STRING_GROUPS) - 1;
	TREF(gtm_utfcgr_string_groups) = trans_numeric(&val, &is_defined, TRUE);
	if (!is_defined)
		TREF(gtm_utfcgr_string_groups) = GTM_UTFCGR_STRING_GROUPS_DEFAULT;
	/* If gtm_locale is defined, reset the locale for this process - but only for UTF8 mode */
	if (is_gtm_chset_utf8)
	{
		val.addr = GTM_LOCALE;
		val.len = SIZEOF(GTM_LOCALE) - 1;
		if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, GTM_PATH_MAX, do_sendmsg_on_log2long)))
		{
			if ((0 < trans.len) && (GTM_PATH_MAX > trans.len))
			{	/* Something was specified - need to clear LC_ALL and set LC_CTYPE but need room in buf[]
				 * for string-ending null.
				 */
				putenv("LC_ALL");			/* Clear LC_ALL before LC_CTYPE can take effect */
				buf[trans.len] = '\0';
				status = setenv("LC_CTYPE", buf, TRUE);
				if (0 != status)
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_INVLOCALE, 2, val.len, val.addr, status);
			}
		}
	}
#	ifdef 	USE_LIBAIO
	/* Initialize variable that controls the nr_events parameter to io_setup() for linux AIO. */
	val.addr = GTM_AIO_NR_EVENTS;
	val.len = SIZEOF(GTM_AIO_NR_EVENTS) - 1;
	TREF(gtm_aio_nr_events) = trans_numeric(&val, &is_defined, FALSE);
	if (!is_defined || (TREF(gtm_aio_nr_events) == 0))
		TREF(gtm_aio_nr_events) = GTM_AIO_NR_EVENTS_DEFAULT;
	/* Populate the io_setup() error string. */
	SNPRINTF(io_setup_errstr, ARRAYSIZE(io_setup_errstr), IO_SETUP_FMT, TREF(gtm_aio_nr_events));
#	endif
	/* Check if gtm_statshare is enabled */
	val.addr = GTM_STATSHARE;
	val.len = SIZEOF(GTM_STATSHARE) - 1;
	ret = logical_truth_value(&val, FALSE, &is_defined);
	if (is_defined)
		TREF(statshare_opted_in) = ret;
	/* Pull in specified gtm_statsdir if specified, else default to gtm_tmp or its default. Note we don't validate the directory
	 * here. It need not exist until a database is opened. If gtm_statsdir does not exist, find an appropriate default and
	 * set it so it is always resolvable.
	 */
	val.addr = GTM_STATSDIR;
	val.len = SIZEOF(GTM_STATSDIR) - 1;
	/* Using MAX_FN_LEN below instead of GTM_PATH_MAX because csa->nl->statsdb_fname[] size is MAX_FN_LEN + 1 */
	if ((SS_NORMAL != (status = TRANS_LOG_NAME(&val, &trans, buf, MAX_STATSDIR_LEN, do_sendmsg_on_log2long)))
			|| (0 == trans.len))
	{	/* Either no translation for $gtm_statsdir or the current and/or expanded value of $gtm_statsdir exceeds the
		 * max path length. For either case $gtm_statsdir needs to be (re)set so try to use $gtm_tmp instead - note
		 * from here down we'll (re)set $gtm_statsdir so it ALWAYS has a (valid) value for mu_cre_file() to later use.
		 */
		val.addr = GTM_TMP_ENV;
		val.len = SIZEOF(GTM_TMP_ENV) - 1;
		if ((SS_NORMAL != (status = TRANS_LOG_NAME(&val, &trans, buf, MAX_STATSDIR_LEN, do_sendmsg_on_log2long)))
				|| (0 == trans.len))
		{	/* Nothing for $gtm_tmp - use DEFAULT_GTM_TMP instead */
			trans.addr = DEFAULT_GTM_TMP;
			trans.len = SIZEOF(DEFAULT_GTM_TMP) - 1;
		}
		/* In the setenv() call below, trans.addr always points to a double quoted string so has a NULL terminator */
		c = GTM_STATSDIR;
		c++;				/* Bump past the '$' to get to the actual envvar name needed by setenv */
		status = setenv(c, trans.addr, 1);
		assert(0 == status);
	}
	assert(MAX_STATSDIR_LEN >= trans.len);
}
