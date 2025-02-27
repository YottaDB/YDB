/****************************************************************
 *								*
 * Copyright (c) 2023-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include <dlfcn.h>

#include "mdef.h"
#include "op.h"
#include "io.h"
#include "iosp.h"
#include "trmdef.h"
#include "iottdef.h"
#include "iottdefsp.h"
#include "iott_edit.h"

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_stdlib.h"
#include "gtmmsg.h"
#include "gtm_malloc.h"
#include "gtmio.h"

#include "eintr_wrappers.h" //CLOSE
#include "ydb_logical_truth_value.h"
#include "ydb_trans_log_name.h"
#include "gtmimagename.h"
#include "stringpool.h"
#include "deferred_events_queue.h"
#include "cli.h"

#include "readline.h"

/*
 * DIRECT MODE READLINE TASKS
 * --------------------------
 * [x]: dlopen
 * [x]: dlsym
 * [x]: Code to only use readline if dlopen succeeds
 * [x]: Seperate code into its own file
 * [x]: Header file
 * [x]: Store in v and execute
 * [x]: Add in-session history
 * [x]: Ctrl-D handling
 * [x]: Readline history env variable for file
 * [x]: Readline history env variable for history length
 * [x]: Readline history load from file
 * [x]: Readline history stifle to max length
 * [x]: Readline history write to file on exit
 * [x]: Readline history de-duplication
 * [x]: Readline print history ("rec"): need to use YDB WRITE calls
 * [x]: Readline event expansion
 * [x]: Change ydb_readline to binary field
 * [x]: Change wordexp to always expand ~ only (actually wordexp removed; now we getenv("HOME"))
 * [x]: Change wordexp to construct file based on caller (dse, lke, yottadb) (actually wordexp removed; now we getenv("HOME"))
 * [x]: Readline dse
 * [x]: Readline lke
 * [x]: Readline mupip
 * [x]: Prompt for non-readline code for dse, lke, mupip (as they were decapitated)
 * [x]: dse/lke/mupip: If line is longer than 32k (MAX_LINE), issue an error
 * [ ]: dse/lke/mupip: Something to print history
 * [x]: out of band handling outside select loop
 * [x]: Handle signals properly
 * [ ]: Set-up default teminators
 * [x]: Select loop out of band handling
 * [ ]: Select call
 * [ ]: Select error handling
 * [ ]: read for select
 * [x]: UTF-8 reading/regular reading
 * [ ]: After read, wrap if exceeding ioptr_width
 * [ ]: Check for teminators
 * [ ]: If termnator (different for UTF-8),
 *      - Maybe discard CRLF
 *      - Issue newlines for next read
 * [ ]: Migrate iott_read also to use readline
 * [ ]: Add READLINE device parameter
 * [x]: Handle backspace
 * [x]: Handle ESC sequences (up down, etc)
 * [x]: Handle CTRL-A,E etc.
 * [x]: Handle Insert/Append
 * [x]: Handle redisplay after insert/append
 * [x]: Update cursor position
 * [x]: Update total string length
 * [x]: Update $X
 * [x]: Move cursor
 * [x]: Update $Y
 * [x]: Reset Terminal for next read
 * [ ]: Debugging output (with yottadb -version): Readline version
 * [ ]: Debugging output (with yottadb -version): Readline history location
 */

GBLREF io_pair 			io_curr_device;
GBLREF spdesc			stringpool;
GBLREF volatile int4		outofband;
GBLREF volatile boolean_t	dollar_zininterrupt;
GBLREF int4			exi_condition;
GBLREF enum			gtmImageTypes image_type;

LITREF gtmImageName		gtmImageNames[];

