/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_termios.h"
#include "gtm_signal.h"	/* for SIGPROCMASK used inside Tcsetattr */

#include "eintr_wrappers.h"
#include "mu_term_setup.h"

#define STDOUT_FILENO	1
#define STDERR_FILENO	2

static struct termios 	tty_settings;

static boolean_t mu_get_term_invoked = FALSE;
static boolean_t get_stdout_charc_pass = TRUE;
static boolean_t get_stderr_charc_pass = TRUE;

void mu_get_term_characterstics(void)
{
	assert(!mu_get_term_invoked);	/* No need to invoke this more than once per process. If assert fails fix second caller. */
	mu_get_term_invoked = TRUE;
	if (get_stdout_charc_pass = isatty(STDOUT_FILENO))
	{
		if (-1 == tcgetattr(STDOUT_FILENO, &tty_settings))
		{
			get_stdout_charc_pass = FALSE;
			PERROR("tcgetattr :");
			FPRINTF(stderr, "Unable to get terminal characterstics for standard out\n");
		}
	} else if (get_stderr_charc_pass = isatty(STDERR_FILENO))
	{
		if (-1 == tcgetattr(STDERR_FILENO, &tty_settings))
		{
			get_stderr_charc_pass = FALSE;
			PERROR("tcgetattr :");
			FPRINTF(stderr, "Unable to get terminal characterstics for standard err\n");
		}
	}
}

void mu_reset_term_characterstics(void)
{
	int tcsetattr_res;
	int save_errno;

	if (!mu_get_term_invoked)
		return;	/* We did not initialize "tty_settings" in this process so dont use it */
	/* Do not use TCSAFLUSH as it drains all buffered (but yet unprocessed) input in the terminal
	 * even if that was for the next command at the shell prompt. So use TCSADRAIN instead.
	 */
	if (get_stdout_charc_pass)
	{
		Tcsetattr(STDOUT_FILENO, TCSADRAIN, &tty_settings, tcsetattr_res, save_errno);
		if (-1 == tcsetattr_res)
		{
			PERROR("tcsetattr :");
			FPRINTF(stderr, "Unable to set terminal characterstics for standard out\n");
		}
	} else if (get_stderr_charc_pass)
	{
		Tcsetattr(STDERR_FILENO, TCSADRAIN, &tty_settings, tcsetattr_res, save_errno);
		if (-1 == tcsetattr_res)
		{
			PERROR("tcsetattr :");
			FPRINTF(stderr, "Unable to set terminal characterstics for standard err\n");
		}
	}
}
