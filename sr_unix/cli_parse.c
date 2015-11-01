/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "cli.h"
#include "cli_parse.h"
#include "error.h"

#define	NO_STRING	"NO"

GBLDEF char	 	parm_ary[MAX_PARMS][MAX_LINE];	/* Parameter strings buffers */
GBLDEF unsigned int	parms_cnt;			/* Parameters count */
GBLDEF void 		(*func)(void);			/* Function to be dispatched
									to for this command */

static char 		cli_err_str[MAX_ERR_STR];	/* Parse Error message buffer */
static CLI_ENTRY 	*gpqual_root;			/* pointer to root of
								subordinate qualifier table */
static CLI_ENTRY 	*gpcmd_qual;			/* pointer command qualifier table */
static CLI_ENTRY 	*gpcmd_verb;			/* pointer to verb table */
static CLI_PARM 	*gpcmd_parm_vals;		/* pointer to parameters for command */

GBLREF char 		cli_token_buf[];
GBLREF CLI_ENTRY 	cmd_ary[];

GBLREF IN_PARMS *cli_lex_in_ptr;



/*
 * -----------------------------------------------
 * Clear all parameter values and flags of command
 * parameter table for a given command.
 *
 * Arguments:
 *	cmd_parms	- address of parameter array
 *
 * Return:
 *	none
 * -----------------------------------------------
 */
void	clear_parm_vals(CLI_ENTRY *cmd_parms) 		/* pointer to option's parameter table */
{
	CLI_ENTRY	*root_param, *find_cmd_param();
	int		need_copy;

	need_copy = (gpcmd_qual != cmd_parms);

	while (strlen(cmd_parms->name) > 0)
	{
		if (cmd_parms->pval_str)
			free(cmd_parms->pval_str);
		/* if root table exists, copy over any qualifier values to the new parameter table */
		if (need_copy && (root_param = find_cmd_param(cmd_parms->name, gpcmd_qual)))
		{
			cmd_parms->pval_str = root_param->pval_str;
			cmd_parms->negated = root_param->negated;
			cmd_parms->present = root_param->present;
			root_param->pval_str = 0;
		} else
		{
			cmd_parms->negated = 0;
			cmd_parms->present = 0;
			cmd_parms->pval_str = 0;
		}
		cmd_parms++;
	}
}

/*
 * ---------------------------------------------------------
 * Find entry in the qualifier table
 *
 * Arguments:
 *	str	- parameter string
 *	pparm	- pointer to parameter table for this command
 *
 * Return:
 *	if found, index into command table array,
 *	else -1
 * ---------------------------------------------------------
 */
int 	find_entry(char *str, CLI_ENTRY *pparm)
{
	int 	match_ind, res;
	int 	ind = 0;
	char 	*sp;

	match_ind = -1;
	cli_strupper(str);

	while (0 < strlen(sp = (pparm + ind)->name))
	{
		if (0 == (res = strncmp(sp, str, strlen(str))))
		{
			if (-1 != match_ind)	 /* If a second match exists */
				return(-1);
			else
				match_ind = ind;
		}
		else
		{
			if (0 < res)
				break;
		}
		ind++;
	}
	if (-1 != match_ind && gpqual_root && 0 == strncmp(gpqual_root->name, str, strlen(str)))
		return(-1);
	return(match_ind);
}



/*
 * -----------------------------------------------
 * Find command in command table
 *
 * Arguments:
 *	str	- command string
 *
 * Return:
 *	if found, index into command table array,
 *	else -1
 * -----------------------------------------------
 */
int 	find_verb(char *str)
{
	return(find_entry(str, cmd_ary));
}


/*
 * ---------------------------------------------------------
 * Find command parameter in command parameter table
 *
 * Arguments:
 *	str	- parameter string
 *	pparm	- pointer to parameter table for this command
 *
 * Return:
 *	if found, pointer to parameter structure
 *	else 0
 * ---------------------------------------------------------
 */
