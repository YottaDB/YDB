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

#ifndef __DBGBLDIR_H__
#define __DBGBLDIR_H__

gd_addr *zgbldir(mval *v);
gd_addr *gd_load(mstr *v);
gd_addr *get_next_gdr(gd_addr *prev);
void cm_add_gdr_ptr(gd_region *greg);
void cm_del_gdr_ptr(gd_region *greg);
boolean_t get_first_gdr_name(gd_addr *current_gd_header, mstr *log_nam);
mstr *get_name(mstr *ms);
void *open_gd_file(mstr *v);

#endif
