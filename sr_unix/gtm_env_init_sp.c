/****************************************************************
 *								*
 *	Copyright 2004, 2013 Fidelity Information Services, Inc	*
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
#include "gtm_string.h"
#include "gtm_strings.h"
#include "gtm_ctype.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"

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
#endif
#define DEFAULT_MUPIP_TRIGGER_ETRAP 		"IF $ZJOBEXAM()"

/* Only for this function, define MAX_TRANS_NAME_LEN to be equal to GTM_PATH_MAX as some of the environment variables can indicate
 * path to files which is limited by GTM_PATH_MAX.
 */
#ifdef MAX_TRANS_NAME_LEN
#undef MAX_TRANS_NAME_LEN
#endif
#define MAX_TRANS_NAME_LEN			GTM_PATH_MAX

GBLREF	int4			gtm_shmflags;			/* Shared memory flags for shmat() */
GBLREF	uint4			gtm_principal_editing_defaults;	/* ext_cap flags if tt */
GBLREF	boolean_t		is_gtm_chset_utf8;
GBLREF	boolean_t		utf8_patnumeric;
GBLREF	boolean_t		badchar_inhibit;
GBLREF	boolean_t		gtm_quiet_halt;
GBLREF	int			gtm_non_blocked_write_retries;	/* number for retries for non_blocked write to pipe */
GBLREF	char			*gtm_core_file;
GBLREF	char			*gtm_core_putenv;
GBLREF	mval			dollar_etrap;
GBLREF	mval			dollar_ztrap;
ZOS_ONLY(GBLREF	char		*gtm_utf8_locale_object;)
ZOS_ONLY(GBLREF	boolean_t	gtm_tag_utf8_as_ascii;)
GTMTRIG_ONLY(GBLREF	mval	gtm_trigger_etrap;)

#ifdef GTM_TRIGGER
LITDEF mval default_mupip_trigger_etrap = DEFINE_MVAL_LITERAL(MV_STR, 0 , 0 , (SIZEOF(DEFAULT_MUPIP_TRIGGER_ETRAP) - 1),
							      DEFAULT_MUPIP_TRIGGER_ETRAP , 0 , 0 );
#endif

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
static readonly unsigned char init_break[1] = {'B'};

void	gtm_env_init_sp(void)
{	/* Unix only environment initializations */
	mstr		val, trans;
	int4		status, index, len, hrtbt_cntr_delta;
	size_t		cwdlen;
	boolean_t	ret, is_defined;
	char		buf[MAX_TRANS_NAME_LEN], *token, cwd[GTM_PATH_MAX];
	char		*cwdptr, *trigger_etrap;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef HUGETLB_SUPPORTED
	libhugetlbfs_init();
#	endif
#	ifdef __MVS__
	/* For now OS/390 only. Eventually, this will be added to all UNIX platforms along with the
	 * capability to specify the desired directory to put a core file in.
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
	val.addr = GTM_SHMFLAGS;
	val.len = SIZEOF(GTM_SHMFLAGS) - 1;
	gtm_shmflags = (int4)trans_numeric(&val, &is_defined, TRUE);	/* Flags vlaue (0 is undefined or bad) */
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
#	ifdef GTM_TRIGGER
	token = GTM_TRIGGER_ETRAP;
	trigger_etrap = GETENV(++token);	/* Point past the $ in gtm_logicals definition */
	if (trigger_etrap)
	{
		len = STRLEN(trigger_etrap);
		gtm_trigger_etrap.str.len = len;
		gtm_trigger_etrap.str.addr = malloc(len);	/* Allocates special null addr if length is 0 which we can key on */
		if (0 < len)
			memcpy(gtm_trigger_etrap.str.addr, trigger_etrap, len);
		gtm_trigger_etrap.mvtype = MV_STR;
	} else if (IS_MUPIP_IMAGE)
		gtm_trigger_etrap = default_mupip_trigger_etrap;
#	endif
	/* ZLIB library compression level */
	val.addr = GTM_ZLIB_CMP_LEVEL;
	val.len = SIZEOF(GTM_ZLIB_CMP_LEVEL) - 1;
	gtm_zlib_cmp_level = trans_numeric(&val, &is_defined, TRUE);
	if (GTM_CMPLVL_OUT_OF_RANGE(gtm_zlib_cmp_level))
		gtm_zlib_cmp_level = ZLIB_CMPLVL_MIN;	/* no compression in this case */
	gtm_principal_editing_defaults = 0;
	val.addr = GTM_PRINCIPAL_EDITING;
	val.len = SIZEOF(GTM_PRINCIPAL_EDITING) - 1;
	if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
	{
		assert(trans.len < SIZEOF(buf));
		trans.addr[trans.len] = '\0';
		token = strtok(trans.addr, ":");
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
			token = strtok(NULL, ":");
		}
	}
	val.addr = GTM_CHSET_ENV;
	val.len = STR_LIT_LEN(GTM_CHSET_ENV);
	if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long))
		&& STR_LIT_LEN(UTF8_NAME) == trans.len)
	{
		if (!strncasecmp(buf, UTF8_NAME, STR_LIT_LEN(UTF8_NAME)))
		{
			is_gtm_chset_utf8 = TRUE;
#			ifdef __MVS__
			val.addr = GTM_CHSET_LOCALE_ENV;
			val.len = STR_LIT_LEN(GTM_CHSET_LOCALE_ENV);
			if ((SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long))) &&
				(0 < trans.len))
			{	/* full path to 64 bit ASCII UTF-8 locale object */
				gtm_utf8_locale_object = malloc(trans.len + 1);
				strcpy(gtm_utf8_locale_object, buf);
			}
			val.addr = GTM_TAG_UTF8_AS_ASCII;
			val.len = STR_LIT_LEN(GTM_TAG_UTF8_AS_ASCII);
			if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
			{	/* We to tag UTF8 files as ASCII so we can read them, this var disables that */
				if (status = logical_truth_value(&val, FALSE, &is_defined) && is_defined)
					gtm_tag_utf8_as_ascii = FALSE;
			}