/* Signal Handling in Direct Mode
 * ------------------------------
 * Since we are using in the current iteration of the codebase the line-based
 * version of readline, there are three ways to handle signals:
 * 1. Readline can handle them, but it then forwards them to YottaDB
 * 2. We can handle them, but we need to do readline events (clean-up etc)
 * 3. Same as #2, but we need to exit readline as well using siglongjmp.
 *
 * The problem with #1 is that CTRL-C did not get handled until you pressed
 * enter, and INTRPT never got handled (readline does not handle SIGUSR1 at
 * all). So basically signals were not real time. This is a non-starter.
 *
 * #2 is closer to how we want things, but dm_read is develishly clever in what
 * it does: after a signal is received, the signal code is executed, then it goes
 * the main loop again, detects an out of band event, runs the code for the event,
 * executes the unwinder, and the unwinder calls dm_read again. #2 by itself will
 * return control to readline after the signal, which is not how dm_read works, as
 * what we want to do after the signal is check the outofband variable and run the
 * appropriate code which will then run the unwinder.
 *
 * In order for us to get functionality like the original YottaDB dm_read, we have
 * to find a way to go outside readline when a signal is received: This is how we
 * end up with using sigsetjmp and siglongjmp. sigsetjmp sets a location where we can
 * handle outofband events, and siglongjmp jumps to the setjmp location after signal
 * processing is done. The "volatile" boolean variable readline_catch_signal is set
 * to activate the longjmp ONLY IF we are inside the readline code.
 *
 * Note the use of readline_signal_count. The problem is that we only have a single
 * sigsetjmp, which records the current stack for siglongjmp. If multiple signals are
 * received, each adds stack levels. The last received signal will execute siglongjmp,
 * but this will result in us losing the processing for the previous signal, which
 * didn't finish processing yet--siglongjmp goes to the stack originally by sigsetjmp.
 * Counting signals in readline_signal_count ensures that we only run siglongjmp if
 * we are the last signal on the stack. See https://gitlab.com/YottaDB/DB/YDB/-/issues/1065.
 *
 * The use of sigsetjmp/siglongjmp is commonly used with readline; e.g. with Postgres
 * psql interpreter, and I modeled this implementation on it.
 *
 * In a future iteration of this code, not yet planned, we will use the alternate
 * callback interface for readline, which will allow us to use a select() call
 * (which is what dm_read does), and this will obviate the need to this fancy
 * jumping.
 *
 * Signal Handling for MUPIP/LKE/DSE
 * ---------------------------------
 * This case is much simpler. The signals used to be handled by readline first using its
 * native handler; this handler does what's needed by readline, and then forwards
 * the signals to YottaDB. SIGUSR1 (mupip interrupt) was not handled.
 *
 * But the above approach did not work (see YDB#1128 for more details) and so we instead
 * choose the same approach for MUPIP/LKE/DSE as we did for Direct Mode. And that is to
 * disable readline signal handling and enable YottaDB signal handling even for utilities.
 */

/* This function checks the env var ydb_readline and then tries to dlopen libreadline.so.
 * If everything goes fine, then we call readline_init. Sets readline_file to a file name
 * as a signal of success.
 */
void readline_check_and_loadlib(void) {
	void			*readline_handle = NULL;
	boolean_t		readline_requested = FALSE;
	boolean_t		setup_succeeded = FALSE;

	/* $ydb_readline=1 */
	readline_requested = ydb_logical_truth_value(YDBENVINDX_READLINE, FALSE, NULL);

	if (readline_requested) {
		readline_handle = dlopen("libreadline.so", RTLD_NOW | RTLD_GLOBAL);
		if (NULL != readline_handle)
			readline_init(readline_handle);
	}

	/* The end caller need to check readline_file to be non-NULL to verify that
	 * this call succeeded */
	return;
}

/* This function
 * - dlsyms all the needed symbols from the readline library
 * - Sets up parameters to interact with the readline library
 * - Loads/creates the readline history file
 * - Sets readline_file as a signal of success
 */
