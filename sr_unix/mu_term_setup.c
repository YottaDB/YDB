/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
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

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_termios.h"
#include "gtm_signal.h"	/* for SIGPROCMASK used inside Tcsetattr */

#include "eintr_wrappers.h"
#include "mu_term_setup.h"

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

GBLREF int	terminal_settings_changed_fd;

static struct termios 	tty_settings;

static boolean_t mu_get_term_invoked = FALSE;
static boolean_t tty_settings_initialized;

void mu_get_term_characterstics(void)
{
	int	fd;

	assert(!mu_get_term_invoked);	/* No need to invoke this more than once per process. If assert fails fix second caller. */
	mu_get_term_invoked = TRUE;
	/* Store terminal characteristics if at least one of stdin/stdout/stderr is a terminal.
	 * Might be needed later in "mu_reset_term_characteristics" for restore (before the process terminates)
	 * in case terminal settings got changed by the runtime logic after this function returns.
	 */
	fd = isatty(STDIN_FILENO) ? STDIN_FILENO
		: isatty(STDOUT_FILENO) ? STDOUT_FILENO
			: isatty(STDERR_FILENO) ? STDERR_FILENO
				: -1;
	if (-1 != fd)
	{
		assert(!terminal_settings_changed_fd);
		if (-1 == tcgetattr(fd, &tty_settings))
		{
			PERROR("tcgetattr :");
			FPRINTF(stderr, "Unable to get terminal characterstics for standard out\n");
		} else
			tty_settings_initialized = TRUE;
	}
}

void mu_reset_term_characterstics(void)
{
	int	fd, save_errno, tcsetattr_res;

	if (!mu_get_term_invoked)
		return;	/* We did not initialize "tty_settings" in this process so dont use it */
	if (0 != terminal_settings_changed_fd)
	{
		assert(tty_settings_initialized);	/* i.e. "tty_settings" is initialized and usable here */
		/* Do not use TCSAFLUSH as it drains all buffered (but yet unprocessed) input in the terminal
		 * even if that was for the next command at the shell prompt. So use TCSADRAIN instead.
		 */
		fd = terminal_settings_changed_fd - 1;	/* Subtract 1 to get fd (Tcsetattr macro would have done a "+ 1") */
		assert((STDIN_FILENO == fd) || (STDOUT_FILENO == fd) || (STDERR_FILENO == fd));
		Tcsetattr(fd, TCSADRAIN, &tty_settings, tcsetattr_res, save_errno, CHANGE_TERM_FALSE);
		if (-1 == tcsetattr_res)
		{
			PERROR("tcsetattr :");
			FPRINTF(stderr, "Unable to set terminal characteristics for standard out\n");
		}
	}
}
