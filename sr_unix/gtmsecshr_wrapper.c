/****************************************************************
 *								*
 * Copyright (c) 2008-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/********************************************************************************************
 * W A R N I N G  ---  W A R N I N G  --- W A R N I N G --- W A R N I N G --- W A R N I N G *
 *											    *
 * This routine (gtmsecshr_wrapper) runs as setuid to root to perform "environmental        *
 * cleanup" prior to invoking gtmsecshr proper. Extreme care must be taken to prevent all   *
 * forms of deceptive access,  linking with unauthorized libraries, etc. Same applies to    *
 * anything it calls.		    							    *
 *											    *
 * W A R N I N G  ---  W A R N I N G  --- W A R N I N G --- W A R N I N G --- W A R N I N G *
 ********************************************************************************************/


#include "mdef.h"
#define BYPASS_MEMCPY_OVERRIDE  /* Signals gtm_string.h to not override memcpy(). This causes linking problems when libmumps.a
                                 * is not available.
                                 */
/* We want system malloc, not gtm_malloc (which comes from mdef.h --> mdefsp.h).  Since gtmsecshr_wrapper runs as root,
 * using the system malloc will increase security over using gtm_malloc.  Additionally, by not using gtm_malloc, we
 * are reducing code bloat.
 */
#undef malloc
#undef free
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_syslog.h"
#include "main_pragma.h"
#include "gtm_signal.h"
#ifndef __MVS__
#  include <malloc.h>
#endif
#include <errno.h>

#define WBOX_GBLDEF		/* Causes whitebox global vars to be GBLDEF'd instead of GBLREF'd thus keeping gbldefs.c out
				 * of gtmsecshr_wrapper the executable.
				 */
#include "wbox_test_init.h"
#include "gtm_limits.h"

#define ROOTUID 0
#define ROOTGID 0
#define MAX_ENV_VAR_VAL_LEN 1024
#define MAX_ALLOWABLE_LEN 256
#define ASCIICTLMAX	32  /* space character */
#define ASCIICTLMIN	0   /* NULL character */
#define GTM_TMP		"gtm_tmp"
#define GTM_DIST	"gtm_dist"
#define	SUB_PATH_TO_GTMSECSHRDIR "/gtmsecshrdir"
#define	REL_PATH_TO_CURDIR "."
#define	REL_PATH_TO_GTMSECSHR "./gtmsecshr"
#define	GTMSECSHR_BASENAME "/gtmsecshr"
#define MAX_ENV_NAME_LEN 2048

/* Build up some defines for use with AIX and reading /etc/environment to pick up a default TZ value plus some defines
 * for testing AIX errors in this wrapper with white box test cases.
 */
#ifdef _AIX
#define GTM_TZ				"TZ"
#define TZLOCATOR			"TZ="
#define NEWLINE				0x0a
#define GTM_WHITE_BOX_TEST_CASE_ENABLE	"gtm_white_box_test_case_enable"
#define GTM_WHITE_BOX_TEST_CASE_NUMBER	"gtm_white_box_test_case_number"
#define GTMETCDIRPATH			"/etc"
#define BADGTMETCDIRPATH		"/bogusdirnotetc"
#define GTMENVIRONFILE			"environment"
#define BADGTMENVIRONFILE		"bogusfilenotenvironment"
#endif

typedef	char	**environptr_t;
extern	char	**environ;

/* Since gtmsecshr_wrapper.c is a stand-alone module, we cannot use error_def-style definitions, so use simple macro defines to
 * initialize all error types used along with their respective mnemonics. Note that we need two '%' to ensure the '%' prefix
 * in the syslog.
 */
#define ERR_SECSHRCLEARENVFAILED		\
	"%%GTM-E-SECSHRCLEARENVFAILED, clearenv failed. gtmsecshr will not be started\n"
#define ERR_SECSHRCHDIRFAILED1			\
	"%%GTM-E-SECSHRCHDIRFAILED1, chdir failed on %s, errno %d. gtmsecshr will not be started\n"
#define ERR_SECSHRCHDIRFAILED2			\
	"%%GTM-W-SECSHRCHDIRFAILED2, chdir failed on %s, errno %d. gtmsecshr will be started with GMT timezone\n"
#define ERR_SECSHREXECLFAILED			\
	"%%GTM-E-SECSHREXECLFAILED, execl of %s failed\n"