CLI_ENTRY *find_cmd_param(char *str, CLI_ENTRY *pparm)
{
	int 	ind;

	if (0 <= (ind = find_entry(str, pparm)))
		return(pparm + ind);
	else
		return(0);
}


/*
 * ---------------------------------------------------------
 * Parse one option.
 * Read tokens from the input.
 * Check if it is a valid qualifier or parameter.
 * If it is a parameter, get it, and save it in the
 * global parameter array.
 * If it is a qualifier, get its value and save it in a value table,
 * corresponding to this option.
 *
 * Arguments:
 *	pcmd_parms	- pointer to command parameter table
 *	eof		- pointer to end of file flag
 *
 * Return:
 *	1 - option parsed OK
 *	-1 - failure to parse option
 *	0 - no more tokens, in which case
 *		the eof flag is set on end of file.
 * ---------------------------------------------------------
 */
int 	parse_arg(CLI_ENTRY *pcmd_parms, int *eof)
{
	CLI_ENTRY 	*pparm;
	char 		*opt_str, *val_str;
	int 		neg_flg;

	/* -----------------------------------------
	 * get qualifier marker, or parameter token
	 * -----------------------------------------
	 */

	if (VAL_LIST == gpcmd_verb->val_type && parms_cnt == gpcmd_verb->max_parms)
		return(0);
	if (!cli_look_next_token(eof))
		return(0);

	/* -------------------------------------------------------------------
	 * here cli_token_buf is set by the previous cli_look_next_token(eof)
	 * call itself since it in turn calls cli_gettoken()
	 * -------------------------------------------------------------------
	 */

	if (!cli_is_qualif(cli_token_buf) && !cli_is_assign(cli_token_buf))
	{
		/* ----------------------------------------------------
		 * If token is not a qualifier, it must be a parameter
		 * ----------------------------------------------------
		 */

		/* ------------------------------------------------------------
		 * no need to check for eof on cli_get_string_token(eof) since
		 * already checked that on the previous cli_look_next_token.
		 * now you have to skip initial white spaces before reading
		 * the string since cli_get_string_token considers a space
		 * as a blank token. hence the need for the skip_white_space()
		 * call.
		 * ------------------------------------------------------------
		 */

		skip_white_space();
		cli_get_string_token(eof);

		if (parms_cnt >= gpcmd_verb->max_parms)
		{
			SPRINTF(cli_err_str, "Too many parameters ");
			return(-1);
		}

		strcpy(parm_ary[parms_cnt++], cli_token_buf);
		return(1);
	}

	/* ---------------------------------------------------------------------
	 * cli_gettoken(eof) need not be checked for return value since earlier
	 * itself we have checked for return value in cli_look_next_token(eof)
	 * ---------------------------------------------------------------------
	 */

	cli_gettoken(eof);
	opt_str = cli_token_buf;

	if (!pcmd_parms)
	{
		SPRINTF(cli_err_str, "No qualifiers allowed for this command");
		return(-1);
	}

	/* ------------------------------------------
	 * Qualifiers must start with qualifier token
	 * ------------------------------------------
	 */

	if (!cli_is_qualif(cli_token_buf))
	{
		SPRINTF(cli_err_str, "Qualifier expected instead of : %s ", opt_str);
		return(-1);
	}

	/* -------------------------
	 * Get the qualifier string
	 * -------------------------
	 */

	if (!cli_look_next_token(eof) || 0 == cli_gettoken(eof))
	{
		SPRINTF(cli_err_str, "Qualifier string missing %s ", opt_str);
		return(-1);
	}

	/* ---------------------------------------
	 * Fold the qualifier string to upper case
	 * ---------------------------------------
	 */

	cli_strupper(opt_str);

	/* -------------------------
	 * See if option is negated
	 * -------------------------
	 */

	if (0 == strncmp(opt_str, NO_STRING, strlen(NO_STRING)))
	{
		opt_str += strlen(NO_STRING);
		neg_flg = 1;
	}
	else
		neg_flg = 0;

	/* --------------------------------------------
	 * search qualifier table for qualifier string
	 * --------------------------------------------
	 */

	if (0 == (pparm = find_cmd_param(opt_str, pcmd_parms)))
	{
		SPRINTF(cli_err_str, "Unrecognized option : %s", opt_str);
		cli_lex_in_ptr->tp = 0;
		return(-1);
	}

	/* --------------------------------------------------
	 * If there is another level, update global pointers
	 * --------------------------------------------------
	 */

	if (pparm->parms)
	{
		func = pparm->func;
		gpqual_root = pparm;
		clear_parm_vals(pparm->parms);
		gpcmd_qual = pparm->parms;
	}

	/* ----------------------------------------------------
	 * if option is negated and it is not negatable, error
	 * ----------------------------------------------------
	 */

	if (!pparm->negatable && neg_flg)
	{
		SPRINTF(cli_err_str, "Option %s may not be negated", opt_str);
		return(-1);
	}

	/* -------------------------------------------------------------
	 * If value is disallowed for this qualifier, and an assignment
	 * token is encounter, report error, values not allowed for
	 * negated qualifiers
	 * -------------------------------------------------------------
	 */

	if (neg_flg || VAL_DISALLOWED == pparm->required)
	{
		if (cli_look_next_token(eof) && cli_is_assign(cli_token_buf))
		{
			SPRINTF(cli_err_str,
			  "Assignment is not allowed for this option : %s",
			  pparm->name);
			return(-1);
		}
	}
	else
	{
		/* --------------------------------------------------
		 * Get Value either optional, or required.
		 * In either case, there must be an assignment token
		 * --------------------------------------------------
		 */

		if (!cli_look_next_token(eof) || !cli_is_assign(cli_token_buf))
		{
	    		if (VAL_REQ == pparm->required)
			{
				SPRINTF(cli_err_str, "Option : %s needs value", pparm->name);
				return(-1);
			}
			else
			{
				pparm->negated = neg_flg;
				pparm->present = 1;

				assert(0 != pparm->parm_values);

				/* -------------------------------
				 * Allocate memory and save value
				 * -------------------------------
				 */

				pparm->pval_str = malloc(strlen(pparm->parm_values->prompt) + 1);
				strcpy(pparm->pval_str, pparm->parm_values->prompt);

				return(1);
			}
		}

 		cli_gettoken(eof);

		/* ---------------------------------
		 * Get the assignment token + value
		 * ---------------------------------
		 */

		if (!cli_is_assign(cli_token_buf))
		{
			SPRINTF(cli_err_str,
			  "Assignment missing after option : %s",
			  pparm->name);
			return(-1);
		}

		/* --------------------------------------------------------
		 * get the value token, "=" is NOT a token terminator here
		 * --------------------------------------------------------
		 */

		if (!cli_look_next_string_token(eof) || 0 == cli_get_string_token(eof))
		{
			SPRINTF(cli_err_str,
			  "Unrecognized option : %s, value expected but not found",
			  pparm->name);
			cli_lex_in_ptr->tp = 0;
			return(-1);
		}

		val_str = cli_token_buf;

		if (VAL_NUM == pparm->val_type)
		{
			if (pparm->hex_num)
			{
				if (!cli_is_hex(val_str))
				{
					SPRINTF(cli_err_str,
					  "Unrecognized value: %s, HEX number expected",
				  	  val_str);
					cli_lex_in_ptr->tp = 0;
					return(-1);
				}
			}
			else if (!cli_is_dcm(val_str))
			{
				SPRINTF(cli_err_str,
			  	  "Unrecognized value: %s, Decimal number expected",
			  	  val_str);
				cli_lex_in_ptr->tp = 0;
				return(-1);
			}
		}

		/* -------------------------------
		 * Allocate memory and save value
		 * -------------------------------
		 */

		pparm->pval_str = malloc(strlen(cli_token_buf) + 1);
		strcpy(pparm->pval_str, cli_token_buf);
	}
	pparm->negated = neg_flg;
	pparm->present = 1;

	return(1);
}

