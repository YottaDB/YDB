/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_PERMISSIONS
#define GTM_PERMISSIONS

int gtm_get_group_id(struct stat *stat_buff);
int gtm_member_group_id(int uid, int gid);
void gtm_set_group_and_perm(struct stat *stat_buff, int *group_id, int *perm, int world_write_perm);

#endif /* GTM_PERMISSIONS */
