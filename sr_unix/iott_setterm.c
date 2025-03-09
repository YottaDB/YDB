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
#include "iosp.h"
#include "iottdef.h"
#include "gtmio.h"
#include "iott_setterm.h"
#include "eintr_wrappers.h"
#include "gtm_isanlp.h"

GBLREF	uint4		process_id;
GBLREF	boolean_t	exit_handler_active;  			//kt added

error_def(ERR_TCSETATTR);

void iott_setterm(io_desc *io_ptr)
{
	d_tt_struct	*tt_ptr;

	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	if (0 != tt_ptr->setterm_done_by)
	{
		assert(process_id == tt_ptr->setterm_done_by);
		return;	/* "setterm" already done */
	}

	iott_compile_state_and_set_tty_and_ydb_echo(io_ptr, 8, 1, handle_set_tty_err_mode_1);

	tt_ptr->setterm_done_by = process_id;
	return;
}


/* These routines are here because it is frightfully important to keep them
   in synch with iott_setterm.  When they get out of line, a r x:0 causes your
   terminal to be unreachable thereafter.
*/

// iott_mterm sets the inter-character timer (t.c_cc[VTIME]) to 0.0 seconds so that a read with a zero timeout (ie.  Read x:0) will not wait.
void iott_mterm(io_desc * io_ptr)
{
	cc_t 		vtime;  //kt added
	d_tt_struct	*tt_ptr;

	tt_ptr = (d_tt_struct *) io_ptr->dev_sp;

	//kt begin addition
#ifdef __MVS__
		vtime = 1;
#else
		vtime = 0;
#endif
	//kt end addition

	iott_compile_state_and_set_tty_and_ydb_echo(io_ptr, vtime, 0, handle_set_tty_err_mode_2);  //kt moved block of code into function

	return;
}


// iott_rterm restores the inter-character timer (t.c_cc[VTIME]) to 0.8 seconds
void iott_rterm(io_desc *io_ptr)
{
	d_tt_struct  	*tt_ptr;
	tt_ptr = (d_tt_struct *) io_ptr->dev_sp;

	iott_compile_state_and_set_tty_and_ydb_echo(io_ptr, 8, 1, handle_set_tty_err_mode_2);  //kt moved block of code into function

	return;
}


//kt begin additions ---------------
void iott_setterm_for_no_canonical(io_desc * io_ptr,  ttio_state * temp_io_state_ptr)
//kt added function.
//Setup terminal with CANONICAL mode OFF.
//  Needed for reading just one character, e.g. via READ *X command
//  Also needed for reading fixed length, e.g. READ #1
//NOTE: This function is different from iott_setterm_for_direct_mode().  That returns a pointer to an existing structure.
//       But this populates a structure passed in.
{
	d_tt_struct*			tt_ptr;
	ttio_state*			io_state_ptr;

	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;

	*temp_io_state_ptr = tt_ptr->io_state;  //start with current io_state
	temp_io_state_ptr->canonical = FALSE;   //if canonical were left on, then TTY IO subsystem wouldn't return character until after NL (or CR) or EOL

	//kt below does same as iott_compile_state_and_set_tty_and_ydb_echo(io_ptr, 8, 1, handle_set_tty_err_mode_1), but uses temp_io_state
	iott_compile_ttio_struct(io_ptr, temp_io_state_ptr, 8, 1);
	iott_set_tty(io_ptr, &(temp_io_state_ptr->ttio_struct), handle_set_tty_err_mode_1);
	iott_set_ydb_echo(io_ptr, temp_io_state_ptr, temp_io_state_ptr);

	tt_ptr->setterm_done_by = 0;  //signal that terminal will need to be setup again before next use.

	return;
}


