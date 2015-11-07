/****************************************************************
 *								*
 * Copyright (c) 2009-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_PERMISSIONS
#define GTM_PERMISSIONS

#include <mdefsp.h>

enum perm_target_types
{
    PERM_FILE = 0x01,			/* Request permissions for creating a new file */
    PERM_IPC  = 0x02,			/* Request permissions for initializing IPCs (shm/sem) */
    PERM_EXEC = 0x04			/* Request execute permissions, masked with the above. Currently only used with PERM_IPC */
};

gid_t	gtm_get_group_id(struct stat *stat_buff);
int	gtm_member_group_id(uid_t uid, gid_t gid);
void	gtm_permissions(struct stat *stat_buff, int *user_id, int *group_id, int *perm, enum perm_target_types target_type);

#endif /* GTM_PERMISSIONS */