void readline_init(void* handle) {
	boolean_t readline_file_creation_success = FALSE;
	int32_t readline_file_history_read_status;
	int BOL, SYM, EOL; /* beginning, start of symbols, end of list, for formatting */

	BOL				= 1;
	freadline			= dlsym(handle, "readline");
	fusing_history			= dlsym(handle, "using_history");
	fadd_history			= dlsym(handle, "add_history");
	fread_history			= dlsym(handle, "read_history");
	fappend_history			= dlsym(handle, "append_history");
	fhistory_set_pos		= dlsym(handle, "history_set_pos");
	fwhere_history			= dlsym(handle, "where_history");
	fcurrent_history		= dlsym(handle, "current_history");
	fnext_history			= dlsym(handle, "next_history");
	fhistory_get			= dlsym(handle, "history_get");
	frl_bind_key			= dlsym(handle, "rl_bind_key");
	frl_variable_bind		= dlsym(handle, "rl_variable_bind");
	frl_redisplay			= dlsym(handle, "rl_redisplay");
	fhistory_expand			= dlsym(handle, "history_expand");
	fstifle_history			= dlsym(handle, "stifle_history");
	frl_free_line_state		= dlsym(handle, "rl_free_line_state");
	frl_reset_after_signal		= dlsym(handle, "rl_reset_after_signal");
	frl_cleanup_after_signal	= dlsym(handle, "rl_cleanup_after_signal");
	frl_insert			= dlsym(handle, "rl_insert");
	frl_overwrite_mode		= dlsym(handle, "rl_overwrite_mode");
	frl_save_state			= dlsym(handle, "rl_save_state");
	frl_restore_state		= dlsym(handle, "rl_restore_state");
	frl_bind_key_in_map		= dlsym(handle, "rl_bind_key_in_map");
	frl_get_keymap			= dlsym(handle, "rl_get_keymap");
	fhistory_truncate_file		= dlsym(handle, "history_truncate_file");
	SYM				= 1;
	vrl_readline_name		= dlsym(handle, "rl_readline_name");
	vrl_prompt			= dlsym(handle, "rl_prompt");
	vhistory_max_entries		= dlsym(handle, "history_max_entries");
	vhistory_length			= dlsym(handle, "history_length");
	vhistory_base			= dlsym(handle, "history_base");
	vrl_catch_signals		= dlsym(handle, "rl_catch_signals");
	vrl_startup_hook		= dlsym(handle, "rl_startup_hook");
	vrl_already_prompted		= dlsym(handle, "rl_already_prompted");
	vrl_redisplay_function		= dlsym(handle, "rl_redisplay_function");
	EOL				= 1;

	if (!(
				BOL
				&& freadline
				&& fusing_history
				&& fadd_history
				&& fread_history
				&& fappend_history
				&& fhistory_set_pos
				&& fwhere_history
				&& fcurrent_history
				&& fnext_history
				&& fhistory_get
				&& frl_bind_key
				&& frl_variable_bind
				&& frl_redisplay
				&& fhistory_expand
				&& fstifle_history
				&& frl_free_line_state
				&& frl_reset_after_signal
				&& frl_cleanup_after_signal
				&& frl_insert
				&& frl_overwrite_mode
				&& frl_save_state
				&& frl_restore_state
				&& frl_bind_key_in_map
				&& frl_get_keymap
				&& fhistory_truncate_file
				&& SYM
				&& vrl_readline_name
				&& vrl_prompt
				&& vhistory_max_entries
				&& vhistory_length
				&& vhistory_base
				&& vrl_catch_signals
				&& vrl_startup_hook
				&& vrl_already_prompted
				&& vrl_redisplay_function
				&& EOL
				))
		return;

	*vrl_catch_signals = 0; /* disable readline signal handling for Direct Mode and MUPIP/LKE/DSE */

	/* Allow conditional parsing of the ~/.inputrc file. */
	*vrl_readline_name = "YottaDB";
	/* Turn on history keeping */
	fusing_history();
	/* disable bracketed paste so that cursor doesn't jump to beginning of prompt */
	frl_variable_bind("enable-bracketed-paste", "off");
	/* readline_file is the variable used as a signal by the callers that the readline
	 * library set-up has succeeded */
	readline_file = get_readline_file();
	if (NULL != readline_file) {
		readline_file_creation_success = create_readline_file(readline_file);
		if (!readline_file_creation_success) {
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_READLINEFILEPERM, 1, readline_file);
			gtm_free(readline_file);
			readline_file = NULL;
			return;
		}
	} else {
		return;
	}

	/* Next step will be to set the history length. This is set using the ~/.inputrc using `history-size`.
	 * If history length is zero (unlimited), stifle at 1000.
	 */
	if (0 == *vhistory_max_entries) {
		fstifle_history(1000);
	}

	/* Load readline history */
	readline_file_history_read_status = fread_history(readline_file);

	/* This should never happen (we ensured a RW file), thus the assert */
	assert(0 == readline_file_history_read_status);
	UNUSED(readline_file_history_read_status);

	/* Initialize session entry count */
	ydb_rl_entries_count = 0;

	return;
}

