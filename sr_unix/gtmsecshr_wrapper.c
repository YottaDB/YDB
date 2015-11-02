/****************************************************************
 *								*
 *	Copyright 2008, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
/* We want system malloc, not gtm_malloc (which comes from mdef.h --> mdefsp.h).  Since gtmsecshr_wrapper runs as root,
 * using the system malloc will increase security over using gtm_malloc.  Additionally, by not using gtm_malloc, we
 * are reducing code bloat.
 */
#undef malloc
#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_syslog.h"
#include "main_pragma.h"
#ifndef __MVS__
#include <malloc.h>
#endif

#define ROOTUID 0
#define ROOTGID 0

#define MAX_ENV_VAR_VAL_LEN 1024
#define MAX_ALLOWABLE_LEN 256

#define OVERWRITE 1

#define GTM_LOG "gtm_log"
#define GTM_TMP "gtm_tmp"
#define GTM_DIST "gtm_dist"
#define GTM_DBGLVL "gtmdbglvl"
#ifdef __MVS__
#define GTM_ZOS_AUTOCVT "_BPXK_AUTOCVT"
#define GTM_ZOS_AUTOCVT_ON "ON"
#endif

#define SUB_PATH_TO_GTMSECSHR "/gtmsecshrdir/gtmsecshr"
#define	GTMSECSHR_BASENAME "/gtmsecshr"

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

int gtm_setenv(char * env_var_name, char * env_var_val, int overwrite);

int gtm_setenv(char * env_var_name, char * env_var_val, int overwrite)
{
/*
	The overwrite parameter is not used. In our case we always want to set the value.
*/
	char *env_var_ptr;
	int len;

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
	return gtm_setenv(env_var_name,"",OVERWRITE);
}

#define MAX_ENV_NAME_LEN 2048

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

int main()
{
	int ret, status;
	char * env_var_ptr;

	char gtm_dist_val[MAX_ENV_VAR_VAL_LEN];
	char gtm_log_val[MAX_ENV_VAR_VAL_LEN];
	char gtm_tmp_val[MAX_ENV_VAR_VAL_LEN];
	char gtm_dbglvl_val[MAX_ENV_VAR_VAL_LEN];
	char gtm_secshr_path[MAX_ENV_VAR_VAL_LEN];
	char gtm_secshr_orig_path[MAX_ENV_VAR_VAL_LEN];

	int gtm_log_exists = 0;
	int gtm_tmp_exists = 0;
	int gtm_dbglvl_exists = 0;

	openlog("GTMSECSHRINIT", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_USER);

	ret = 0; /* start positive */

	/* get the ones we need */
	if (env_var_ptr = getenv(GTM_DIST))
	{
		if ((MAX_ALLOWABLE_LEN < strlen(env_var_ptr) + strlen(SUB_PATH_TO_GTMSECSHR)))
		{
			syslog(LOG_USER | LOG_INFO, "gtm_dist env var too long. gtmsecshr will not be started.\n");
			ret = -1;
		} else
		{
			strcpy(gtm_dist_val,env_var_ptr);
			/* point the path to the real gtmsecshr */
			strcpy(gtm_secshr_path, env_var_ptr);
			strcat(gtm_secshr_path, SUB_PATH_TO_GTMSECSHR);
		}
	} else
	{
		syslog(LOG_USER | LOG_INFO, "gtm_dist env var does not exist. gtmsecshr will not be started.\n");
		ret = -1;
	}

	if (env_var_ptr = getenv(GTM_LOG))
	{
		if (MAX_ALLOWABLE_LEN < strlen(env_var_ptr))
		{
			syslog(LOG_USER | LOG_INFO, "gtm_log env var too long. gtmsecshr will not be started.\n");
			ret = -1;
		} else
		{
			gtm_log_exists = 1;
			strcpy(gtm_log_val, env_var_ptr);
		}

	}

	if (env_var_ptr = getenv(GTM_TMP))
	{
		if (MAX_ALLOWABLE_LEN < strlen(env_var_ptr))
		{
			syslog(LOG_USER | LOG_INFO, "gtm_tmp env var too long. gtmsecshr will not be started.\n");
			ret = -1;
		} else
		{
			gtm_tmp_exists = 1;
			strcpy(gtm_tmp_val, env_var_ptr);
		}
	}

	if (env_var_ptr = getenv(GTM_DBGLVL))
	{
		if (MAX_ALLOWABLE_LEN < strlen(env_var_ptr))
		{
			syslog(LOG_USER | LOG_INFO, "gtmdbglvl env var too long. gtmsecshr will not be started.\n");
			ret = -1;
		} else
		{
			gtm_dbglvl_exists = 1;
			strcpy(gtm_dbglvl_val, env_var_ptr);
		}
	}

#ifdef __MVS__
	if (!(env_var_ptr = getenv(GTM_ZOS_AUTOCVT)))
	{
		syslog(LOG_USER | LOG_INFO, "_BPXK_AUTOCVT is not set, forcing autoconversion\n");
	}
#endif

	if (!ret)
	{	/* clear all */
		status = gtm_clearenv();
		if (status)
		{
			syslog(LOG_USER | LOG_INFO, "clearenv failed. gtmsecshr will not be started.\n");
			ret = -1;
		}

		/* add the ones we need */
		status = gtm_setenv(GTM_DIST, gtm_dist_val, OVERWRITE);
		if (status)
		{
			syslog(LOG_USER | LOG_INFO, "setenv for gtm_dist failed. gtmsecshr will not be started.\n");
			ret = -1;
		}
		if (gtm_log_exists)
		{
			status = gtm_setenv(GTM_LOG, gtm_log_val, OVERWRITE);
			if (status)
			{
				syslog(LOG_USER | LOG_INFO, "setenv for gtm_log failed. gtmsecshr will not be started.\n");
				ret = -1;
			}
		}
		if (gtm_tmp_exists)
		{
			status = gtm_setenv(GTM_TMP, gtm_tmp_val, OVERWRITE);
			if (status)
			{
				syslog(LOG_USER | LOG_INFO, "setenv for gtm_tmp failed. gtmsecshr will not be started.\n");
				ret = -1;
			}
		}
		if (gtm_dbglvl_exists)
		{
			status = gtm_setenv(GTM_DBGLVL, gtm_dbglvl_val, OVERWRITE);
			if (status)
			{
				syslog(LOG_USER | LOG_INFO, "setenv for gtmdbglvl failed. gtmsecshr will not be started.\n");
				ret = -1;
			}
		}
#ifdef __MVS__
		status = gtm_setenv(GTM_ZOS_AUTOCVT, GTM_ZOS_AUTOCVT_ON, OVERWRITE);
		if (status)
		{
			syslog(LOG_USER | LOG_INFO,
				"setenv for _BPXK_AUTOCVT failed. gtmsecshr logs may contain mixed ASCII and EBCDIC.\n");
		}
#endif
	}

	if (!ret)
	{	/* go to root */
		if (-1 == setuid(ROOTUID))
			syslog(LOG_USER | LOG_INFO, "setuid failed. gtmsecshr will not be started.\n");
		else
		{	/* call the real gtmsecshr, but have ps display the original gtmsecshr location */
			strcpy(gtm_secshr_orig_path, gtm_dist_val);
			strcat(gtm_secshr_orig_path, GTMSECSHR_BASENAME);
			ret = execl(gtm_secshr_path, gtm_secshr_orig_path, NULL);
			if (-1 == ret)
				syslog(LOG_USER | LOG_INFO, "execl of %s failed\n", gtm_secshr_path);
		}
	}

	closelog();

	return ret;
}