#define ERR_SECSHRGTMDIST2LONG			\
	"%%GTM-E-SECSHRGTMDIST2LONG, gtm_dist env var too long. gtmsecshr will not be started\n"
#define ERR_SECSHRGTMTMP2LONG			\
	"%%GTM-E-SECSHRGTMTMP2LONG, gtm_tmp env var too long. gtmsecshr will not be started\n"
#define ERR_SECSHRNOGTMDIST			\
	"%%GTM-E-SECSHRNOGTMDIST, gtm_dist env var does not exist. gtmsecshr will not be started\n"
#define ERR_SECSHRNOTOWNEDBYROOT		\
	"%%GTM-E-SECSHRNOTOWNEDBYROOT, %s not owned by root. gtmsecshr will not be started\n"
#define ERR_SECSHRNOTSETUID			\
	"%%GTM-E-SECSHRNOTSETUID, %s not set-uid. gtmsecshr will not be started\n"
#define ERR_SECSHRPERMINCRCT			\
	"%%GTM-E-SECSHRPERMINCRCT, %s permissions incorrect (%04o). gtmsecshr will not be started\n"
#define ERR_SECSHRSETGTMDISTFAILED		\
	"%%GTM-E-SECSHRSETGTMDISTFAILED, setenv for gtm_dist failed. gtmsecshr will not be started\n"
#define ERR_SECSHRSETGTMTMPFAILED		\
	"%%GTM-E-SECSHRSETGTMTMPFAILED, setenv for gtm_tmp failed. gtmsecshr will not be started\n"
#define ERR_SECSHRSETUIDFAILED			\
	"%%GTM-E-SECSHRSETUIDFAILED, setuid failed. gtmsecshr will not be started\n"
#define ERR_SECSHRSTATFAILED			\
	"%%GTM-E-SECSHRSTATFAILED, stat failed on %s, errno %d. gtmsecshr will not be started\n"
#define ERR_SECSHRTZFAIL			\
	"%%GTM-W-SECSHRTZFAIL, %s %d. gtmsecshr will start with TZ set to GMT\n"
#define ERR_SECSHRWRITABLE			\
	"%%GTM-E-SECSHRWRITABLE, %s writable. gtmsecshr will not be started\n"

/*
Make sure these are synced with the above. We need this comment for the InfoHub tools to generate message
information for gtmsecshr_wrapper.c, since it does not have a stand-alone .msg file.

	.FACILITY	GTMSECSHRINIT,251/PREFIX=ERR_

SECSHRCHDIRFAILED1	<chdir failed on !AD, errno !UL. gtmsecshr will not be started>/error/fao=3!/ansi=0
SECSHRCHDIRFAILED2	<chdir failed on !AD, errno !UL. gtmsecshr will be started with GMT timezone>/warning/fao=3!/ansi=0
SECSHRCLEARENVFAILED	<clearenv failed. gtmsecshr will not be started>/error/fao=0!/ansi=0
SECSHREXECLFAILED	<execl of !AD failed>/error/fao=2!/ansi=0
SECSHRGTMDIST2LONG	<gtm_dist env var too long. gtmsecshr will not be started>/error/fao=0!/ansi=0
SECSHRGTMTMP2LONG	<gtm_tmp env var too long. gtmsecshr will not be started>/error/fao=0!/ansi=0
SECSHRNOGTMDIST		<gtm_dist env var does not exist. gtmsecshr will not be started>/error/fao=0!/ansi=0
SECSHRNOTOWNEDBYROOT	<!AD not owned by root. gtmsecshr will not be started>/error/fao=3!/ansi=0
SECSHRNOTSETUID		<!AD not set-uid. gtmsecshr will not be started>/error/fao=2!/ansi=0
SECSHRPERMINCRCT	<!AD permissions incorrect (0!UL). gtmsecshr will not be started>/error/fao=3!/ansi=0
SECSHRSETGTMDISTFAILED	<setenv for gtm_dist failed. gtmsecshr will not be started>/error/fao=0!/ansi=0
SECSHRSETGTMTMPFAILED	<setenv for gtm_tmp failed. gtmsecshr will not be started>/error/fao=0!/ansi=0
SECSHRSETUIDFAILED	<setuid failed. gtmsecshr will not be started>/error/fao=0!/ansi=0
SECSHRSTATFAILED	<stat failed on !AD, errno !UL. gtmsecshr will not be started>/error/fao=3!/ansi=0
SECSHRTZFAIL		<!AD !UL. gtmsecshr will start with TZ set to GMT>/warning/fao=3!/ansi=0
SECSHRWRITABLE		<!AD writable. gtmsecshr will not be started>/error/fao=2!/ansi=0
! the following line stops getmsginfo.m
	.end
*/

