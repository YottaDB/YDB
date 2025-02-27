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

void  iott_resetterm(io_desc * io_ptr)
/* ----------------------------------------------------------------------------------------------------------------------
//kt ADDED NOTES
PURPOSE: Initially restterm() was called at end of TTY READ's, which caused problems.  Text here will describe some of those
	 problems and modifications applied to improve performance.

	With original functionality, iott_resetterm() was called after each IO read call, often via macro RESETTERM_IF_NEEDED.
	This could happen multiple times in a single line of code. For example:

	      USE $P:NOECHO READ *X hang 1 READ Y#1 hang 1 READ Z

	  After each of these READ's, before proceeding to the next command, a iott_resetterm would  be called.
	  The ideas seemed to be that each command may need to set the IO state in some special way, so when done,
	  it would reset the TTY IO system.
	  A problem arose from this system, however was that this caused an unexpected state **inbetween** IO reads.
	  One would expect that the IO state would be NOECHO during the 1 second of HANG in the example above.
	  But if terminal is reset to initial entry IO state after each READ, then the system would actually be back in
	  ECHO mode during the hang. And if the user typed characters during this 1 second, then they would be shown in
	  terminal output -- put there by the TTY IO subsystem.
	  The codebase has now been modified such that instead of calling iott_resetterm() after READ commands, the TTY IO READ's
	  will call a new function called iott_restoreterm(), which will just restore the current ydb compiled IO state.
	  In the example above, it would restore the NOECHO state, not reset to ECHO mode.

	The TTY IO subsystem is controlled by sending a ttio_struct with bitflags specifying the various settings.
	  Ydb will maintain a copy of this struct in io_state.ttio_struct.
	Ydb also has settings on how it manages input, which needs to cooperate with the TTY IO settings.  Previously,
	  these ydb settings were stored in tt_ptr->term_ctrl, using a different set of bitflags.  However, in order
	  to make the logic of this control more clear, these settings have now been moved into discrete boolean variables
	  in tt_ptr->io_state.  For example, tt_ptr->io_state.devparam_echo.  This has removed all TRM_NOECHO etc flags
	  from the codebase.

	One might wonder, if the terminal is reset after every IO command, how will the next IO command be completed
	  in the proper IO state?  The answer would be that at the BEGINNING of each IO command, setterm() is called,
	  often via macro SETTERM_IF_NEEDED.  This ensured that ydb's current working IO state (based on USE commands
	  etc) is applied to the TTY IO system before completing the upcoming IO command.

	Using iott_restoreterm() instead of iott_resetterm() after commands fixed issues with unexpected state between IO calls,
	  as shown above. But there still is a role for iott_resetterm, esp after a direct mode (DM) command is completed.
	  More on this below.
	When analyzing the codebase to correct above issues, it was initially difficult to determine the purpose of iott_resetterm().
	  Obviously iott_resetterm() was supposed to reset the terminal.  But _when_ should the terminal be reset, and
	  _to_what_state_ should the terminal be returned to?  The IO state when ydb first started?  Or
	  the IO state after $P has been initialized with USE parameters?  Or from the last USE command?
	  After study, it appeared that iott_resetterm() should return the terminal to the IO state when ydb first started.
	  This would ensure that any interaction with the user in direct mode will be under the same conditions as
	  when ydb was launched.

	Another point to emphasize is that iott_resetterm() should NOT change the "state" of ydb's IO status.  If the user
	    has specified NOECHO as a USE parameter, this state should be maintained, regardless of resets to the terminal
	    that might be applied between IO commands.  THEREFORE, this function just ensures that the TTY IO subsystem
	    matches tt_ptr->initial_ttio_struct.  It doesn't modify, for example, tt_ptr->io_state.canonical.
	    And it doesn't modify or reset ydb's representation of state.

	When interacting with the user at the console in direct mode (DM), special consideration needs to be given to the
	  IO state.  For example, if the user enters DM command of "USE $P:NOECHO", then ydb's state should be set
	  to NOECHO.  And yet, there DOES need to be echo when the user enters their next command, e.g. "READ X".
	  To handle this situation, before each direct mode command is read from the user, iott_setterm_for_direct_mode() is called.
	  This maintains the state (in this case "NOECHO") that will need to be reinstated before executing the next command.
	  Currently, it has been determined that the IO state does not need to be changed at the END of a a DM read. Instead,
	  it can just be left in the IO state designed for DM mode.  Whenever READ's are called, they will themselves change
	  to the IO state they need.

	At the end of this iott_resetterm function, tt_ptr->setterm_done_by is set to 0.  This is a signal that ydb needs
	  to set the terminal based on internal state variables the next time it is needed.  This is checked in setterm().

	Notice that after this iott_resetterm(), the TTY IO system and ydb's internal state (held in .io_state) are out of sync.  But
	  it will only be this way during DM interaction, and will be corrected with next READ via setterm.

NOTE: renamed "iod" to "io_ptr" for consistency in codebase. Changes not marked.  Just search for "io_ptr"
        	codebase seems to use names "iod", "ioptr", and "io_ptr" interchangably.
NOTE: renamed "ttptr" to "tt_ptr" for consistency across codebase.  Changes not marked below, just search for "tt_ptr"
NOTE: //kt made extensive changes to this function.  Not all are marked.
----------------------------------------------------------------------------------------------------------------------
*/
{
	int		status;
	int		save_errno;
	struct termios 	t;
	d_tt_struct	*tt_ptr;

	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	if (process_id != tt_ptr->setterm_done_by)
	{
		return;	/* "iott_resetterm" already done */
	}

	//set TTY IO subsystem to value stored in initial_io_state
	iott_set_tty(io_ptr, &(tt_ptr->initial_io_state.ttio_struct),  SET_TTY_CHECK_ERRORS_MODE_4);
	iott_set_ydb_echo(io_ptr, &(tt_ptr->initial_io_state), &(tt_ptr->io_state));

	tt_ptr->setterm_done_by = 0;	//<-- This will trigger terminal to be set up again next time used, via setterm()

	//kt printf("debug point 1 in iott_resetterm() in iott_resetterm.c  Setting Echo value to: %d\n", BIT_FLAG_IS_ON (0010, tt_ptr->initial_io_state.ttio_struct.c_lflag));  // ECHO   (0010): 000000001000 Enable echo.

	return;
}

void  iott_restoreterm(io_desc * io_ptr)
//kt added function
//Purpose: This will send currently compiled IO state to the TTY IO subsystem.
//	   This is different from iott_resetterm(), which will return IO state to same as when ydb first started.
{
	d_tt_struct *   		tt_ptr;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;

	//Send ydb current active state to underlying TTY IO subsystem
	iott_set_tty_and_ydb_echo(io_ptr, &(tt_ptr->io_state.ttio_struct), SET_TTY_CHECK_ERRORS_MODE_1);

	tt_ptr->setterm_done_by = process_id;  //signal that another TTY IO set is not currently needed.
}
