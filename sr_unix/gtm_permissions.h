/****************************************************************
 *								*
 * Copyright (c) 2009-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#define MAX_PERM_LEN		12
#define MAX_PRINT_GID_LEN	128
#define GID_ELLIPSIS		"..."
#define GID_ELLIPSIS_LEN	4
#define PERMALL			07777

struct perm_diag_data
{
	uid_t	this_uid;
	gid_t	this_gid;
	uid_t	file_uid;
	gid_t	file_gid;
	char	file_perm[MAX_PERM_LEN];
	gid_t	lib_gid;
	char	lib_perm[MAX_PERM_LEN];
	char	print_gid_list[MAX_PRINT_GID_LEN];	/* Can't imagine this getting any longer in the real world */
	int	print_gid_list_len;
};

error_def(ERR_PERMGENFAIL);
error_def(ERR_PERMGENDIAG);

#define PERMGENDIAG_ARGS(pdd)									\
		ERR_PERMGENDIAG, 11,								\
		(pdd).this_uid, (pdd).this_gid,							\
		(pdd).print_gid_list_len, (pdd).print_gid_list,					\
		(pdd).file_uid, (pdd).file_gid, RTS_ERROR_STRING((pdd).file_perm),		\
		(pdd).lib_gid, RTS_ERROR_STRING((pdd).lib_perm)
#define PERMGENDIAG_ARG_COUNT	(13)

GBLREF	gid_t		*gid_list;
GBLREF	int		gid_list_len;

/* Returns TRUE if GID is in the supplementary GID list of calling process, otherwise FALSE */
#define GID_IN_GID_LIST(GID)	((0 > gid_list_len) ? (gtm_init_gid_list(), gtm_gid_in_gid_list(GID)) : gtm_gid_in_gid_list(GID))

void		gtm_init_gid_list(void);
boolean_t	gtm_gid_in_gid_list(gid_t);
gid_t		gtm_get_group_id(struct stat *stat_buff);
boolean_t	gtm_member_group_id(uid_t uid, gid_t gid, struct perm_diag_data *pdd);
boolean_t	gtm_permissions(struct stat *stat_buff, int *user_id, int *group_id, int *perm, enum perm_target_types target_type,
				struct perm_diag_data *pdd);

#endif /* GTM_PERMISSIONS */