void strsanitize(char *src, char *dst);
void strsanitize(char *src, char *dst)
{
	int i, srclen;

	/* The calling function already validates the string length. */
	srclen = strlen(src);
	for (i = 0; (i <= srclen) && (MAX_ENV_VAR_VAL_LEN > i); i++)
	{
		/* Convert all control characters to '*'. */
		if ((ASCIICTLMAX > (int)src[i]) && (ASCIICTLMIN < (int)src[i]))
			dst[i] = '*';
		else
			dst[i] = src[i];
	}
}

int main()
{
	int		ret, status;
	char 		*env_var_ptr;
	struct stat	gtm_secshrdir_stat;
	struct stat	gtm_secshr_stat;
	char 		gtm_dist_val[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_tmp_val[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshrdir_path[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshrdir_path_display[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshr_path[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshr_path_display[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshr_orig_path[MAX_ENV_VAR_VAL_LEN];
	boolean_t	gtm_tmp_exists = FALSE;
	int		rc;
	sigset_t	mask;
#	ifdef _AIX
	FILE		*envfile;
	int		recnum, reclen, save_errno;
	char		*fgets_rc;
	boolean_t	gtm_TZ_found;
	char		gtm_TZ_val[MAX_ENV_VAR_VAL_LEN + 1], *gtm_TZ_val_ptr;
	char		*etcdirpath, *environfile;
#	endif

	/* Reset the signal mask (since the one inherited from the invoking process might have signals such as SIGALRM or SIGTERM
	 * blocked) to let gtmsecshr manage its own signals using sig_init.
	 */
	sigemptyset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);	/* BYPASSOK(sigprocmask) */
	OPENLOG("GTMSECSHRINIT", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_USER);
#	ifdef _AIX
#	ifdef DEBUG
	/* Use some very simplistic processing to obtain values for $gtm_white_box_test_case_enable/number since we are basically
	 * standalone in this routine without the ability to call into other mumps routines. We fetch the value and convert it
	 * numerically as best as possible. For the boolean enable flag, If it's non-zero - it's true else false. No errors raised
	 * here for conversions.
	 */
	env_var_ptr = getenv(GTM_WHITE_BOX_TEST_CASE_ENABLE);
	if (NULL != env_var_ptr)
	{
		gtm_white_box_test_case_enabled = atoi(env_var_ptr);
		if (gtm_white_box_test_case_enabled)
		{
			env_var_ptr = getenv(GTM_WHITE_BOX_TEST_CASE_NUMBER);
			if (NULL != env_var_ptr)
				gtm_white_box_test_case_number = atoi(env_var_ptr);
		}
	}
#	endif /* DEBUG */
	etcdirpath = !(WBTEST_ENABLED(WBTEST_SECSHRWRAP_NOETC)) ? GTMETCDIRPATH : BADGTMETCDIRPATH;
	environfile = !(WBTEST_ENABLED(WBTEST_SECSHRWRAP_NOENVIRON)) ? GTMENVIRONFILE : BADGTMENVIRONFILE;
	/* Note syslog timestamps are handled somewhat differently on AIX. If one undefines the TZ environment variable (such as
	 * our wrapper does to prevent reporting time in the timezone of the process that started gtmsecshr, process time reverts
	 * to GMT. So to prevent that, lookup the default timezone in /etc/environment and use it to prevent whacky time reporting
	 * by gtmsecshr in the operator log. Secure file read method is to switch dir first, then do relative open.
	 */
	gtm_TZ_found = FALSE;
	if (-1 == CHDIR(etcdirpath))	/* Note chdir is changed again below so this is only temporary */
		SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRCHDIRFAILED2, etcdirpath, errno);
	else
	{
		envfile = fopen(environfile, "r");
		if (NULL == envfile)
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRTZFAIL, "Unable to open /etc/environment errno:", errno);
		else
		{	/* /etc/environments is open, locate TZ= record */
			for (recnum = 0; ; recnum++)
			{
				FGETS(gtm_TZ_val, MAX_ENV_VAR_VAL_LEN, envfile, fgets_rc);
				if (NULL == fgets_rc)
					break;
				if (STRLEN(TZLOCATOR) >= (reclen = STRLEN(gtm_TZ_val)))		/* Note assignment */
					continue;
				if (0 == memcmp(TZLOCATOR, gtm_TZ_val, STRLEN(TZLOCATOR)))
				{
					gtm_TZ_found = TRUE;
					break;
				}
			}
			if (!gtm_TZ_found DEBUG_ONLY(|| WBTEST_ENABLED(WBTEST_SECSHRWRAP_NOTZREC_READERR)
						     || WBTEST_ENABLED(WBTEST_SECSHRWRAP_NOTZREC_EOF)))
			{	/* We didn't find the TZ record - report it depending if we just hit EOF or something more
				 * severe.
				 */
				save_errno = errno;
#				if DEBUG
				/* Separately test for some white box conditions before testing the real return codes */
				if (WBTEST_ENABLED(WBTEST_SECSHRWRAP_NOTZREC_READERR))
					SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRTZFAIL, "FGETS() failure reading /etc/environment "
                                               "errno:", 999);
				else if (WBTEST_ENABLED(WBTEST_SECSHRWRAP_NOTZREC_EOF))
					SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRTZFAIL, "File /etc/environment has no default TZ ",
					       0);	/* This puts a spurious 0 out with the message but this condition should
							 * be so rare as to be irrelevant and not worth a separate message.
							 */
				else
#				endif
				if (!feof(envfile))
					SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRTZFAIL, "FGETS() failure reading /etc/environment "
					       "errno:", save_errno);
				else
					/* Have EOF - didn't find TZ */
					SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRTZFAIL, "File /etc/environment has no default TZ ",
					       0);	/* This puts a spurious 0 out with the message but this condition should
							 * be so rare as to be irrelevant and not worth a separate message.
							 */
			} else
			{	/* TZ record acquired - isolate TZ value. Note we don't allocate storage for it like we would if
				 * this were a long(er) running process but this process's sole job is to call the real gtmsecshr
				 * so the stack storage flavor of the buffer is fine.
				 */
				if (NEWLINE == gtm_TZ_val[reclen - 1])
					gtm_TZ_val[--reclen] = '\0';		/* Overwrite nl with null and decr length */
				/* Set pointer to point past the TZ= part of the value pair as we use setenv() to set this value */
				gtm_TZ_val_ptr = gtm_TZ_val + STRLEN(TZLOCATOR);
				/* In case this default TZ is different from the process that started gtmsecshr, go ahead and
				 * establish this version now for any remaining messages that happen before we do a clearenv().
				 * We will re-establish the value again after the clear. Until we establish the default value
				 * here, gtmsecshr wrapper messages will have the timezone of the invoking process.
				 */
				status = setenv(GTM_TZ, gtm_TZ_val_ptr, TRUE);
				if ((0 != status) || WBTEST_ENABLED(WBTEST_SECSHRWRAP_SETENVFAIL1))
				{
					save_errno = errno;
#					ifdef DEBUG
					if (WBTEST_ENABLED(WBTEST_SECSHRWRAP_SETENVFAIL1))
					    save_errno = 999;
#					endif
					SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRTZFAIL, "TZ reset with setenv() failed (1) errno:",
					       save_errno);
				}
			}
			fclose(envfile);
		}
	}
