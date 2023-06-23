/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CLI_PARSE_H_INCLUDED
#define CLI_PARSE_H_INCLUDED

void	clear_parm_vals(CLI_ENTRY *cmd_parms, boolean_t follow);
int 	find_entry(char *str, CLI_ENTRY *pparm);
int 	find_verb(char *str);
CLI_ENTRY *find_cmd_param(char *str, CLI_ENTRY *pparm, int follow);
int 	parse_arg(CLI_ENTRY *pcmd_parms, int *eof);
int	parse_cmd(void);
CLI_ENTRY *get_parm_entry(char *parm_str);
boolean_t cli_get_parm(char *entry, char val_buf[]);
boolean_t cli_numeric_check(CLI_ENTRY *pparm, char *val_str);
boolean_t cli_get_sub_quals(CLI_ENTRY *pparm);
int	cli_check_negated(char **opt_str_ptr, CLI_ENTRY *pcmd_parm_ptr, CLI_ENTRY **pparm_ptr);
#ifdef GTM_TRIGGER
int	parse_triggerfile_cmd(void);
#endif

#endif