/*
 * -----------------------------------------------------------
 * Parse one command.
 * Get tokens from the input stream.
 * See if the first token is a command name, as it should be,
 * and if it is, check if optional arguments that follow,
 * are legal, and if they are, get their values and
 * save them in a value table, corresponding to this
 * option.
 * If any of these conditions are not met, parse error occures.
 *
 * Return:
 *	1 - command parsed OK
 *	0 - failure to parse command
 *	EOF - end of file
 * -----------------------------------------------------------
 */
int parse_cmd(void)
{
	int 	res, cmd_ind;
	char 	*cmd_str;
	int 	opt_cnt = 0;
	int 	eof, cmd_err;

	gpqual_root = 0;
	func = 0;
	cmd_err = 0;
	parms_cnt = 0;			/* Parameters count */
	*cli_err_str = 0;

	cmd_str = cli_token_buf;

	/* ------------------------
	 * If no more tokens exist
	 * ------------------------
	 */

	if (0 == cli_gettoken(&eof))
	{
		if (eof)
			return(EOF);
		return(0);
	}

	/* ------------------------------
	 * Find command in command table
	 * ------------------------------
	 */

	if (-1 != (cmd_ind = find_verb(cmd_str)))
	{
		gpcmd_qual = cmd_ary[cmd_ind].parms;
		gpcmd_parm_vals = cmd_ary[cmd_ind].parm_values;
		gpcmd_verb = &cmd_ary[cmd_ind];
		if (gpcmd_qual)
			clear_parm_vals(gpcmd_qual);

		func = cmd_ary[cmd_ind].func;

		/* ----------------------
		 * Parse command options
		 * ----------------------
		 */

		do
		{
			res = parse_arg(gpcmd_qual, &eof);
			if (1 == res)
			{
				opt_cnt++;
			}
		} while (1 == res);
	}
	else
	{
		SPRINTF(cli_err_str, "Unrecognized command: %s", cmd_str);
		cli_lex_in_ptr->tp = 0;
		res = -1;
	}
	if (1 > opt_cnt && -1 != res
	    && VAL_REQ == cmd_ary[cmd_ind].required)
	{
		SPRINTF(cli_err_str,
		  "Command argument expected, but not found");
		res = -1;
	}

	/* -------------------------------------
	 * If parse error, display error string
	 * -------------------------------------
	 */

	if (-1 == res)
	{
		func = 0;
		PRINTF("%s\n", cli_err_str);
	}
	else
	{
		return(1);
	}

	/* -------------------------
	 * If gettoken returned EOF
	 * -------------------------
	 */

	if (eof)
		return(EOF);
	else
		return(0);
}


