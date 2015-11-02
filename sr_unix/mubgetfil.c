/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "mupip_exit.h"

GBLREF 	mstr		directory;
GBLREF 	bool		is_directory;
GBLREF	bool            error_mupip;
GBLREF	backup_reg_list	*mu_repl_inst_reg_list;

bool mubgetfil(backup_reg_list *list, char *name, unsigned short len)
{
	struct stat	stat_buf;
	int		stat_res;
	char		temp;
	char 		tcp[5];

	if (0 == len)
		return FALSE;
	if (list != mu_repl_inst_reg_list)
	{	/* Do the following checks only if this region does NOT correspond to the replication instance region. */
		if ('|' == *name)
		{
			len -= 1;
			list->backup_to = backup_to_exec;
			list->backup_file.len = len;
			list->backup_file.addr = (char *)malloc(len + 1);
			memcpy(list->backup_file.addr, name + 1, len);
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
				list->backup_file.len = len;
				list->backup_file.addr = (char *)malloc(len + 1);
				memcpy(list->backup_file.addr, name, len);
				*(list->backup_file.addr + len) = 0;
				return TRUE;
			}
		}
	}
	temp = name[len];
	name[len] = 0;
	STAT_FILE(name, &stat_buf, stat_res);
	if (-1 == stat_res)
	{
		if (errno != ENOENT)
		{
			util_out_print("Error accessing backup output file or directory: !AD", TRUE, len, name);
			mupip_exit(errno);
		} else
		{	/* new file */
			list->backup_file.len = len;
			list->backup_file.addr = (char *)malloc(len + 1);
			memcpy(list->backup_file.addr, name, len);
			*(list->backup_file.addr + len) = 0;
		}
	} else if (S_ISDIR(stat_buf.st_mode))
	{
		if (!is_directory)
		{
			is_directory = TRUE;
			directory.len = len;
			directory.addr = (char *)malloc(len + 1);
			memcpy(directory.addr, name, len);
			*(directory.addr + len) = 0;
		}
		mubexpfilnam(name, len, list);
	} else
	{	/* the file already exists */
		list->backup_file.len = len;
		list->backup_file.addr = (char *)malloc(len + 1);
		memcpy(list->backup_file.addr, name, len);
		*(list->backup_file.addr + len) = 0;
	}
	name[len] = temp;
	return TRUE;
}
