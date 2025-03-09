/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
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

#include "gtm_termios.h"
#include "gtm_signal.h"	/* for SIGPROCMASK used inside Tcsetattr */
#include "gtm_unistd.h"

#include "io.h"
#include "iottdef.h"
#include "gtmio.h"
#include "iott_setterm.h"
#include "eintr_wrappers.h"
#include "gtm_isanlp.h"
#include "svnames.h"
#include "util.h"
#include "op.h"
#include "send_msg.h"

GBLREF	uint4		process_id;
GBLREF	boolean_t	prin_in_dev_failure, prin_out_dev_failure;
GBLREF	boolean_t	exit_handler_active;

error_def(ERR_TCSETATTR);

void  iott_resetterm(io_desc* io_ptr)
/* ----------------------------------------------------------------------------------------------------------------------
//kt ADDED NOTES
PURPOSE: Initially restterm() was called at end of READ commands, which caused problems.  Text here will describe some of those
	 problems and modifications applied to improve performance.

	With original functionality, resetterm() was called after each IO read call, often via macro RESETTERM_IF_NEEDED.
	This could happen multiple times in a single line of code. For example:

	      USE $P:NOECHO READ *X hang 1 READ Y#1 hang 1 READ Z

	  After each of these READ's, before proceeding to the next command, a resetterm would  be called.
	  The ideas seemed to be that each command may need to set the IO state in some special way, so when done,
	  code would reset the TTY IO system.

	  A problem resulting from this system, however, was that this caused an unexpected state **inbetween** IO reads.
	  One would expect that the IO state would be NOECHO during the 1 second of HANG in the example above.
	  But if terminal is reset to initial entry IO state after each READ, then the system would actually be back in
	  ECHO mode during the hang. And if the user typed characters during this 1 second, then they would be shown in
	  terminal output -- put there by the TTY IO subsystem.

	  The codebase has now been modified such that instead of calling resetterm() after READ commands, ydb
	  will call a new function called iott_restoreterm(), which will just restore the current ydb compiled IO state.
	  In the example above, it would restore the NOECHO state, not reset to initial state with accompanying ECHO mode.

	Notice that ydb needs to keep track of three (3) different states.
	  1) The original state from when ydb was first started.  This is needed so that the terminal TTY settings can
	     be reset upon exiting ydb.
	     This will be stored in tt_ptr->initial_io_state.
	  2) The state that has been set by mumps code, e.g. USE $P:NOECHO.  This can be code in a routine or in direct mode.
	     This will be stored in tt_ptr->io_state.
	  3) A state needed for interacting with user in direct mode.  For example, if user in direct mode issues a
	     USE $P:NOECHO at the command prompt, then while this specified state must be stored, but we DO stil want
	     the user to be able to type additional commands and see the output via ECHO being on.
	     This will be stored in tt_ptr->direct_mode_io_state.

	The TTY IO subsystem is controlled by sending a ttio_struct with bitflags specifying the various settings.
	  Ydb will maintain a copy of this struct in the various tt_ptr->[io_state].ttio_struct's.

	Ydb also has settings on how it manages input, which needs to cooperate with the TTY IO settings.  Previously,
	  these ydb settings were stored in tt_ptr->term_ctrl, using a different set of bitflags.  Because multiple sets
	  had to be maintained for each [io_state], and to clarify meaning, these settings have now been moved into
	  discrete boolean variables in tt_ptr->[io_state].  For example, tt_ptr->io_state.devparam_echo.  This has
	  removed the need for TRM_NOECHO etc flags from the codebase.

	One might wonder, if the terminal is reset after every IO command, how will the next IO command be completed
	  in the proper IO state?  The answer is that at the BEGINNING of each IO command, setterm() is called,
	  often via macro SETTERM_IF_NEEDED.  This ensured that ydb's current working IO state (based on USE commands
	  etc) is applied to the TTY IO system before completing the upcoming IO command.

	Using new iott_restoreterm() function instead of resetterm() AFTER commands fixed issues with unexpected state between
	   IO calls, as shown above. But there still is a role for resetterm, esp when leaving ydb.

	When analyzing the codebase to correct above issues, it was initially difficult to determine the purpose of resetterm().
	  Obviously resetterm() was supposed to reset the terminal.  But _when_ should the terminal be reset, and
	  _to_what_state_ should the terminal be returned to?  The IO state when ydb first started (i.e. initial_io_state)?  Or
	  the IO state after $P has been initialized with USE parameters?  Or from the last USE command (i.e. io_state)?
	  After study, it appeared that resetterm() should return the terminal to the IO state when ydb first started,
	  i.e. to initial_io_state.

	resetterm() does not, and should not change the state of ydb's IO status (io_state).  If the user or mumps code
	    has specified NOECHO as a USE parameter, this state should be maintained, regardless of resets to the terminal
	    that might be applied between IO commands.  THEREFORE, this function just ensures that the TTY IO subsystem
	    matches tt_ptr->initial_io.ttio_struct.  It doesn't modify, for example, tt_ptr->io_state.canonical.

	At the end of this resetterm function, tt_ptr->setterm_done_by is set to 0.  This is a signal that ydb needs
	  to set the terminal based on internal state variables the next time it is needed.  This is checked in setterm().

	Notice that after this resetterm(), the TTY IO system and ydb's internal state will be that stored in initial_io_state,
	But with next READ command, io_state will be restored via setterm.

//----------------------------------------------------------------------------------------------------------------------
*/
{
	int		status;
	int		save_errno;
	d_tt_struct	*tt_ptr;

	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	if (process_id != tt_ptr->setterm_done_by)
	{
		return;	/* "resetterm" already done */
	}

	//set TTY IO subsystem to value stored in initial_io_state
	iott_set_tty(io_ptr, &(tt_ptr->initial_io_state.ttio_struct),  handle_reset_tty_err);
	iott_set_ydb_echo(io_ptr, &(tt_ptr->initial_io_state), &(tt_ptr->io_state));

	tt_ptr->setterm_done_by = 0;	//<-- This will trigger terminal to be set up again next time used, via setterm()

	return;
}

void handle_reset_tty_err(io_desc* io_ptr, int save_errno, int filedes)
//kt added
{
	assert(ENOTTY != save_errno);
	// Skip TCSETATTR error for ENOTTY (in case fildes is no longer a terminal)
	if ((ENOTTY != save_errno) && (0 == gtm_isanlp(filedes)))
	{
		ISSUE_NOPRINCIO_BEFORE_RTS_ERROR_IF_APPROPRIATE(io_ptr);	// just like is done in "iott_use.c"
		// If we are already in the exit handler, do not issue an error for this event as this
		// could cause a condition handler overrun (i.e. invoke "ch_overrun()") which would create
		// a core file. Since this error most likely means the terminal has gone away, it is better
		// to terminate the process without resetting a non-existent terminal than creating a core file.
		//
		if (!exit_handler_active)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_TCSETATTR, 1, filedes, save_errno);
	}
}

void  iott_restoreterm(io_desc * io_ptr)
//kt added function
//Purpose: This will send currently compiled IO state to the TTY IO subsystem.
//	   This is different from resetterm(), which will return IO state to same as when ydb first started.
{
	d_tt_struct *   		tt_ptr;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;

	//Send ydb current active state to underlying TTY IO subsystem
	iott_set_tty_and_ydb_echo(io_ptr, &(tt_ptr->io_state.ttio_struct), handle_set_tty_err_mode_1);

	tt_ptr->setterm_done_by = process_id;  //signal that another TTY IO set is not currently needed.
}
