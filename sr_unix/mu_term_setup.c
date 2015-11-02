/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

#include "eintr_wrappers.h"
#include "mu_term_setup.h"

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

static struct termios 	term_in;
static struct termios 	term_out;
static struct termios 	term_err;

static boolean_t get_stdin_charc_pass = TRUE;
static boolean_t get_stdout_charc_pass = TRUE;
static boolean_t get_stderr_charc_pass = TRUE;

void mu_get_term_characterstics(void)
{
	if ((get_stdin_charc_pass = isatty(STDIN_FILENO)) && (tcgetattr(STDIN_FILENO, &term_in) == -1))
	{
		get_stdin_charc_pass = FALSE;
		PERROR("tcgetattr :");
		FPRINTF(stderr, "Unable to get terminal characterstics for standard in\n");
	}

	if ((get_stdout_charc_pass = isatty(STDOUT_FILENO)) && (tcgetattr(STDOUT_FILENO, &term_out) == -1))
	{
		get_stdout_charc_pass = FALSE;
		PERROR("tcgetattr :");
		FPRINTF(stderr, "Unable to get terminal characterstics for standard out\n");
	}

	if ((get_stderr_charc_pass = isatty(STDERR_FILENO)) && (tcgetattr(STDERR_FILENO, &term_err) == -1))
	{
		get_stderr_charc_pass = FALSE;
		PERROR("tcgetattr :");
		FPRINTF(stderr, "Unable to get terminal characterstics for standard err\n");
	}
}

void mu_reset_term_characterstics(void)
{
	int tcsetattr_res;
	int save_errno;

	/* Do not use TCSAFLUSH as it drains all buffered (but yet unprocessed) input in the terminal
	 * even if that was for the next command at the shell prompt. TCSANOW seems to do what we want
	 * (which is to reset terminal characteristics right away).
	 */
	Tcsetattr(STDIN_FILENO, TCSANOW, &term_in, tcsetattr_res, save_errno);
	if (get_stdin_charc_pass && (-1 == tcsetattr_res))
	{
		PERROR("tcsetattr :");
		FPRINTF(stderr, "Unable to set terminal characterstics for standard in\n");
	}

	Tcsetattr(STDOUT_FILENO, TCSANOW, &term_out, tcsetattr_res, save_errno);
	if (get_stdout_charc_pass && (-1 == tcsetattr_res))
	{
		PERROR("tcsetattr :");
		FPRINTF(stderr, "Unable to set terminal characterstics for standard out\n");
	}

	Tcsetattr(STDERR_FILENO, TCSANOW, &term_err, tcsetattr_res, save_errno);
	if (get_stderr_charc_pass && (-1 == tcsetattr_res))
	{
		PERROR("tcsetattr :");
		FPRINTF(stderr, "Unable to set terminal characterstics for standard err\n");
	}
}