ttio_state* iott_setterm_for_direct_mode(io_desc* io_ptr)
//kt added function.  Setup terminal for interaction with user at console in direct mode.
//NOTE: This function is different from iott_setterm_for_no_canonical().  That populates a structure passed in.
//         Whereas this function returns a pointer to an existing structure.
{
	d_tt_struct*			tt_ptr;
	ttio_state* 			direct_mode_io_state_ptr;

	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	direct_mode_io_state_ptr = &(tt_ptr->direct_mode_io_state);

	//kt below does same as iott_compile_state_and_set_tty_and_ydb_echo(io_ptr, 8, 1, handle_set_tty_err_mode_1), but uses direct_mode_io_state
	iott_compile_ttio_struct(io_ptr, direct_mode_io_state_ptr, 8, 1);
	iott_set_tty(io_ptr, &(direct_mode_io_state_ptr->ttio_struct), handle_set_tty_err_mode_1);
	iott_set_ydb_echo(io_ptr, direct_mode_io_state_ptr, direct_mode_io_state_ptr);

	tt_ptr->setterm_done_by = 0;  //signal that terminal will need to be setup again before next use.

	return direct_mode_io_state_ptr;
}

void handle_set_tty_err_mode_1(io_desc* io_ptr, int save_errno, int filedes)  //for SET_TTY_CHECK_ERRORS_MODE_1
//kt added
{
	if (gtm_isanlp(filedes) == 0)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_TCSETATTR, 1, filedes, save_errno);
}


void handle_set_tty_err_mode_2(io_desc* io_ptr, int save_errno, int filedes)  //for SET_TTY_CHECK_ERRORS_MODE_2
//kt added
{
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TCSETATTR, 1, filedes, save_errno);
}


