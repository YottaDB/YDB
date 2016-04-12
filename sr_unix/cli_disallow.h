/****************************************************************
 *								*
 *	Copyright 2002, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CLI_DISALLOW_H
#define CLI_DISALLOW_H

void cli_err_strcat(char *str);
boolean_t d_c_cli_present(char *str);
boolean_t d_c_cli_negated(char *str);
boolean_t cli_check_any2(int argcnt, ...);
boolean_t check_disallow(CLI_ENTRY *pparm);

#define CLI_DIS_CHECK		if (disallow_return_value) return TRUE;
#define CLI_DIS_CHECK_N_RESET	CLI_DIS_CHECK; *cli_err_str_ptr = '\0';
#define DOT '.'
#endif
