/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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
#include "gtm_stdlib.h"

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

/** This was originally a macro, but was changed to an inline to make veracode
 * checking easier.  Also, it originally directly set a passed in destination pointer
 * and it now returns a pointer value instead.
 * Function to copy a source string to a malloced area that is set to the destination pointer.
 * Since it is possible that any destination might have multiple pointer dereferences in its usage, we
 * use a local pointer variable and finally return its value for assignment thereby avoiding duplication of
 * those pointer dereferences (one for the malloc and one for the strcpy).
 *
 * @param src Source string to copy
 * @return Pointer to the newly malloced copy of the source
 */
static inline char *malloc_cpy_str(char *src)
{
	char	*mcs_ptr;
	size_t  slen;
	size_t	mcs_len;

	slen = strlen(src);
	mcs_len = (slen < SIZE_MAX) ? slen + 1 : SIZE_MAX;	/* (theoritical) overflow guard */
	mcs_ptr = malloc(mcs_len);
	assert(mcs_ptr);
	memcpy(mcs_ptr, src, mcs_len);
	return(mcs_ptr);
}
#endif