/* Writes history; called by various functions in YottaDB at shutdown */
void readline_write_history(void) {
	if (NULL != readline_file) {
		/* Clamp the number of entries to append to the file to the maximum runtime list */
		if (ydb_rl_entries_count > *vhistory_max_entries)
			ydb_rl_entries_count = *vhistory_max_entries;
		/* Append session history to readline file */
		fappend_history(ydb_rl_entries_count, readline_file);
		/* Truncate the file to 1000 entries; we have no setting right now to change this. */
		fhistory_truncate_file(readline_file, 1000);
		gtm_free(readline_file);
		readline_file = NULL;
	}
}

/* This function creates the history file; the file is closed after creation.
 * The readline library will perform opening/closing the file when it's written out.
 */
boolean_t create_readline_file(char * readline_file) {
	int fd;
	int garbage_var;

	fd = OPEN3(readline_file, O_CREAT, S_IRUSR | S_IWUSR);
	if (fd >= 0) {
		CLOSE(fd, garbage_var);
		return TRUE;
	} else {
		return FALSE;
	}
}

/* Used by Direct Mode Only: Read input from user into the stringpool */
void readline_read_mval(mval *v) {
	char *line;
	char line0;
	char *expansion;
	int expansion_result;
	int line_length;
	io_desc 	*io_ptr;
	d_tt_struct 	*tt_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	io_ptr = io_curr_device.in;
	tt_ptr = (d_tt_struct *)(io_ptr->dev_sp);
	boolean_t done = FALSE;
	recall_data recall_data;

	/* finish any pending flush timers (or else we would see a newline) */
	iott_flush(io_curr_device.out);

	do {
		/* Signal Handling destination (siglongjmp from signal handling code)
		 * outofband tested separately because a signal can be received
		 * outside the readline code, we we need to handle that too.
		 * Once we have the readline callback interface, we can take all of
		 * the setjmp/longjmp code out, as we can use a select() call like
		 * dm_read.c. */
		if ((sigsetjmp(readline_signal_jmp, 1) != 0) || outofband) {
			/* Assert that the current number of active signals is
			 * now zero. We only want to run siglongjmp (which
			 * destroys the stack) if we are processing the last
			 * signal on the stack, otherwise, we end up destroying
			 * the stack of the other signals if multiple signals
			 * are being processed.
			 * See https://gitlab.com/YottaDB/DB/YDB/-/issues/1065
			 */
			assert(0 == readline_signal_count);
			/* Handle signals */
			if (outofband) {
				if ((jobinterrupt == outofband)) {
					unsigned char *readline_text_before_interrupt;

					/* SIGUSR1/mupip intrpt: save state if jobinterrupt *
					 * See my message to the maintainer on how to do this. *
					 * https://lists.gnu.org/archive/html/bug-readline/2023-09/msg00002.html *
					 */
					/* Clean line state (cleans undo list--memory issue if we don't do that) */
					frl_free_line_state();

					/* Get the state from Readline and save */
					ydb_readline_state = gtm_malloc(sizeof(struct readline_state));
					frl_save_state(ydb_readline_state);

					/* But extract string from readline as it will be overwritten in _rl_init_line_state
					 * next time we use readline() call:
					 *  741   the_line = rl_line_buffer;
					 *  742   the_line[0] = 0; <--Here
					 */
					readline_text_before_interrupt = system_malloc(ydb_readline_state->buflen + 1);
					strncpy((char *)readline_text_before_interrupt, ydb_readline_state->buffer,
							ydb_readline_state->buflen);
					readline_text_before_interrupt[ydb_readline_state->buflen] = '\0';

					/* Save our string in tt_state_save, so we can replace Readline's overwritten string
					 * with ours in the function hook readline_after_zinterrupt_startup_hook.
					 */
					tt_ptr->tt_state_save.buffer_start = readline_text_before_interrupt;

					/* We get rid of the prompt as we don't need it (it's always (TREF(gtmprompt)).addr).
					 * We are not supposed to touch rl_prompt, but there is no other way to prevent doing
					 * readline from doing another free() on the prompt, which results in an ASAN double free error.
					 */
					system_free(ydb_readline_state->prompt);
					ydb_readline_state->prompt = NULL;
					*vrl_prompt = NULL;

					/* We have an interrupt */
					tt_ptr->mupintr = TRUE;
				}
				if (ctrlc == outofband) {
					/* SIGINT */
					/* Cancel current line */
					/* https://lists.gnu.org/archive/html/bug-readline/2016-04/msg00067.html */
					/* The message references rl_callback_sigcleanup(), which is applicable to the callback
					 * interface; we are using the traditional line interface, so we need to use
					 * rl_reset_after_signal()
					 */
					frl_free_line_state();
					frl_reset_after_signal();
				}
				if ((deferred_signal == outofband) || (ztimeout == outofband)) {
					/* SIGSTOP/mupip stop: Reset terminal to be pre-readline so we can go back to the shell.
					 * Also, make sure we do that for SIGALRM/$ZTIMEOUT too, as that may kick us out to the shell */
					frl_cleanup_after_signal();
				}
				if (sighup == outofband) {
					/* This is just copied from dm_read; don't know what it does exactly */
					exi_condition = -ERR_TERMHANGUP;
				}

				TAREF1(save_xfer_root, outofband).event_state = pending;
				async_action(FALSE);
			}
		}

		if (tt_ptr->mupintr) {
			/* We are coming from an interrupt; restore state to before jobinterrupt */
			int 	done;
			char	*old_string; /* to free after we replace rl_line_buffer */

			/* Somehow interrupts disable echo on the terminal; re-enable.
			 * That's what the funky SETTERM_IF_NEEDED, RESETTERM_IF_NEEDED do.
			 * This sends in an extra line feed with Readline v7 *
			 */
			SETTERM_IF_NEEDED(io_ptr, tt_ptr);
			RESETTERM_IF_NEEDED(io_ptr, EXPECT_SETTERM_DONE_TRUE);

			/* Interrupt happened outside of direct mode; or we overflowed the stack - Call unwinder */
			if (dollar_zininterrupt) {
				if (NULL != ydb_readline_state) {
					/* Don't do a free on ydb_readline_state->buffer, as the memory location will be reused
					 * by readline again.
					 */
					free(ydb_readline_state);
					ydb_readline_state = NULL;
				}
				tt_ptr->mupintr = FALSE;
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZINTDIRECT);
			}

			/* This is supposed to have been saved in the outofband event */
			assert(NULL != ydb_readline_state);
			/* Is the readline done? (i.e. interrupt received after we pressed enter) */
			done = ydb_readline_state->done;
			/* Old string (->buffer will be replaced in readline_after_zinterrupt_startup_hook) */
			old_string = ydb_readline_state->buffer;
			/* Temporarily use a different start-up hook to restore data and not reprompt after interrupt */
			*vrl_startup_hook = readline_after_zinterrupt_startup_hook;
			/* Ditto, don't repaint the screen; use existing text already entered */
			*vrl_redisplay_function = readline_after_zinterrupt_redisplay;
			/* Readline call */
			/* For readline_catch_signal and (TREF(gtmprompt)).addr, see below for an explanation. */
			readline_catch_signal = TRUE;
			line = freadline((TREF(gtmprompt)).addr);
			readline_catch_signal = FALSE;
			/* Now free state */
			gtm_free(ydb_readline_state);
			ydb_readline_state = NULL;
			/* Free old string from before interrupt: Cannot do it before as memory location is re-used by readline;
			 * We also need to ensure it happens AFTER rl_line_buffer gets populated in the above call, as otherwise a CTRL-C here
			 * will cancel all of this while we are in the middle leading to a free after use. If we don't get to free it, there is
			 * no harm; just a memory leak.
			 */
			system_free(old_string);
			/* if ydb_readline_state->done, this means the interrupt was sent AFTER we pressed enter on a line; if
			 * that's the case, readline will return immediately on the next read, and the line is NULL. Continue with
			 * the next read after this, otherwise we will exit the process in the line that says if (NULL == line)...
			 */
			if (done) {
				tt_ptr->mupintr = FALSE;
				continue;
			}
			/* This MUST be the last thing done here as it being TRUE is what prevents us from receiving interrupts
			 * while we are processing interrupts */
			tt_ptr->mupintr = FALSE;
		} else {
			/* Normal dm_read equivalent readline line call */
			*vrl_startup_hook = readline_startup_hook;
			*vrl_redisplay_function = frl_redisplay;
			/* readline_catch_signal is used by various signal handlers to siglongjmp to the
			 * sigsetjmp location above. We earlier turned off all signal handling in
			 * readline, so we have to handle everything ourselves. readline_catch_signal
			 * ensures that we only do the siglongjmp if are inside readline, and not
			 * outside. This implementation mirrors the one in Postgres in
			 * src/bin/psql/input.c.
			 */
			readline_catch_signal = TRUE;
			/* TREF(gtmprompt)).addr is already null terminated
			 * line is malloc'ed by readline, and we free it after copying to stringpool.
			 */
			line = freadline((TREF(gtmprompt)).addr);
			readline_catch_signal = FALSE;
		}

		if (NULL == line) {
			/* CTRL-D pressed */
			op_wteol(1);
			op_zhalt(ERR_IOEOF, FALSE);
		}

		if (readline_is_recall(line, &recall_data)) {
			/* User typed rec/recall */
			assert((recall_data.result == recall_all) || (recall_data.result == recall_one));
			if (recall_data.result == recall_all) {
				/* recall */
				readline_print_history();
				assert(0 == recall_data.recall_number);
				assert(NULL == recall_data.recall_string);
				system_free(line);
				continue;
			} else if (NULL == recall_data.recall_string) {
				/* recall n */
				/* replace line with event !n */
				int len;

				system_free(line);
				len = snprintf(NULL, 0, "!%d", recall_data.recall_number) + 1; // NULL
				line = system_malloc(len);
				snprintf(line, len, "!%d", recall_data.recall_number);
			} else {
				/* recall xxx */
				/* replace line with event !?xxx */
				char *original_line;
				int len;

				assert(0 == recall_data.recall_number);
				/* we need to keep line pointer around until after sprintf since
				 * recall_data.recall_string is a pointer inside of it */
				original_line = line;
				len = snprintf(NULL, 0, "!?%s", recall_data.recall_string) + 1; // NULL
				line = system_malloc(len);
				snprintf(line, len, "!?%s", recall_data.recall_string);
				system_free(original_line);
			}
		}

		/* Readline expansion */
		/* Only expand if ! or ^ is at the beginning of the line, as these are otherwise valid in M code */
		line0 = line[0];
		if (('!' == line0) || ('^' == line0)) {
			expansion_result = fhistory_expand(line, &expansion);
			if (1 == expansion_result) {
				//expansion successful
				DOWRITE(tt_ptr->fildes, expansion, strlen(expansion));
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
				system_free(line);
				line = system_malloc(strlen(expansion) + 1);
				memcpy(line, expansion, strlen(expansion) + 1);
				done = TRUE;
			}
			else if (-1 == expansion_result) {
				//error in expansion
				DOWRITE(tt_ptr->fildes, expansion, strlen(expansion));
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
			}
			else if (2 == expansion_result) {
				//:p added to print the line not execute it
				DOWRITE(tt_ptr->fildes, expansion, strlen(expansion));
				DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
			}
			else {
				//No expansions took place, pass result down to YottaDB as is
				assert(0 == expansion_result);
				done = TRUE;
			}
			system_free(expansion);
		} else {
			done = TRUE;
		}
	} while (!done);

	// Move the line to the stringpool
	assert(NULL != line);
	line_length = STRLEN(line);
	ENSURE_STP_FREE_SPACE(line_length);
	v->str.addr = (char *)stringpool.free;
	memcpy(v->str.addr, line, line_length); // no NULL here as this is M String
	v->mvtype = MV_STR;
	v->str.len = line_length;
	stringpool.free += line_length;

	// Increment $Y and wrap by device length
	io_ptr->dollar.y++;
	if (0 != io_ptr->length)
		io_ptr->dollar.y %= io_ptr->length;

	// Add history
	add_single_history_item(line);

	// Reverse readline malloc
	system_free(line);
}