/*
 * ------------------------------------------------------------
 * See if command parameter is present on the command line,
 * and if it is, return the pointer to its table entry.
 *
 * Arguments:
 *	parm_str	- parameter string to search for
 *
 * Return:
 *	0 - not present
 *	pointer to parameter entry
 * ------------------------------------------------------------
 */
CLI_ENTRY *get_parm_entry(char *parm_str)
{
	CLI_ENTRY	*pparm;
	bool		root_match;
	char		local_str[MAX_LINE];
	error_def(ERR_MUPCLIERR);

	strncpy(local_str, parm_str, sizeof(local_str) - 1);
	root_match = (gpqual_root && !strncmp(gpqual_root->name, local_str, strlen(local_str)));

	/* ---------------------------------------
	 * search qualifier table for this option
	 * ---------------------------------------
	 */

	if (!gpcmd_qual)
		return(NULL);
	pparm = find_cmd_param(local_str, gpcmd_qual);
	if (root_match && !pparm)
		return(gpqual_root);
	else if (pparm)
		return(pparm);
	else
		return(NULL);
}


/*
 * ------------------------------------------------------------
 * See if command parameter is present on the command line
 *
 * Arguments:
 *	entry	- parameter string to search for
 *
 * Return:
 *	0 - not present
 *	<> 0 - present
 * ------------------------------------------------------------
 */
