/****************************************************************
 *								*
 *	Copyright 2002, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_stdio.h"
#include <errno.h>

#include "gtm_string.h"
#include "gtmmsg.h"
#include "cli.h"
#include "cli_parse.h"
#include "cli_disallow.h"
#include "error.h"
#include "util.h"
#include "mupip_cmd_disallow.h"

GBLREF char	cli_err_str[];
GBLREF char	*cli_err_str_ptr;

/* to check lengths and update cli_err_str*/
void cli_err_strcat(char *str)
{
	int lencli, lenstr;


	lencli = STRLEN(cli_err_str);
	lenstr = STRLEN(str);

	/* No error string should be longer than MAX_CLI_ERR_STR */
	assert(MAX_CLI_ERR_STR > lencli + lenstr + 2);
	memcpy(cli_err_str + lencli," ",1);
	memcpy(cli_err_str + lencli + 1, str, lenstr);
	*(cli_err_str + lencli + lenstr + 1) = '\0';
}

/*----------------------------------------------
 * d_c_cli_present:
 * disallow_check_cli_present
 *
 * This is a wrapper for cli_present to be used
 * from disallow functions (i.e. check_disallow
 * and those it calls).
 *
 * Mimicks VMS CLI.
 *
 * Do not use other than in *_disallow functions.
 *
 * Arguments:
 *	qualifier string
 *
 * Return:
 *	TRUE if present,
 * 	FALSE otherwise
 *----------------------------------------------
 */
boolean_t d_c_cli_present(char *str)
{
	int val;
	char *str_ptr;

	val = cli_present(str);
	str_ptr = strchr(str,DOT);
	if (str_ptr)
		str_ptr++; /* skip the dot */
	else
		str_ptr = str;
	cli_err_strcat(str_ptr);
	return(CLI_PRESENT == val);
}

/*----------------------------------------------
 * d_c_cli_negated
 * disallow_check_cli_negated
 *
 * This is a wrapper for cli_negated to be used
 * from disallow functions (i.e. check_disallow
 * and those it calls).
 *
 * Mimicks NEG operator of VMS CLI.
 *
 * Do not use other than in *_disallow functions.
 *
 * Arguments:
 *	qualifier string
 *
 * Return:
 *	TRUE if negated,
 * 	FALSE otherwise
 *----------------------------------------------
 */
boolean_t d_c_cli_negated(char *str)
{
	boolean_t val;
	char *str_ptr;

	val = cli_negated(str);
	str_ptr = strchr(str,DOT);
	if (str_ptr)
		str_ptr++; /* skip the dot */
	else
		str_ptr = str;
	cli_err_strcat(str_ptr);
	return(val);
}

/*----------------------------------------------
 * cli_check_any2
 *
 * checks if any two of the (many) inputs are
 * non-zero.
 *
 * Mimicks ANY2 operator of VMS CLI.
 *
 * Do not use other than in *_disallow functions.
 *
 * This function is added for ease of port of
 * CLD files.
 *
 * Arguments:
 *	list of booleans
 *
 * Return:
 *	TRUE if any2 condition holds,
 * 	FALSE otherwise
 *----------------------------------------------
 */
boolean_t cli_check_any2(int argcnt, ...)
{
	va_list var;
	int oper, state = 0;

	VAR_START(var, argcnt);
	oper = 0;
	while(argcnt)
	{
		if (va_arg(var, VA_ARG_TYPE_BOOL) && 1 < ++state)
			return TRUE;
		argcnt--;
	}
	va_end(var);
	return FALSE;
}

/*-------------------------------------------------
 * check_disallow
 * Checks whether the disallow condition is met
 * It is called for the tables that are involved
 * in the command being executed (which might be
 * one or two, whether there are extra qualifiers
 * or not)
 *
 * Return:
 * TRUE : ok to go on
 * FALSE: disallowed.
 *
 *-------------------------------------------------
 */
boolean_t check_disallow(CLI_ENTRY *pparm)
{
	static boolean_t (*qual_disallow_func)(void);	/* Ptr to disallow function */
	boolean_t	tmpres;

	/* parm should be NULL only for the extra qualifiers table */
	if (!pparm)
		return TRUE;
	qual_disallow_func = pparm->disallow_func;
	if (NULL == qual_disallow_func)
		return TRUE;
	assert(NULL != pparm->parms);	/* should never add a line in *_cmd.c with a disallow function and no sub-qualifiers */
	/* Copy the error string ahead of time */
	SPRINTF(cli_err_str, "Missing or illegal combination of command elements - check documentation:");
	/* point to the end so that individual disallow functions can fill it */
	cli_err_str_ptr = cli_err_str + strlen(cli_err_str);
	if (qual_disallow_func())
	{
		return FALSE;
	}
	/* If there was no error, clear cli_err_str */
	*cli_err_str = 0;
	return TRUE;
}