/* Used by MUPIP/LKE/DSE: Read input from user into *output; *output freed by caller */
void readline_read_charstar(char **output) {
	int output_len;
	char *prompt;

	prompt = gtm_malloc(gtmImageNames[image_type].imageNameLen + 3); // +3 = > , space , \0
	memcpy(prompt, gtmImageNames[image_type].imageName, gtmImageNames[image_type].imageNameLen);
	prompt[gtmImageNames[image_type].imageNameLen] = '>';
	prompt[gtmImageNames[image_type].imageNameLen + 1] = ' ';
	prompt[gtmImageNames[image_type].imageNameLen + 2] = '\0';

	//*output is free'd by the caller
	*vrl_startup_hook = readline_startup_hook;
	*output = freadline(prompt);
	gtm_free(prompt);
	if (NULL == *output) {
		/* CTRL-D pressed */
		return;
	}

	// Add history
	add_single_history_item(*output);
}

boolean_t readline_is_recall(char* input, recall_data *recall_data) {
	const char	delimiter_string[] = " \t";
	char		*recall_item = NULL;
	int		recall_number = 0, match_length = 0;
	boolean_t	match_found = FALSE;

	match_length = (uint4)strcspn((const char *)input, delimiter_string);
	/* only "rec" and "recall" should be accepted */
	if (((strlen(REC) == match_length) || (strlen(RECALL) == match_length))
			&& (0 == strncasecmp((const char *)input, RECALL, match_length)))
	{
		char	*strtokptr;
#ifdef DEBUG
		char	*delim_ptr;
#endif

		match_found = TRUE;
		DEBUG_ONLY(delim_ptr = )
			STRTOK_R((char *)input, delimiter_string, &strtokptr);
		assert(NULL != delim_ptr);
		recall_item = STRTOK_R(NULL, "", &strtokptr);
	}

	if (match_found) {
		if (NULL == recall_item) {
			recall_data->result = recall_all;
			recall_data->recall_number = 0;
			recall_data->recall_string = NULL;
		} else {
			if (!cli_is_dcm(recall_item)) {	/* Not a positive decimal number */
				recall_data->result = recall_one;
				recall_data->recall_number = 0;
				recall_data->recall_string = recall_item;
			} else { /* Positive number */
				recall_data->result = recall_one;
				recall_data->recall_number = ATOI(recall_item);
				recall_data->recall_string = NULL;
			}
		}
	}

	return match_found;
}