#	endif /* _AIX */
	ret = 0; /* start positive */
	/* get the ones we need */
	if (env_var_ptr = getenv(GTM_DIST))		/* Warning - assignment */
	{
		if (MAX_ALLOWABLE_LEN < (strlen(env_var_ptr) + STR_LIT_LEN(SUB_PATH_TO_GTMSECSHRDIR)
		    + STR_LIT_LEN(GTMSECSHR_BASENAME)))
		{
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRGTMDIST2LONG);
			ret = -1;
		} else
		{
			strcpy(gtm_dist_val, env_var_ptr);
			/* point the path to the real gtmsecshr - for display purposes only */
			strcpy(gtm_secshr_path, env_var_ptr);
			strcat(gtm_secshr_path, SUB_PATH_TO_GTMSECSHRDIR);
			strcat(gtm_secshr_path, GTMSECSHR_BASENAME);
			strsanitize(gtm_secshr_path, gtm_secshr_path_display);
			/* point the path to the real gtmsecshrdir */
			strcpy(gtm_secshrdir_path, env_var_ptr);
			strcat(gtm_secshrdir_path, SUB_PATH_TO_GTMSECSHRDIR);
			strsanitize(gtm_secshrdir_path, gtm_secshrdir_path_display);
		}
	} else
	{
		SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRNOGTMDIST);
		ret = -1;
	}
	if (env_var_ptr = getenv(GTM_TMP))		/* Warning - assignment */
	{
		if (MAX_ALLOWABLE_LEN < strlen(env_var_ptr))
		{
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRGTMTMP2LONG);
			ret = -1;
		} else
		{
			gtm_tmp_exists = TRUE;
			strcpy(gtm_tmp_val, env_var_ptr);
		}
	}
	if (!ret)
	{	/* clear all */
#		if defined(SUNOS) || defined(__CYGWIN__)
		environ = NULL;
          	status = 0;
#		else
		status = clearenv();
#		endif
		if (status)
		{
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRCLEARENVFAILED);
			ret = -1;
		}
		/* add the ones we need */
		status = setenv(GTM_DIST, gtm_dist_val, TRUE);
		if (status)
		{
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRSETGTMDISTFAILED);
			ret = -1;
		}
		if (gtm_tmp_exists)
		{
			status = setenv(GTM_TMP, gtm_tmp_val, TRUE);
			if (status)
			{
				SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRSETGTMTMPFAILED);
				ret = -1;
			}
		}