void iott_compile_ttio_struct(io_desc * io_ptr, ttio_state* an_io_state_ptr, cc_t vtime, cc_t vmin)
//kt added function
//Purpose: to compile any state variables into ttio_struct, ready to be later passed to TTY IO system
//         NOTE: Because tt_ptr->io_state.term_ctrl settings have to match TTY IO system, it should also
//		 be set up -- but will not be handling here.  See iott_set_ydb_echo()
/*
Input: an_io_state_ptr -- pointer to IO state structure
       vtime  -- sets VTIME parameter (timeout time) if canonical mode
       vmin   -- sets VMIN parameter (minimum chars to read) if canonical mode

    From: https://www.man7.org/linux/man-pages/man3/termios.3.html
    VTIME  Timeout in deciseconds for noncanonical read (TIME).
    VMIN   Minimum number of characters for noncanonical read (MIN). <-- note MIN is not minutes.
    ...
     In noncanonical mode input is available immediately (without the
       user having to type a line-delimiter character), no input
       processing is performed, and line editing is disabled.  The read
       buffer will only accept 4095 chars; this provides the necessary
       space for a newline char if the input mode is switched to
       canonical.  The settings of MIN (c_cc[VMIN]) and TIME
       (c_cc[VTIME]) determine the circumstances in which a read(2)
       completes; there are four distinct cases:

       MIN == 0, TIME == 0 (polling read)
              If data is available, read(2) returns immediately, with
              the lesser of the number of bytes available, or the number
              of bytes requested.  If no data is available, read(2)
              returns 0.

       MIN > 0, TIME == 0 (blocking read)
              read(2) blocks until MIN bytes are available, and returns
              up to the number of bytes requested.

       MIN == 0, TIME > 0 (read with timeout)
              TIME specifies the limit for a timer in tenths of a
              second.  The timer is started when read(2) is called.
              read(2) returns either when at least one byte of data is
              available, or when the timer expires.  If the timer
              expires without any input becoming available, read(2)
              returns 0.  If data is already available at the time of
              the call to read(2), the call behaves as though the data
              was received immediately after the call.

       MIN > 0, TIME > 0 (read with interbyte timeout)
              TIME specifies the limit for a timer in tenths of a
              second.  Once an initial byte of input becomes available,
              the timer is restarted after each further byte is
              received.  read(2) returns when any of the following
              conditions is met:

              •  MIN bytes have been received.
              •  The interbyte timer expires.
              •  The number of bytes requested by read(2) has been
                 received.  (POSIX does not specify this termination
                 condition, and on some other implementations read(2)
                 does not return in this case.)

              Because the timer is started only after the initial byte
              becomes available, at least one byte will be read.  If
              data is already available at the time of the call to
              read(2), the call behaves as though the data was received
              immediately after the call.

----------------------------------------------------------------------------------------------------------------------------------------
  //NOTE: Below is definition of flags. Not all are set here, and will just inherit values from initial state, before device was opened.

  tcflag_t c_iflag;	// input mode flags
  			//	    (Octal) :         Binary
                        // IGNBRK  (0000001): 000000000000000000000001 Ignore break condition.
                        // BRKINT  (0000002): 000000000000000000000010 Signal interrupt on break.
                        // IGNPAR  (0000004): 000000000000000000000100 Ignore characters with parity errors.
                        // PARMRK  (0000010): 000000000000000000001000 Mark parity and framing errors.
                        // INPCK   (0000020): 000000000000000000010000 Enable input parity check.
                        // ISTRIP  (0000040): 000000000000000000100000 Strip 8th bit off characters.
                        // INLCR   (0000100): 000000000000000001000000 Map NL to CR on input.
                        // IGNCR   (0000200): 000000000000000010000000 Ignore CR.
                        // ICRNL   (0000400): 000000000000000100000000 Map CR to NL on input.
                        // IUCLC   (0001000): 000000000000001000000000 Map uppercase characters to lower case on input  (not in POSIX).
                        // IXON    (0002000): 000000000000010000000000 Enable start/stop output control.
                        // IXANY   (0004000): 000000000000100000000000 Enable any character to restart  output.
                        // IXOFF   (0010000): 000000000001000000000000 Enable start/stop input  control.
                        // IMAXBEL (0020000): 000000000010000000000000 Ring bell wh en input queue is full (not in POSIX).
                        // IUTF8   (0040000): 000000000100000000000000 Input is UTF8 (not in POSIX).
The above constants are define in /usr/include/x86_64-linux-gnu/bits/termios-c_iflag.h

  tcflag_t c_oflag;	// output mode flags   //NOTE: doesn't seem to be used in yottadb
  tcflag_t c_cflag;	// control mode flags  //NOTE: doesn't seem to be used in yottadb

  tcflag_t c_lflag;	// local mode flags
  			//	 (Octal):    Binary
                        // ISIG   (0001): 000000000001 Enable signals.
                        // ICANON (0002): 000000000010 Canonical input (erase and kill processing).
                        // XCASE  (0004): 000000000100 (obsolete), Flip upper and lower case (not in POSIX)
                        // ECHO   (0010): 000000001000 Enable echo.
                        // ECHOE  (0020): 000000010000 Echo erase character as error-correcting backspace
                        // ECHOK  (0040): 000000100000 Echo KILL.
                        // ECHONL (0100): 000001000000 Echo NL.
                        // NOFLSH (0200): 000010000000 Disable flush after interrupt or quit.
                        // TOSTOP (0400): 000100000000 Send SIGTTOU for background output.
The above constants are define in /usr/include/x86_64-linux-gnu/bits/termios-c_lflag.h

NOTE:  For TTY IO signals, e.g. ISIG (enable signals), that are never modified by YDB, the state will not
       be stored in separate tt_ptr->io_state variables.  Instead the combined state will be contained in
       tty_ptr->io_state.ttio_struct and tty_ptr->initial_io_state.ttio_struct

NOTE!! --> One of the most confusing elements of IO state is echo status!
	This stems, in part, because for things to work properly BOTH the TTY IO subsystem AND ydb's settings
  	have to be configured correctly to work together.

Here are the various parts to consider:
  -- ECHO mode as a device parameter.  E.g.  USE $P:NOECHO    Stored in io_state.devparam_echo
  -- The bit flag in the ttio_struct c_lflag specifying TTY IO echo status.  E.g. 00000001000
  -- ydb settings for echo.  Previously stored in .term_ctrl, but now stored in io_state.ydb_echo boolean
  These are not redundant, and have to be stored separately.

When CANONICAL mode is active, there is an extra layer of complexity because is affects the behavior of echo.

In the table below are all combinations of 4 relevant variables, giving 16 combination.
  -- "PARAM CANONICAL" is the device parameter [NO]CANONICAL.  e.g. USE $P:CANONICAL
  -- "PARAM ECHO" is the device parameter [NO]ECHO.  E.g.  USE $P:NOECHO
  -- "TTY ECHO" means the bit flag that will be sent to the TTY IO subsystem
  -- "io_state.ydb_echo" means the ydb settings which determine if it provides echo for input.

As user types "hello<return>, what happens in each case?

NOTE: In table below, "H e l l o" is used to show that characters are returned one at a time, not that space is added between.

      PARAM      PARAM                 io_state.
     CANONICAL   ECHO     TTY ECHO     ydb_echo     USER TYPES         TTY OUTPUTS   YDB OUTPUTS   NET OUTPUT       COMMENT
 1     (+)        (+)       (+)         (-)         Hello<enter>         Hello         ""              Hello         OK
 2     (+)        (+)       (+)         (+)         Hello<enter>         Hello        Hello         Hello Hello      BAD
 3     (+)        (+)       (-)         (-)         Hello<enter>          ""           ""                ""          violates PARAM ECHO (+)
 4     (+)        (+)       (-)         (+)         Hello<enter>          ""          Hello            Hello         BAD because output only appears AFTER <enter>
 5     (+)        (-)       (+)         (-)         Hello<enter>         Hello        ""               Hello         violates  PARAM ECHO (-)
 6     (+)        (-)       (+)         (+)         Hello<enter>         Hello        ""               Hello         violates  PARAM ECHO (-)
 7     (+)        (-)       (-)         (-)         Hello<enter>          ""          ""               ""            OK
 8     (+)        (-)       (-)         (+)         Hello<enter>          ""          Hello            Hello         violates PARAM ECHO (-)
 9     (-)        (+)       (+)         (-)         Hello<enter>        H e l l o      ""              Hello         OK
10     (-)        (+)       (+)         (+)         Hello<enter>        H e l l o    H e l l o        HHeelloo       BAD
11     (-)        (+)       (-)         (-)         Hello<enter>           ""          ""              ""            violates PARAM ECHO (+)
12     (-)        (+)       (-)         (+)         Hello<enter>           ""        H e l l o         Hello         OK
13     (-)        (-)       (+)         (-)         Hello<enter>        H e l l o      ""              Hello         volates PARAM ECHO (-)
14     (-)        (-)       (+)         (+)         Hello<enter>        H e l l o    H e l l o        HHeelloo       volates PARAM ECHO (-)
15     (-)        (-)       (-)         (-)         Hello<enter>           ""          ""               ""           OK
16     (-)        (-)       (-)         (+)         Hello<enter>           ""        H e l l o         Hello         violates PARAM ECHO (-)

SUMMARY OF ACCEPTABLE CONFIGURATIONS

      PARAM      PARAM                io_state.
     CANONICAL   ECHO     TTY ECHO     ydb_echo     USER TYPES         TTY OUTPUTS   YDB OUTPUTS   NET OUTPUT       COMMENT
 1     (+)        (+)       (+)         (-)         Hello<enter>         Hello         ""              Hello         OK
 7     (+)        (-)       (-)         (-)         Hello<enter>          ""           ""                ""          OK
 9     (-)        (+)       (+)         (-)         Hello<enter>        H e l l o      ""              Hello         OK
12     (-)        (+)       (-)         (+)         Hello<enter>          ""         H e l l o         Hello         OK
15     (-)        (-)       (-)         (-)         Hello<enter>          ""           ""                ""          OK

The above shows that if the user DOES want output [CANONICAL (-) and PARAM ECHO (+)],  (items 9 & 12) then this can be achieve two different ways:
 9 -- TTY (+) and io_state.ydb_echo (-)   <--- This lets TTY IO take care of output
12 -- TTY (-) and io_state.ydb_echo (+)   <--- This lets YDB take care of output  (preferred)

Will exclude #9, and use #12 instead, and we therefore have this logic:

PARAM CANONICAL --> TTY CANONICAL  <-- direct sync between these two.

     PARAM     PARAM
   CANONICAL   ECHO      -->  TTY ECHO                                 io_state.ydb_echo
 1    (+)       (+)      -->    (+)       FYI, to be set elsewhere --> (-)
 7    (+)       (-)      -->    (-)       FYI, to be set elsewhere --> (-)
12    (-)       (+)      -->    (-)       FYI, to be set elsewhere --> (+)
15    (-)       (-)      -->    (-)       FYI, to be set elsewhere --> (-)

The reason that io_state.ydb_echo is NOT set here is because there is a case where initial_io_state needs
  to be source instead of io_state. So this will be set in a separate function, iott_set_ydb_echo()
  //kt update: after changing this function to accept arbitrary an_io_state_ptr, perhaps all could be done in this one function.  Would need to be researched.

*/
{
	d_tt_struct * 		tt_ptr;
	tt_ptr = (d_tt_struct *) io_ptr->dev_sp;
	struct termios		t;

	//Start with TTY IO state from when first entered ydb.
	t = tt_ptr->initial_io_state.ttio_struct;

	if (an_io_state_ptr->canonical)
	{
		SET_BIT_FLAG_ON(ICANON, t.c_lflag);

		//NOTE: In canonical mode, the TTY system won't end input until newline (NL) found.
		//	To enable termination on CR (enter key), then must enable CR->NL mapping.
		SET_BIT_FLAG_ON(ICRNL, t.c_iflag); 	//turn ON ICRNL  = turn ON  Map CR to NL on input.
		SET_BIT_FLAG_OFF(INLCR, t.c_iflag); 	//turn OFF INLCR = turn OFF Map NL to CR on input.

		//	   PARAM     PARAM
		//	  CANONICAL   ECHO      -->  TTY ECHO                                  io_state.ydb_echo
		//     1    (+)       (+)             (+)         FYI, to be set elsewhere --> (-)
		//     7    (+)       (-)             (-)         FYI, to be set elsewhere --> (-)
		SET_BIT_FLAG_BY_BOOL(an_io_state_ptr->devparam_echo, ECHO, t.c_lflag);
		//note: io_state.ydb_echo also needs to be set.  Will be done in iott_set_ydb_echo()

	} else  //NOT canonical
	{
		//	   PARAM     PARAM
		//	  CANONICAL   ECHO      -->  TTY ECHO                                io_state.ydb_echo
		//    12    (-)       (+)             (-)       FYI, to be set elsewhere --> (+)
		//    15    (-)       (-)             (-)       FYI, to be set elsewhere --> (-)

		SET_BIT_FLAG_OFF(ICANON, t.c_lflag);	//turn OFF ICANON = turn off TTY canonical mode
		SET_BIT_FLAG_OFF(ICRNL,  t.c_iflag);	//turn OFF ICRNL  = turn OFF Map CR to NL on input.
		SET_BIT_FLAG_OFF(INLCR,  t.c_iflag);	//turn OFF INLCR  = turn OFF Map NL to CR on input.

		t.c_cc[VTIME] = vtime;			//this is deciseconds of timeout length
		t.c_cc[VMIN] = vmin;			//this is minimum number of chars to read (min is NOT 'minutes')

		SET_BIT_FLAG_OFF(ECHO, t.c_lflag);	//turn OFF TTY echo
		//note: io_state.ydb_echo also needs to be set.  Will be done in iott_set_ydb_echo()

	}
	SET_BIT_FLAG_BY_BOOL(an_io_state_ptr->hostsync, IXOFF, t.c_iflag);  // start/stop input control
	SET_BIT_FLAG_BY_BOOL(an_io_state_ptr->ttsync,   IXON,  t.c_iflag);  // start/stop output control

	//Save compiled ttio_struct
	//Should contain all state elements from tt_ptr->io_state
	an_io_state_ptr->ttio_struct = t;
}