void readline_print_history(void) {
	HIST_ENTRY *h;
	int i;
	io_desc 	*io_ptr;
	d_tt_struct 	*tt_ptr;
	char		history_index[MAX_DIGITS_IN_INT4 + 3];
	int32_t		history_index_max_length;

	io_ptr = io_curr_device.in;
	tt_ptr = (d_tt_struct *)(io_ptr->dev_sp);

	history_index_max_length = snprintf(NULL, 0, " %d ", *vhistory_length);
	assert(history_index_max_length <= (MAX_DIGITS_IN_INT4 + 2));

	fhistory_set_pos(0);
	for (h = fcurrent_history(); NULL != h; h = fnext_history()) {
		i = fwhere_history() + *vhistory_base;
		snprintf(history_index, history_index_max_length + 1, " %d ", i); // + 1 for NULL
		DOWRITE(tt_ptr->fildes, history_index, history_index_max_length);
		DOWRITE(tt_ptr->fildes, h->line, strlen(h->line));
		DOWRITE(tt_ptr->fildes, NATIVE_TTEOL, strlen(NATIVE_TTEOL));
	}

	return;
}

/* Return readline history file path; caller must free the return */
char* get_readline_file(void) {
	char * 	     my_readline_file, *home;
	int	     len;

	home = getenv("HOME");
	if (NULL == home) {
		return NULL;
	}

	if (STRLEN(home) > PATH_MAX) {
		/* User is trying to fuzz us with an invalid HOME; ignore it and use the current directory */
		home = ".";
	}

	len = snprintf(NULL, 0, "%s/.ydb_%s_history", home, gtmImageNames[image_type].imageName) + 1; //NULL
	my_readline_file = gtm_malloc(len);
	snprintf(my_readline_file, len, "%s/.ydb_%s_history", home, gtmImageNames[image_type].imageName);
	return my_readline_file;
}