#		ifdef _AIX
		if (gtm_TZ_found DEBUG_ONLY(|| WBTEST_ENABLED(WBTEST_SECSHRWRAP_SETENVFAIL2)))
		{
#			ifdef DEBUG
			if (gtm_TZ_found)	/* Want to run setenv even if WBOX if TZ was found so we can find log entries */
				status = setenv(GTM_TZ, gtm_TZ_val_ptr, TRUE);
			if (WBTEST_ENABLED(WBTEST_SECSHRWRAP_SETENVFAIL2))
				status = -1;
#			else
			status = setenv(GTM_TZ, gtm_TZ_val_ptr, TRUE);
#			endif
			if (0 != status)
			{
				save_errno = errno;
#				ifdef DEBUG
				if (WBTEST_ENABLED(WBTEST_SECSHRWRAP_SETENVFAIL2))
					save_errno = 999;
#				endif
				SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRTZFAIL, "TZ reset with setenv() failed (2) errno:",
				       save_errno);
			}
		}
#		endif
	}
	if (!ret)
	{	/* go to root */
		if (-1 == CHDIR(gtm_secshrdir_path))
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRCHDIRFAILED1, gtm_secshrdir_path_display, errno);
		else if (-1 == Stat(REL_PATH_TO_CURDIR, &gtm_secshrdir_stat))
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRSTATFAILED, gtm_secshrdir_path_display, errno);
		else if (ROOTUID != gtm_secshrdir_stat.st_uid)
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRNOTOWNEDBYROOT, gtm_secshrdir_path_display);
		else if (gtm_secshrdir_stat.st_mode & 0277)
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRPERMINCRCT, gtm_secshrdir_path_display,
					gtm_secshrdir_stat.st_mode & 0777);
		else if (-1 == Stat(REL_PATH_TO_GTMSECSHR, &gtm_secshr_stat))
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRSTATFAILED, gtm_secshr_path_display, errno);
		else if (ROOTUID != gtm_secshr_stat.st_uid)
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRNOTOWNEDBYROOT, gtm_secshr_path_display);
		else if (gtm_secshr_stat.st_mode & 022)
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRWRITABLE, gtm_secshr_path_display);
		else if (!(gtm_secshr_stat.st_mode & 04000))
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRNOTSETUID, gtm_secshr_path_display);
		else if (-1 == setuid(ROOTUID))
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRSETUIDFAILED);
		else
		{	/* call the real gtmsecshr, but have ps display the original gtmsecshr location */
			strcpy(gtm_secshr_orig_path, gtm_dist_val);
			strcat(gtm_secshr_orig_path, GTMSECSHR_BASENAME);
			ret = execl(REL_PATH_TO_GTMSECSHR, gtm_secshr_orig_path, NULL);
			if (-1 == ret)
				SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHREXECLFAILED, gtm_secshr_path_display);
		}
	}
	CLOSELOG();
	return ret;
}
