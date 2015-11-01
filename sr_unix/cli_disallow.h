/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __CLI_DISALLOW_H__
#define __CLI_DISALLOW_H__

void cli_err_strcat(char *str);
boolean_t d_c_cli_present(char *str);
boolean_t d_c_cli_negated(char *str);
boolean_t cli_check_any2();
boolean_t check_disallow(CLI_ENTRY *pparm);

#define CLI_DIS_CHECK_N_RESET	if (disallow_return_value) return TRUE; *cli_err_str_ptr = '\0';
#define DOT '.'
#endif
