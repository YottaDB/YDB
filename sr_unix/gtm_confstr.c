/****************************************************************
 *								*
 * Copyright (c) 2017-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_common_defs.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "eintr_wrappers.h"
#include "gtmmsg.h"

error_def(ERR_SYSUTILCONF);

GBLREF    volatile boolean_t      timer_in_handler;

/* Get the path to system utilities and prepend it to the command. Caller sends the max size of the command buffer */
int gtm_confstr(char *command, unsigned int maxsize)
{
	char 		pathbuf[MAX_FN_LEN];
	char 		*cmd_path, *path_tok, *path_tokptr, *cmd_ptr;
	size_t 		n, tok_len, cmdlen;
	int 		status, i;
	struct stat 	sb;

	n = confstr(_CS_PATH, NULL, (size_t) 0);
	assert((MAX_FN_LEN >= n) && (MAX_FN_LEN >= maxsize));
	if ((n > maxsize) || (0 == n))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SYSUTILCONF, 2,
						LEN_AND_LIT("Path for system utilities not defined"));
		return -1;
	}
	confstr(_CS_PATH, pathbuf, n);
	i = 0;
	cmdlen = strnlen(command, MAX_FN_LEN);
	assert(0 < cmdlen); /* Internal commands should never be null */
	path_tok = STRTOK_R(pathbuf, ":", &path_tokptr);
	while (path_tok != NULL)
	{
		tok_len = strlen(path_tok);
		n = (tok_len + cmdlen + 2);
		assert(n < maxsize);
		if (maxsize <= n)	/* Path + command will exceed command buffer size */
			break;
		cmd_ptr = cmd_path = (char *)malloc(n);
		assert(NULL != cmd_path);
		memcpy(cmd_path, path_tok, tok_len); /* For SCI: Use cmd_path for memcpy, since it was the name malloced */
		cmd_ptr += tok_len;
		*cmd_ptr = '/';
		cmd_ptr++;
		memcpy(cmd_ptr, command, cmdlen - 1); /* Don't copy the trailing space in the command */
		cmd_ptr += cmdlen - 1;
		*cmd_ptr = '\0';
		STAT_FILE(cmd_path, &sb, status);
		if (!status && (S_IXUSR & sb.st_mode)) /* File is present and an executable */
		{
			cmdlen = strlen(cmd_path);
			assert(maxsize > (cmdlen + 1));
			if (maxsize <= (cmdlen + 1))	/* Path + command will exceed command buffer size */
			{
				free(cmd_path);
				break;
			}
			assert(MAX_FN_LEN > cmdlen); 		/* For SCI */
			memcpy(command, cmd_path, cmdlen);
			command[cmdlen] = ' ';
			command[cmdlen + 1] = '\0';
			free(cmd_path);
			return 0;
		}
		free(cmd_path);
		path_tok = STRTOK_R(NULL, ":", &path_tokptr);
	}
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SYSUTILCONF, 2,
				LEN_AND_LIT("System utilities not found at the specified path"));
	return -1;
}
