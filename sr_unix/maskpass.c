/****************************************************************
 *								*
 * Copyright 2009, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#include "gtmxc_types.h"

#include "gtmcrypt_util.h"
#include "ydb_getenv.h"

static struct termios *tty = NULL;

void maskpass_signal_handler(int sig);

int main()
{
	char			passwd[GTM_PASSPHRASE_MAX], hex_out[GTM_PASSPHRASE_MAX * 2], mumps_exe[YDB_PATH_MAX], *env_ptr;
	struct stat		stat_info;
	gtm_string_t		passwd_str;
	struct sigaction	reset_term_handler, ignore_handler;
	int			sig;

	/* Since the obfuscated password depends on $USER and the inode of $ydb_dist/mumps, make sure all the pre-requisites are
	 * available to this process.
	 */
	if (NULL == ydb_getenv(YDBENVINDX_GENERIC_USER, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH))
	{
		printf(ENV_UNDEF_ERROR "\n", USER_ENV);
		exit(EXIT_FAILURE);
	}
	if (NULL == (env_ptr = ydb_getenv(YDBENVINDX_DIST, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH)))
	{
		printf(ENV_UNDEF_ERROR2 "\n", YDB_DIST_ENV, GTM_DIST_ENV);
		exit(EXIT_FAILURE);
	}
	SNPRINTF(mumps_exe, YDB_PATH_MAX, "%s/%s", env_ptr, "mumps");
	if (0 != stat(mumps_exe, &stat_info))
	{
		printf("Cannot stat %s\n", mumps_exe);
		exit(EXIT_FAILURE);
	}
	/* We want the process to restore the terminal settings (if they already changed by the time a signal is caught) on the more
	 * conventional terminal signals, such as SIGINT and SIGTERM, and ignore the non-critical other ones. We also do not want to
	 * allow putting the process in the background because the terminal settings may be unsuitable for user interaction at that
	 * point, and the user may decide to "sanitize" them, which might render the entered password visible upon resumption.
	 */
	reset_term_handler.sa_handler = maskpass_signal_handler;
	reset_term_handler.sa_flags = 0;
	sigfillset(&reset_term_handler.sa_mask);
	ignore_handler.sa_handler = SIG_IGN;
	ignore_handler.sa_flags = 0;
	sigemptyset(&ignore_handler.sa_mask);
	for (sig = 1; sig <= NSIG; sig++)
	{
		switch (sig)
		{
			case SIGINT:
			case SIGTERM:
				sigaction(sig, &reset_term_handler, NULL);
				break;
			case SIGSEGV:
			case SIGABRT:
			case SIGBUS:
			case SIGFPE:
			case SIGTRAP:
			case SIGKILL:
				break;
			default:
				sigaction(sig, &ignore_handler, NULL);
		}
	}
	/* Read the password (with terminal echo turned off). */
	if (-1 == gc_read_passwd(GTMCRYPT_DEFAULT_PASSWD_PROMPT, passwd, GTM_PASSPHRASE_MAX, &tty))
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

void maskpass_signal_handler(int sig)
{	/* If gc_read_passwd() changed the terminal settings before we got hit by an interrupt, the original terminal state should
	 * have been saved in tty, so we will only restore the terminal settings when the pointer is non-NULL.
	 */
	if (NULL != tty)
		tcsetattr(fileno(stdin), TCSAFLUSH, tty);
	exit(-1);
}
