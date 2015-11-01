/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __READ_DB_FILES_FROM_GLD_H__
#define __READ_DB_FILES_FROM_GLD_H__

typedef struct gld_dbname_list_struct
{
        struct gld_dbname_list_struct   *next;
	struct file_control_struct	*db_ctl;
	struct gd_region_struct		*gd;
} gld_dbname_list;

gld_dbname_list *read_db_files_from_gld(gd_addr *addr);

#endif
