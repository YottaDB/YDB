/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_string.h"
#include "cli.h"
#include <rtnhdr.h>
#include "mu_gv_stack_init.h"
#include "trigger_trgfile_protos.h"
#include "mu_trig_trgfile.h"
#include "mupip_exit.h"

GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
GBLREF	IN_PARMS		*cli_lex_in_ptr;
GBLREF	boolean_t		is_replicator;
GBLREF	int			tprestart_state;

void mu_trig_trgfile(char *trigger_filename, int trigger_filename_len, boolean_t noprompt)
{
	IN_PARMS		*cli_lex_in_ptr_save = NULL;
	boolean_t		trigger_error;

	error_def(ERR_MUNOACTION);

	is_replicator = TRUE;
	/* Since ^#t global is the ONLY global that is touched by MUPIP TRIGGERS and we currently dont support triggers
	 * on ^#t global, it is safe to set skip_dbtriggers to TRUE for this process. This is in fact needed so we
	 * skip cdb_sc_triggermod processing in t_end and tp_tend as otherwise that could cause unnecessary restarts in
	 * journal recovery and update process which at least the former is not designed to handle.
	 */
	skip_dbtriggers = TRUE;
	mu_gv_stack_init();
	if (NULL != cli_lex_in_ptr)
	{
		cli_lex_in_ptr_save = (IN_PARMS *)malloc(SIZEOF(IN_PARMS) + cli_lex_in_ptr->buflen);
		cli_lex_in_ptr_save->argc = cli_lex_in_ptr->argc;
		cli_lex_in_ptr_save->argv = cli_lex_in_ptr->argv;
		cli_lex_in_ptr_save->tp = cli_lex_in_ptr_save->in_str + (cli_lex_in_ptr->tp - cli_lex_in_ptr->in_str);
		cli_lex_in_ptr_save->buflen = cli_lex_in_ptr->buflen;
		memcpy(cli_lex_in_ptr_save->in_str, cli_lex_in_ptr->in_str, cli_lex_in_ptr->buflen);
	} else
		cli_lex_in_ptr_save = NULL;
	trigger_error = trigger_trgfile_tpwrap(trigger_filename, trigger_filename_len, noprompt);
	if (NULL != cli_lex_in_ptr)
		free(cli_lex_in_ptr);
	cli_lex_in_ptr = cli_lex_in_ptr_save;
	if (trigger_error)
		mupip_exit(ERR_MUNOACTION);
}