void iott_set_ydb_echo(io_desc* io_ptr, ttio_state* source_io_state_ptr, ttio_state* output_io_state_ptr)
//kt added
//Purpose: Setup output_io_state_ptr->ydb_echo settings have to match passed *source_io_state_ptr
//
//The reason that io_state.ydb_echo is set separately here is because there is a case where initial_io_state needs
//  to be the source instead of io_state.

//       PARAM     PARAM
//     CANONICAL   ECHO     -->   io_state.ydb_echo
// 1     (+)       (+)      -->   (-)
// 7     (+)       (-)      -->   (-)
// 12    (-)       (+)      -->   (+)
// 15    (-)       (-)      -->   (-)

{
	d_tt_struct *   		tt_ptr;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;

	if (source_io_state_ptr->canonical)
	{
		//configuration #1 & #7 from table above
		output_io_state_ptr->ydb_echo = FALSE;

	} else  //NOT canonical
	{
		//configuration #12 & #15 from table in iott_compile_ttio_struct above
		output_io_state_ptr->ydb_echo = source_io_state_ptr->devparam_echo;
	}
}


tty_getsetattr_status iott_compile_state_and_set_tty_and_ydb_echo(io_desc * io_ptr, cc_t vtime, cc_t vmin, set_tty_err_handler an_err_handler)
//kt added function
//PURPOSE: To create common function for compiling ydb IO state and then setting tty io system.
//Input:  io_ptr -- 		pointer to relevant IO structure.  NOTE: other places in codebase used "iod" for this pointer
//        vtime  -- 		timeout in deciseconds if noncanonical read (VTIME).  E.g. '8' --> 0.8 sec timeout.
//        vmin   -- 		minimum number of characters if noncanonical read (VMIN)
//        err_check_mode -- 	determines how error state is handled.