#			endif
			/* Initialize $ZPATNUMERIC only if $ZCHSET is "UTF-8" */
			val.addr = GTM_PATNUMERIC_ENV;
			val.len = STR_LIT_LEN(GTM_PATNUMERIC_ENV);
			if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long))
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
		TREF(dbinit_max_hrtbt_delta) = DEFAULT_DBINIT_MAX_HRTBT_DELTA;
	else if ((INDEFINITE_WAIT_ON_EAGAIN != hrtbt_cntr_delta) && (NO_SEMWAIT_ON_EAGAIN != hrtbt_cntr_delta))
		TREF(dbinit_max_hrtbt_delta) = (ROUND_UP2(hrtbt_cntr_delta, 8)) / 8;
	else
		TREF(dbinit_max_hrtbt_delta) = hrtbt_cntr_delta;
	/* Initialize variable that controls the location of GT.M custom errors file (used for anticipatory freeze) */
	val.addr = GTM_CUSTOM_ERRORS;
	val.len = SIZEOF(GTM_CUSTOM_ERRORS) - 1;
	if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
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
	/* Initialize which ever error trap we are using (ignored in the utilities except the update process) */
	val.addr = GTM_ETRAP;
	val.len = SIZEOF(GTM_ETRAP) - 1;
	if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
	{
		if (MAX_SRCLINE >= trans.len)
		{	/* Only set $ETRAP if the length is usable (may be NULL) */
			dollar_etrap.str.addr = malloc(trans.len + 1); /* +1 for '\0'; This memory is never freed */
			memcpy(dollar_etrap.str.addr, trans.addr, trans.len);
			*(dollar_etrap.str.addr + trans.len + 1) = '\0';
			dollar_etrap.str.len = trans.len;
			dollar_etrap.mvtype = MV_STR;
		}
	} else if (0 == dollar_etrap.mvtype)
	{	/* If didn't setup $ETRAP, set default $ZTRAP instead */
		dollar_ztrap.mvtype = MV_STR;
		dollar_ztrap.str.len = SIZEOF(init_break);
		dollar_ztrap.str.addr = (char *)init_break;
	}
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
#	endif
#	ifdef GTMDBGFLAGS_ENABLED
	val.addr = GTMDBGFLAGS;
	val.len = SIZEOF(GTMDBGFLAGS) - 1;
	TREF(gtmdbgflags) = trans_numeric(&val, &is_defined, TRUE);
	val.addr = GTMDBGFLAGS_FREQ;
	val.len = SIZEOF(GTMDBGFLAGS_FREQ) - 1;
	TREF(gtmdbgflags_freq) = trans_numeric(&val, &is_defined, TRUE);
#	endif
}
