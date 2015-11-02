/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc	*
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
    PERM_FILE,			/* request permissions for creating a new file */
    PERM_IPC			/* request permissions for initializing IPCs (shm/sem) */
};

struct perm_diag_data
{
	uid_t	process_uid;
	gid_t	process_gid;
	uid_t	file_uid;
	gid_t	file_gid;
	char	file_perm[12];
	gid_t	lib_gid;
	char	lib_perm[12];
	int	opener_in_file_group;
	int	owner_in_file_group;
};

error_def(ERR_PERMGENDIAG);

#define PERMGENDIAG_ARGS(pdd)									\
		ERR_PERMGENDIAG, 11,								\
		(pdd).process_uid, (pdd).process_gid,						\
		(pdd).file_uid, (pdd).file_gid, RTS_ERROR_STRING((pdd).file_perm),		\
		(pdd).lib_gid, RTS_ERROR_STRING((pdd).lib_perm),				\
		(pdd).opener_in_file_group, (pdd).owner_in_file_group
#define PERMGENDIAG_ARG_COUNT	(13)

int gtm_get_group_id(struct stat *stat_buff);
int gtm_member_group_id(int uid, int gid);
int gtm_set_group_and_perm(struct stat *stat_buff, int *group_id, int *perm, enum perm_target_types target_type,
			   struct perm_diag_data *pdd);

#endif /* GTM_PERMISSIONS */
