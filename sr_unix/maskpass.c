/****************************************************************
 *								*
 *	Copyright 2009, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "main_pragma.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gtmxc_types.h"

#include "gtmcrypt_util.h"

int main()
{
	char		passwd[GTM_PASSPHRASE_MAX], hex_out[GTM_PASSPHRASE_MAX * 2], mumps_exe[GTM_PATH_MAX], *env_ptr;
	struct stat	stat_info;
	gtm_string_t	passwd_str;

	/* Since the obfuscated password depends on $USER and the inode of $gtm_dist/mumps, make sure all the pre-requisites are
	 * available to this process.
	 */
	if (NULL == (env_ptr = (char *)getenv(USER_ENV)))
	{
		printf(ENV_UNDEF_ERROR "\n", USER_ENV);
		exit(EXIT_FAILURE);
	}
	if (NULL == (env_ptr = (char *)getenv(GTM_DIST_ENV)))
	{
		printf(ENV_UNDEF_ERROR "\n", GTM_DIST_ENV);
		exit(EXIT_FAILURE);
	}
	SNPRINTF(mumps_exe, GTM_PATH_MAX, "%s/%s", env_ptr, "mumps");
	if (0 != stat(mumps_exe, &stat_info))
	{
		printf("Cannot stat %s\n", mumps_exe);
		exit(EXIT_FAILURE);
	}
	/* Read the password (with terminal echo turned off). */
	if (-1 == gc_read_passwd(GTMCRYPT_DEFAULT_PASSWD_PROMPT, passwd, GTM_PASSPHRASE_MAX))
	{
		printf("%s\n", gtmcrypt_err_string);
		exit(EXIT_FAILURE);
	}
	/* Obfuscate the password. */
	passwd_str.address = &passwd[0];
	passwd_str.length = (int)STRLEN(passwd);
	if (-1 == gc_mask_unmask_passwd(2, &passwd_str, &passwd_str))
	{
		printf("%s\n", gtmcrypt_err_string);
		exit(EXIT_FAILURE);
	}
	/* Convert obfuscated password to a hex representation for easy viewing. */
	GC_HEX(passwd, hex_out, passwd_str.length * 2);
	printf("%s\n", hex_out);
	return 0;
}