/* Add single history item to readline history but don't duplicate */
void add_single_history_item(char *input) {
	HIST_ENTRY *cur_hist;

	// Don't add empty lines
	if (0 == STRLEN(input))
		return;

	// If the first character is a space, don't add it (mirrors bash behavior)
	if (' ' == input[0])
		return;

	/* get the last item added to the history; if it is the same don't add it to the history again */
	cur_hist = fhistory_get(*vhistory_base + *vhistory_length - 1);
	if (NULL != cur_hist) {
		if (0 != strcmp(cur_hist->line, input)) {
			fadd_history(input);
			ydb_rl_entries_count++;
		}
	} else {
		fadd_history(input);
		ydb_rl_entries_count++;
	}
}

int readline_startup_hook(void) {
	boolean_t insert_mode;
	io_desc 	*io_ptr;
	d_tt_struct 	*tt_ptr;
	Keymap		current_readline_keymap;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	// Disable tab auto complete
	current_readline_keymap = frl_get_keymap();
	frl_bind_key_in_map('\t', frl_insert, current_readline_keymap);

	io_ptr = io_curr_device.in;

	/* LKE does not set this up; so abandon the start-up hook. */
	if (NULL == io_ptr)
		return 0;
	if (NULL == io_ptr->dev_sp)
		return 0;
	assert(tt == io_ptr->type);
	tt_ptr = (d_tt_struct *)(io_ptr->dev_sp);

	/* Reverse the job interrupt start-up hook */
	*vrl_already_prompted = 0;

	/* Insert mode from ydb_principal_editing="[NO]INSERT" */
	insert_mode = !(TT_NOINSERT & tt_ptr->ext_cap);
	if (!insert_mode) {
		/* Activate OVERWRITE mode */
		frl_overwrite_mode(1,0);
	}
	return 0;
}

/* Called after zinterrupt: start-up hook */
int readline_after_zinterrupt_startup_hook(void) {
	io_desc 	*io_ptr;
	d_tt_struct 	*tt_ptr;

	assert(NULL != ydb_readline_state);
	io_ptr = io_curr_device.in;
	tt_ptr = (d_tt_struct *)(io_ptr->dev_sp);

	/* Put saved string from before interrupt (this will be freed normally as if readline malloc'ed it in the first place) */
	ydb_readline_state->buffer = (char *)tt_ptr->tt_state_save.buffer_start;
	/* TODO: I think rl_point, rl_end, are not set correctly, leading us to have various things no work after interrupt.
	 * It's possible that to fix this we have to edit the redisplay function below.
	 */
	frl_restore_state(ydb_readline_state);
	*vrl_already_prompted = 1;
	return 0;
}

/* Called after zinterrupt: redisplay hook */
void readline_after_zinterrupt_redisplay(void) {
	/* Redisplay should do nothing after zinterrupt */
	return;
}
