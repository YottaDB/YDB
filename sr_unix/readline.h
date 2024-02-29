/****************************************************************
 *								*
 * Copyright (c) 2023-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef READLINE_INCLUDED
#define READLINE_INCLUDED
#include <readline/history.h> /* for HIST_ENTRY */
#include <readline/readline.h> /* for readline_state */

#define	REC			"rec"
#define	RECALL			"recall"

enum recall_result {
	recall_none,
	recall_all,
	recall_one
};

typedef struct {
	enum recall_result result;
	int recall_number;
	char *recall_string;
} recall_data;

void readline_check_and_loadlib(void);
void readline_init(void*);
void readline_read_mval(mval*);
void readline_read_charstar(char **);
void readline_write_history(void);
char* get_readline_file(void);
boolean_t create_readline_file(char *);
boolean_t readline_is_recall(char*, recall_data*);
void readline_print_history(void);
void add_single_history_item(char *input);
int readline_startup_hook(void);
int readline_after_zinterrupt_startup_hook(void);
void readline_after_zinterrupt_redisplay(void);

/* Internal Global variables */
GBLREF char 			*readline_file;
GBLREF volatile boolean_t	readline_catch_signal; /* to go to sigsetjmp location */
#include <setjmp.h>
GBLREF sigjmp_buf		readline_signal_jmp; /* for signal handling */
GBLREF unsigned int		readline_signal_count; /* keeps track of multiple signals to do siglongjmp */
GBLREF struct readline_state    *ydb_readline_state; /* for saving state after an interrupt */
/* Counter for entries in this session for use in saving history */
GBLREF int			ydb_rl_entries_count;
/* Readline library functions = f + function name */
GBLREF char			*(*freadline)(char *);
GBLREF void			(*fusing_history)(void);
GBLREF void			(*fadd_history)(char *);
GBLREF int			(*fread_history)(const char *);
GBLREF int			(*fappend_history)(int, const char *);
GBLREF int 			(*fhistory_set_pos)(int);
GBLREF int 			(*fwhere_history)(void);
GBLREF HIST_ENTRY* 		(*fcurrent_history)(void);
GBLREF HIST_ENTRY* 		(*fnext_history)(void);
GBLREF HIST_ENTRY*              (*fhistory_get)(int);
GBLREF void			(*frl_bind_key)(int, rl_command_func_t *);
GBLREF void			(*frl_variable_bind)(char *, char *);
GBLREF void			(*frl_redisplay)(void);
GBLREF int			(*fhistory_expand)(char *, char **);
GBLREF void			(*fstifle_history)(int);
GBLREF void			(*frl_free_line_state)(void);
GBLREF void			(*frl_reset_after_signal)(void);
GBLREF void			(*frl_cleanup_after_signal)(void);
GBLREF int			(*frl_insert)(int, int);
GBLREF int			(*frl_overwrite_mode)(int, int);
GBLREF int			(*frl_save_state)(struct readline_state*);
GBLREF int			(*frl_restore_state)(struct readline_state*);
GBLREF int			(*frl_bind_key_in_map)(int, rl_command_func_t *, Keymap);
GBLREF Keymap			(*frl_get_keymap)(void);
GBLREF int			(*fhistory_truncate_file)(const char *, int);
/* Readline library variables = v + variable name */
GBLREF char			**vrl_readline_name;
GBLREF char			**vrl_prompt;
GBLREF int			*vhistory_max_entries;
GBLREF int			*vhistory_length;
GBLREF int			*vhistory_base;
GBLREF int                      *vrl_catch_signals;
GBLREF rl_hook_func_t		**vrl_startup_hook;
GBLREF int			*vrl_already_prompted;
GBLREF rl_voidfunc_t 		**vrl_redisplay_function;
#endif