int cli_present(char *entry)
{
	CLI_ENTRY	*pparm;
	char		local_str[MAX_LINE];

	strncpy(local_str, entry, sizeof(local_str) - 1);

	cli_strupper(local_str);
	if (pparm = get_parm_entry(local_str))
	{
		if (pparm->negated)
			return(CLI_NEGATED);
		if (pparm->present)
			return(CLI_PRESENT);
	}

	return(FALSE);
}


/*
 * ------------------------------------------------------------
 * Get the command parameter value
 *
 * Arguments:
 *	entry		- parameter string to search for
 *	val_buf		- if parameter is present, it is copied to
 *			  this buffer.
 *
 * Return:
 *	0 - not present
 *	<> 0 - ok
 * ------------------------------------------------------------
 */
bool cli_get_value(char *entry, char val_buf[])
{
	CLI_ENTRY	*pparm;
	char		local_str[MAX_LINE];

	strncpy(local_str, entry, sizeof(local_str) - 1);

	cli_strupper(local_str);
	if (NULL == (pparm = get_parm_entry(local_str)))
	{
		return(FALSE);
	}

	if (!pparm->present || NULL == pparm->pval_str)
		return(FALSE);
	else
		strcpy(val_buf, pparm->pval_str);

	return(TRUE);
}

/*
 * --------------------------------------------------
 * See if the qualifier given by 'entry' is negated
 * on the command line
 *
 * Return:
 *	TRUE	- Negated
 *	FALSE	- otherwise
 * --------------------------------------------------
 */
bool cli_negated(char *entry) 		/* entity */
{
	CLI_ENTRY	*pparm;
	char		local_str[MAX_LINE];

	strncpy(local_str, entry, sizeof(local_str) - 1);
	cli_strupper(local_str);

	if (pparm = get_parm_entry(local_str))
	{
		return(pparm->negated);
	}

	return(FALSE);
}


bool cli_get_parm(char *entry, char val_buf[])
{
	char		*sp;
	int		ind = 0;
	int		match_ind, res;
	char		local_str[MAX_LINE];
	int		eof;
	char		*gets_res;

	assert(0 != gpcmd_parm_vals);
	strncpy(local_str, entry, sizeof(local_str) - 1);
	cli_strupper(local_str);
	match_ind = -1;

	while (0 < strlen(sp = (gpcmd_parm_vals + ind)->name))
	{
		if (0 == (res = strncmp(sp, local_str, strlen(local_str))))
		{	if (-1 != match_ind)
				return(FALSE);
			else
				match_ind = ind;
		}
		else
		{	if (0 < res)
				break;
		}
		ind++;
	}

	if (-1 != match_ind)
	{
		/* ---------------------------
		 * If no value, prompt for it
		 * ---------------------------
		 */

		if (0 == parm_ary[match_ind][0])
		{
			PRINTF("%s", (gpcmd_parm_vals + match_ind)->prompt);
			GETS((char *)parm_ary[match_ind], gets_res);
		}
		else if ((char)-1 == parm_ary[match_ind][0])
		{
			return(FALSE);
		}

		memcpy(val_buf, parm_ary[match_ind], strlen(parm_ary[match_ind]) + 1);
		if (!cli_look_next_token(&eof) || (0 == cli_gettoken(&eof)))
		{
			parm_ary[match_ind][0] = (char)-1;
		}
		else
		{	if (MAX_LINE < (strlen(cli_token_buf) + 1))
			{
				PRINTF("Parameter string too long\n");
				return(FALSE);
			}
			memcpy(parm_ary[match_ind], cli_token_buf, strlen(cli_token_buf) + 1);
		}
	}
	else
	{
		/* -----------------
		 * check qualifiers
		 * -----------------
		 */

		if (!cli_get_value(local_str, val_buf))
			return(FALSE);
	}
	return(TRUE);
}