{
	tty_getsetattr_status	status;
	d_tt_struct *   	tt_ptr;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;

	iott_compile_ttio_struct(io_ptr,  &(tt_ptr->io_state), vtime, vmin);   //compiles IO state into io_state.ttio_struct
	status = iott_set_tty_and_ydb_echo(io_ptr, &(tt_ptr->io_state.ttio_struct), an_err_handler);	//send to underlying TTY IO subsystem
	return status;
}

tty_getsetattr_status iott_set_tty_and_ydb_echo(io_desc* io_ptr, struct termios* ttio_struct_ptr, set_tty_err_handler an_err_handler)
//kt added
{
	tty_getsetattr_status		status;
	d_tt_struct *   		tt_ptr;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;

	status = iott_set_tty(io_ptr, ttio_struct_ptr, an_err_handler);
	iott_set_ydb_echo(io_ptr, &(tt_ptr->io_state), &(tt_ptr->io_state));

	return status;
}

tty_getsetattr_status iott_set_tty(io_desc * io_ptr, struct termios * ttio_struct_ptr, set_tty_err_handler an_err_handler)
//kt added function
//PURPOSE: To create common function for actually setting tty io system.
//Input:  io_ptr -- 		pointer to relevant IO structure.
//        ttio_struct_ptr --	pointer to ttio_struct data structure to send to TTY IO subsystem
//        err_check_mode --	determines how error state is handled.
//NOTE: io_state.ydb_echo needs to match the TTY IO settings, but it is not handled here.  See iott_set_ydb_echo()
{
	tty_getsetattr_status		status;
	int				save_errno;
	int				filedes;
	d_tt_struct *   		tt_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	filedes = tt_ptr->fildes;

	Tcsetattr(filedes, TCSANOW, ttio_struct_ptr, status, save_errno, CHANGE_TERM_TRUE); //send ttio_struct_ptr to TTY IO subsystem
	if ((GETSETATTR_SUCCESS != status) && (an_err_handler != NULL))
	{
		an_err_handler(io_ptr, save_errno, filedes);

	}
	return status;
}
