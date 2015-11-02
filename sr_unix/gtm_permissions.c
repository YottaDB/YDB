/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "eintr_wrappers.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_limits.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_pwd.h"
#include <grp.h>
#include "gtm_permissions.h"

#if defined(__hpux) && defined(__hppa)
#	define LIBGTMSHR "%s/libgtmshr.sl"
#elif defined(__MVS__)
#	define LIBGTMSHR "%s/libgtmshr.dll"
#else
#	define LIBGTMSHR "%s/libgtmshr.so"
#endif
/* Return the group id of the distribution based on libgtmshr.xx[x]. If there is some
   problem accessing that file then return -1 which signals no change to group.  Otherwise,
   the pointer to the stat buffer will contain the result of the call to STAT_FILE */
int gtm_get_group_id(struct stat *stat_buff)
{
	char		*env_var;
	int		ret_stat;
	char		temp[PATH_MAX + SIZEOF("libgtmshr.dll")];

	env_var = GETENV("gtm_dist");
	if (NULL != env_var)
	{
		/* build a path to libgtmshr.so or .sl on hpux or .dll on zos */
		SPRINTF(temp, LIBGTMSHR, env_var);
		STAT_FILE(temp, stat_buff, ret_stat);
		if (0 == ret_stat)
			return(stat_buff->st_gid);
	}
	/* return a -1 if $gtm_dist found or if STAT_FILE returned a -1 */
	return(-1);
}

/* Return TRUE is the "uid" parameter is a member of the "gid" group parameter.
   Return FALSE if it is not. */
int gtm_member_group_id(int uid, int gid)
{
	struct group	*grp;
	struct passwd	*pwd, *pwd2;

	/* get group id for database */
	grp = getgrgid(gid);
	if (NULL == grp)
		return(FALSE); 	/* if group id not found then assume uid not a member */
	pwd = getpwuid(uid);
	if (NULL == pwd)
		return(FALSE); 	/* if user id not found then assume uid not a member */
	/* if the gid of the file is the same as the gid for the process uid we are done */
	if (gid == pwd->pw_gid)
		return(TRUE);
	else
	{
		/*
		 * Otherwise we have to compare the name stored in pwd struct
		 * with the names of the group members in the group struct.
		 */
		while (NULL != *(grp->gr_mem))
		{
			if (!strcmp(pwd->pw_name, *(grp->gr_mem++)))
				return(TRUE);
		}

		return(FALSE);
	}
}

/* Based on security rules in this routine, set *group_id of the group to be used
   for shared resources, journals, and temp files.  If a no change then it will be set to -1.
   Also, set *perm to the permissions to be used.  The precalculated world_write_perm (need to
   change name to masked_permissions) is to be used in the one case indicated below. */
void gtm_set_group_and_perm(struct stat *stat_buff, int *group_id, int *perm, int world_write_perm)
{
	int		lib_gid = -1;
	int		use_world_writeable;
	uid_t		process_uid;
	uid_t		db_uid;
	struct stat	dist_stat_buff;

	/* if database is world writeable then select no change to group and set permission to "and" of 0666 with database mode */
	if (02 & stat_buff->st_mode)
	{
			*group_id = -1;
			*perm = stat_buff->st_mode & 0666;
			return;
	}
	/* default to database gid */
	*group_id = stat_buff->st_gid;
	/* set process_uid to uid of current process and see if member of database group */
	process_uid = GETUID();
	/* get database uid */
	db_uid = stat_buff->st_uid;
	/* see if db_uid is a member of database group and process_uid is a member of database group */
	if ((FALSE != gtm_member_group_id(db_uid, *group_id)) && (FALSE != gtm_member_group_id(process_uid, *group_id)))
			use_world_writeable = TRUE;
	else
			use_world_writeable = FALSE;
	/* if (database is world writeable or database uid is member of database group)
	 * and process_uid is a member of database group then set perm and use database gid
	 */
	if (FALSE != use_world_writeable)
			*perm = world_write_perm;
	else
	{       /* get the gid of the distribution */
			lib_gid = gtm_get_group_id(&dist_stat_buff);
			/* see if database uid is a member of distribution group and process_uid is a member of dist_group_id */
			if ((-1 != lib_gid) && (FALSE != gtm_member_group_id(db_uid, lib_gid))
					&& (FALSE != gtm_member_group_id(process_uid, lib_gid)))
			{
					*group_id = lib_gid;
					*perm = 0660;
			} else
			{
					/* no change to group id */
					*group_id = -1;
					*perm = 0666;
			}
	}
}
