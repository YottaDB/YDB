/****************************************************************
 *								*
 *	Copyright 2008, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

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
#ifndef __MVS__
#  include <malloc.h>
#endif
#include <signal.h>
#include <errno.h>
#define ROOTUID 0
#define ROOTGID 0
#define MAX_ENV_VAR_VAL_LEN 1024
#define MAX_ALLOWABLE_LEN 256
#define ASCIICTLMAX	32  /* space character */
#define ASCIICTLMIN	0   /* NULL character */
#define OVERWRITE 1
#define GTM_TMP "gtm_tmp"
#define GTM_DIST "gtm_dist"
#define GTM_DBGLVL "gtmdbglvl"
#define	SUB_PATH_TO_GTMSECSHRDIR "/gtmsecshrdir"
#define	REL_PATH_TO_CURDIR "."
#define	REL_PATH_TO_GTMSECSHR "./gtmsecshr"
#define	GTMSECSHR_BASENAME "/gtmsecshr"
#define MAX_ENV_NAME_LEN 2048

#ifdef __osf__
	/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
	 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
	 * the only exception and, in particular because the operating system does not support such an exception, the argv
	 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
	 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
	 */
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

typedef	char	**environptr_t;
extern	char	**environ;

#ifdef __osf__
#pragma pointer_size (restore)
#endif

/* Since gtmsecshr_wrapper.c is a stand-alone module, we cannot use error_def-style definitions, so use simple macro defines to
 * initialize all error types used along with their respective mnemonics. Note that we need two '%' to ensure the '%' prefix
 * in the syslog.
 */
#define ERR_SECSHRCHDIRFAILED			\
	"%%GTM-E-SECSHRCHDIRFAILED, chdir failed on %s, errno %d. gtmsecshr will not be started\n"
#define ERR_SECSHRCLEARENVFAILED		\
	"%%GTM-E-SECSHRCLEARENVFAILED, clearenv failed. gtmsecshr will not be started\n"
#define ERR_SECSHREXECLFAILED			\
	"%%GTM-E-SECSHREXECLFAILED, execl of %s failed\n"
#define ERR_SECSHRGTMDBGLVL2LONG		\
	"%%GTM-E-SECSHRGTMDBGLVL2LONG, gtmdbglvl env var too long. gtmsecshr will not be started\n"
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
#define ERR_SECSHRWRITABLE			\
	"%%GTM-E-SECSHRWRITABLE, %s writable. gtmsecshr will not be started\n"

int gtm_setenv(char * env_var_name, char * env_var_val, int overwrite);
int gtm_setenv(char * env_var_name, char * env_var_val, int overwrite)
{	/* The overwrite parameter is not used. In our case we always want to set the value */
	char	*env_var_ptr;
	int 	len;

	len = STRLEN(env_var_name) + STRLEN("=") + STRLEN(env_var_val) + 1;
	env_var_ptr = (char *)malloc(len);
	if (NULL == env_var_ptr)
		return -1;
	strcpy(env_var_ptr, env_var_name);
	strcat(env_var_ptr, "=");
	strcat(env_var_ptr, env_var_val);
	if (putenv(env_var_ptr))
		return -1;
	return 0;
}

int gtm_unsetenv(char * env_var_name);
int gtm_unsetenv(char * env_var_name)
{
	return gtm_setenv(env_var_name, "", OVERWRITE);
}

int gtm_clearenv(void);
int gtm_clearenv()
{
        char		env_var_name[MAX_ENV_NAME_LEN];
	environptr_t	p;
        char		*eq;
        int		len;

        if (NULL == environ)
                return 0;
        for (p = environ; *p; p++)
        {
                eq = strchr(*p, '=');
                if (NULL != eq)
                {
			len = (int)(eq - *p);
			if (MAX_ENV_NAME_LEN > len)
			{
                        	memcpy(env_var_name, *p, len);
                        	env_var_name[len] = '\0';
                        	if (gtm_unsetenv(env_var_name))
					return -1;
			} else
				return -1;
                } else
                        return -1;
        }
        return 0;
}

void strsanitize(char *src, char *dst);
void strsanitize(char *src, char *dst)
{
	int i, srclen;

	/* The calling function already validate the string length. */
	srclen = strlen(src);
	for (i = 0; i <= srclen && i < MAX_ENV_VAR_VAL_LEN; i++)
	{
		/* Convert all control characters to '*'. */
		if (ASCIICTLMAX > (int)src[i] && ASCIICTLMIN < (int)src[i])
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
	char 		gtm_dbglvl_val[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshrdir_path[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshrdir_path_display[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshr_path[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshr_path_display[MAX_ENV_VAR_VAL_LEN];
	char 		gtm_secshr_orig_path[MAX_ENV_VAR_VAL_LEN];
	int		gtm_tmp_exists = 0;
	int		gtm_dbglvl_exists = 0;
	sigset_t	mask;

	/* Reset the signal mask (since the one inherited from the invoking process might have signals such as SIGALRM or SIGTERM
	 * blocked) to let gtmsecshr manage its own signals using sig_init.
	 */
	sigemptyset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);
	OPENLOG("GTMSECSHRINIT", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_USER);
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
			gtm_tmp_exists = 1;
			strcpy(gtm_tmp_val, env_var_ptr);
		}
	}
	if (env_var_ptr = getenv(GTM_DBGLVL))		/* Warning - assignment */
	{
		if (MAX_ALLOWABLE_LEN < strlen(env_var_ptr))
		{
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRGTMDBGLVL2LONG);
			ret = -1;
		} else
		{
			gtm_dbglvl_exists = 1;
			strcpy(gtm_dbglvl_val, env_var_ptr);
		}
	}
	if (!ret)
	{	/* clear all */
		status = gtm_clearenv();
		if (status)
		{
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRCLEARENVFAILED);
			ret = -1;
		}
		/* add the ones we need */
		status = gtm_setenv(GTM_DIST, gtm_dist_val, OVERWRITE);
		if (status)
		{
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRSETGTMDISTFAILED);
			ret = -1;
		}
		if (gtm_tmp_exists)
		{
			status = gtm_setenv(GTM_TMP, gtm_tmp_val, OVERWRITE);
			if (status)
			{
				SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRSETGTMTMPFAILED);
				ret = -1;
			}
		}
	}
	if (!ret)
	{	/* go to root */
		if (-1 == CHDIR(gtm_secshrdir_path))
			SYSLOG(LOG_USER | LOG_INFO, ERR_SECSHRCHDIRFAILED, gtm_secshrdir_path_display, errno);
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
