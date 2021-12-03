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

#include "mdef.h"

#include <errno.h>
#include "gtm_stat.h"
#include "gtm_string.h"
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "iosp.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mupipbckup.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"
#include "tp.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "mupip_exit.h"

GBLREF	backup_reg_list	*mu_repl_inst_reg_list;
GBLREF 	boolean_t	is_directory;
GBLREF 	mstr		directory;

#define	SET_BACKUP_FILE_LIST(BACKUPFILE, FULLPATH, LEN)		\
{								\
	BACKUPFILE.len = LEN;					\
	BACKUPFILE.addr = (char *)malloc(LEN + 1);		\
	assert(NULL != BACKUPFILE.addr);			\
	memcpy(BACKUPFILE.addr, FULLPATH, LEN);			\
	*(BACKUPFILE.addr + LEN) = 0;				\
}

boolean_t mubgetfil(backup_reg_list *list, char *name, unsigned short len)
{
	char		tcp[5], temp, fullpath[GTM_PATH_MAX];
	uint4		fullpathlen, status;
	int		stat_res;
	struct stat	stat_buf;
	boolean_t	newfile = FALSE;

	if (0 == len)
		return FALSE;
	if (list != mu_repl_inst_reg_list)
	{	/* Do the following checks only if this region does NOT correspond to the replication instance region. */
		if ('|' == *name)
		{
			len -= 1;
			list->backup_to = backup_to_exec;
			SET_BACKUP_FILE_LIST(list->backup_file, name + 1, len);
			return TRUE;
		}
		if (len > 5)
		{
			lower_to_upper((uchar_ptr_t)tcp, (uchar_ptr_t)name, 5);
			if (0 == memcmp(tcp, "TCP:/", 5))
			{
				list->backup_to = backup_to_tcp;
				len -= 5;
				name += 5;
				while ('/' == *name)
				{
					len--;
					name++;
				}
				SET_BACKUP_FILE_LIST(list->backup_file, name, len);
				return TRUE;
			}
		}
	}
	temp = name[len];
	name[len] = '\0';
	STAT_FILE(name, &stat_buf, stat_res);
	get_full_path(name, len, fullpath, &fullpathlen, GTM_PATH_MAX, &status);
	if (-1 == stat_res)
	{
		if (errno != ENOENT)
		{
			util_out_print("Error accessing backup output file or directory: !AD", TRUE, len, name);
			mupip_exit(errno);
		} else
		{	/* new file */
			SET_BACKUP_FILE_LIST(list->backup_file, fullpath, fullpathlen);
		}
	} else if (S_ISDIR(stat_buf.st_mode))
	{
		assert(!newfile);
		if (!is_directory)
		{
			is_directory = TRUE;
			SET_BACKUP_FILE_LIST(directory, fullpath, fullpathlen);
		}
		mubexpfilnam(fullpath, fullpathlen, list);
	} else
	{	/* The file already exists. */
		assert(!newfile);
		SET_BACKUP_FILE_LIST(list->backup_file, fullpath, fullpathlen)
	}
	name[len] = temp;
	return TRUE;
}
