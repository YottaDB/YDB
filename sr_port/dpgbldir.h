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

boolean_t	get_first_gdr_name(gd_addr *current_gd_header, mstr *log_nam);
gd_addr		*zgbldir(mval *v);
gd_addr		*gd_load(mstr *v);
gd_addr		*get_next_gdr(gd_addr *prev);
mstr		*get_name(mstr *ms);
void		cm_add_gdr_ptr(gd_region *greg);
void		cm_del_gdr_ptr(gd_region *greg);
void		*open_gd_file(mstr *v);
void		gd_rundown(void);
void		gd_ht_kill(struct htab_desc_struct *table, boolean_t contents);

#endif
