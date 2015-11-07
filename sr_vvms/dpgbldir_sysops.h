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

#ifndef __DPGBLDIR_SYSOPS_H__
#define  __DPGBLDIR_SYSOPS_H__

bool comp_gd_addr(gd_addr *gd_ptr, struct FAB *file_ptr);
void fill_gd_addr_id(gd_addr *gd_ptr, struct FAB *file_ptr);
void file_read(struct FAB *file_ptr, int4 size, char *buff, int4 pos);
void close_gd_file(struct FAB *file_ptr);
void dpzgbini(void);
mstr *get_name(mstr *ms);

#endif
