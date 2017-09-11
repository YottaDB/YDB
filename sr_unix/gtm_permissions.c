/****************************************************************
 *								*
 * Copyright (c) 2009-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "send_msg.h"
#include "eintr_wrappers.h"

GBLDEF		gid_t		*gid_list = NULL;
GBLDEF		int		gid_list_len = 0;

GBLREF		char		gtm_dist[GTM_PATH_MAX];
GBLREF		boolean_t	gtm_dist_ok_to_use;

#if defined(__MVS__)
#	define LIBGTMSHR "%s/libgtmshr.dll"
#else
#	define LIBGTMSHR "%s/libgtmshr.so"
#endif

/* Get the process's group list and stash the information to avoid repeated calls */
void gtm_init_gid_list(void)
{
	int	tmp_gid_list_len;

	assert(NULL == gid_list);
	tmp_gid_list_len = getgroups(0, NULL);
	assert(0 <= tmp_gid_list_len);
	if (0 < tmp_gid_list_len)
	{
		gid_list = malloc(tmp_gid_list_len * SIZEOF(gid_t));
		gid_list_len = getgroups(tmp_gid_list_len, gid_list);
		assert(gid_list_len == tmp_gid_list_len);
		if (gid_list_len != tmp_gid_list_len)
		{
			gid_list_len = 0;
			free(gid_list);
			gid_list = NULL;
		}
	}
}

/* Search through the supplementary gid list for a match */
boolean_t	gtm_gid_in_gid_list(gid_t gid)
{
	int	i;

	assert(NULL != gid_list);
	if (NULL == gid_list)
		return FALSE;
	for (i = 0; i < gid_list_len; i++)
		if (gid == gid_list[i])
			return TRUE;
	return FALSE;
}
/* Return the group id of the distribution based on libgtmshr.xx[x]. If there is some
 * problem accessing that file then return INVALID_GID which signals no change to group.  Otherwise,
 * the pointer to the stat buffer will contain the result of the call to STAT_FILE.
 */
gid_t	gtm_get_group_id(struct stat *stat_buff)
{
	int			ret_stat;
	char			temp[GTM_PATH_MAX];
	static boolean_t	first_time = TRUE;
	static struct stat	st_buff;

	if (!first_time)
	{
		*stat_buff = st_buff;
		return st_buff.st_gid;
	}
	if (gtm_dist_ok_to_use)
	{
		/* build a path to libgtmshr.so or .sl on hpux or .dll on zos */
		SNPRINTF(temp, SIZEOF(temp), LIBGTMSHR, gtm_dist);
		STAT_FILE(temp, stat_buff, ret_stat);
		if (0 == ret_stat)
		{
			first_time = FALSE;
			st_buff = *stat_buff;
			return(stat_buff->st_gid);
		}
	}
	/* return INVALID_GID if STAT_FILE returned a -1 or gtm_dist has not been validated */
	return (INVALID_GID);
}

/* Return TRUE if the "uid" parameter is a member of the "gid" group parameter.
 * Return FALSE if it is not.
 */
boolean_t gtm_member_group_id(uid_t uid, gid_t gid, struct perm_diag_data *pdd)
{
	struct group	*grp;
	struct passwd	*pwd;

#	ifdef DEBUG
	if (WBTEST_HELPOUT_FAILGROUPCHECK == gtm_white_box_test_case_number)
	{
		if (NULL == gid_list)
			gtm_init_gid_list();
		return FALSE;
	}
#	endif
	/* Effective user is uid ?*/
	if (GETEUID() == uid)
	{	/* Effective group is gid? */
		if (GETEGID() == gid)
			return(TRUE);
		/* EUID == uid but check if gid is in any groups associated with the process */
		return GID_IN_GID_LIST(gid);
	}
	/* Get "uid" details */
	pwd = getpwuid(uid);
	if (NULL == pwd)
		return(FALSE); 	/* If user id not found then assume uid not a member */
	/* If the gid of the file is the same as the gid for the process uid we are done */
	if (gid == pwd->pw_gid)
		return(TRUE);
	/* Else get "gid" details */
	grp = getgrgid(gid);
	if (NULL == grp)
		return(FALSE); 	/* If group id not found then assume uid not a member */
	/* Else we have to compare the name stored in pwd struct with the names of the group members in the group struct */
	while (NULL != *(grp->gr_mem))
	{
		if (!strcmp(pwd->pw_name, *(grp->gr_mem++)))
			return(TRUE);
	}
	return(FALSE);
}

/* Based on security rules in this routine, set
 *	a) *group_id to the group to be used for shared resources (journals, temp files etc.).
 *		If no change, will be set to INVALID_GID.
 *	b) *perm to the permissions to be used.
 *	c) *user_id to the user id to be used.
 *		If the user is root, *user_id may be set to a target uid if needed. Otherwise, it will be set to INVALID_UID.
 */
boolean_t gtm_permissions(struct stat *stat_buff, int *user_id, int *group_id, int *perm,
			  enum perm_target_types target_type, struct perm_diag_data *pdd)
{
	uid_t		this_uid, file_uid;
	gid_t		this_gid, file_gid;
	gid_t		gtm_dist_gid = INVALID_GID;
	struct stat	dist_stat_buff;
	int		this_uid_is_file_owner;
	int		this_uid_is_root;
	int		this_uid_in_file_group;
	int		owner_in_file_group;
	int		gtm_group_restricted;
	int		file_owner_perms, file_group_perms, file_other_perms;
	int		new_owner_perms, new_group_perms, new_other_perms;
	char		*strnow, *strtop;
	int		i, len;
	mode_t		st_mode, dir_mode = 0;

	/* get this_uid/gid */
	this_uid = GETEUID();
	this_gid = GETEGID();
	file_uid = stat_buff->st_uid;	/* get owning file uid */
	file_gid = stat_buff->st_gid;	/* get owning file gid */
	/* set variables for permission logic */
	this_uid_is_file_owner = (this_uid == file_uid);
	this_uid_is_root = (0 == this_uid);
	this_uid_in_file_group = gtm_member_group_id(this_uid, file_gid, pdd);
	owner_in_file_group = gtm_member_group_id(file_uid, file_gid, pdd);
	*user_id = INVALID_UID;		/* set default uid */
	*group_id = INVALID_GID;	/* set default gid */
	assert((PERM_FILE & target_type) || (PERM_IPC & target_type));	/* code below relies on this */
	st_mode = stat_buff->st_mode;
	/* General rule for permissions below. If target is a FILE (not an IPC, i.e. "(target_type & PERM_FILE) == TRUE",
	 * and if "this_uid_is_file_owner" is FALSE but "this_uid_in_file_group" is TRUE, copy over group permissions of
	 * source file into owner permissions of the target file. This ensures that the target file is accessible by the
	 * new owner (who is governed by the owner permissions) as well as other members of the group owning the source
	 * file (and in turn the target file).
	 */
	file_owner_perms = (0600 & st_mode);
	file_group_perms = (0060 & st_mode);
	if (file_owner_perms && !(0066 & st_mode))
	{	/* File is only user accessible */
		/* Use default group */
		assert(this_uid_is_file_owner || this_uid_is_root);
		if (this_uid_is_root)
		{
			*user_id = file_uid;
			*group_id = file_gid;
		}
		/* Else: use default uid/gid */
		new_owner_perms = ((PERM_FILE & target_type) ? file_owner_perms : 0600);
		*perm = new_owner_perms;	/* read and/or write for ONLY user and none for group/other */
	} else if (file_group_perms && !(0606 & st_mode))
	{	/* File is only group accessible */
		if (this_uid_is_root)
		{
			*user_id = file_uid;
			new_group_perms = ((PERM_FILE & target_type)) ? file_group_perms : 0060;
			*perm = new_group_perms;
		} else
		{
			/* No need to initialize *user_id. Use default uid instead. */
			if (PERM_FILE & target_type)
			{
				new_owner_perms = (file_group_perms << 3);
				new_group_perms = file_group_perms;
			} else
			{
				new_owner_perms = 0600;
				new_group_perms = 0060;
			}
			*perm = new_owner_perms | new_group_perms;
		}
		*group_id = file_gid;			/* use file group */
	} else
	{	/* File is other accessible OR accessible to user and group but not other */
		file_other_perms = (0006 & st_mode);
		if (PERM_IPC & target_type)
		{
			new_owner_perms = file_owner_perms ? 0600 : 0000;
			new_group_perms = file_group_perms ? 0060 : 0000;
			new_other_perms = file_other_perms ? 0006 : 0000;
		} else
		{
			new_owner_perms = file_owner_perms;
			new_group_perms = file_group_perms;
			new_other_perms = file_other_perms;
		}
		/* Find restricted group, if any */
		gtm_dist_gid = gtm_get_group_id(&dist_stat_buff);
		dir_mode = dist_stat_buff.st_mode;
		gtm_group_restricted = ((INVALID_GID != gtm_dist_gid) && !(dir_mode & 01)); /* not other executable */
		if ((this_uid_is_file_owner && this_uid_in_file_group) || this_uid_is_root)
		{
			if (this_uid_is_root)		/* otherwise, use default uid */
				*user_id = file_uid;
			*group_id = file_gid;		/* use file group */
			*perm = new_owner_perms | new_group_perms | new_other_perms;
		} else if (this_uid_is_file_owner)
		{	/* This uid has access via file owner membership but is not a member of the file group */
			assert(!this_uid_in_file_group);
			/* Since there could be userids that are members of the file group as well as the new group,
			 * one needs to keep the existing group permissions while setting up the new group permissions.
			 */
			new_group_perms = (new_group_perms | (new_other_perms << 3));
			if (gtm_group_restricted)
			{
				assert((WBTEST_HELPOUT_FAILGROUPCHECK == gtm_white_box_test_case_number)
						|| gtm_member_group_id(this_uid, gtm_dist_gid, pdd));
				*group_id = gtm_dist_gid;	/* use restricted group */
				*perm = new_owner_perms | new_group_perms;
			} else
			{
				/* No need to set *group_id. Use default group.
				 * But because file owner is not part of the file group, transfer group permissions of
				 * file to "other" when creating the new file so members of the file group (that are not
				 * part of the default group of the file owner) can still access the new file with same
				 * permissions as the source file. And transfer other permissions of file to "group" when
				 * creating the new file for similar reasons. Note that it is possible for userids to exist
				 * that are part of the file group and the default group of the file owner. For those userids,
				 * we need to ensure the group permissions of the new file stay same as source file. Therefore
				 * we need the new group and other permissions to be a bitwise OR of the old group and other.
				 */
				assert(file_owner_perms);
				new_other_perms = new_group_perms >> 3;
				*perm = new_owner_perms | new_group_perms | new_other_perms;
			}
		} else
		{	/* This uid has access via file group OR file other membership */
			/* Use default uid in all cases below */
			if ((this_uid_in_file_group && owner_in_file_group) || this_uid_is_root)
			{
				*group_id = file_gid;		/* use file group */
				assert(file_group_perms);
				new_owner_perms = new_group_perms << 3;
				*perm = new_owner_perms | new_group_perms | new_other_perms;
			} else if (gtm_group_restricted)
			{
				assert((WBTEST_HELPOUT_FAILGROUPCHECK == gtm_white_box_test_case_number)
						|| gtm_member_group_id(this_uid, gtm_dist_gid, pdd));
				*group_id = gtm_dist_gid;	/* use restricted group */
				new_group_perms = (new_group_perms | (new_other_perms << 3));
				/* "owner" gets group permissions if in group, otherwise "other" permissions */
				new_owner_perms = this_uid_in_file_group ? (new_group_perms << 3) : (new_other_perms << 6);
				*perm = new_owner_perms | new_group_perms;
			} else
			{
				if (this_uid_in_file_group)
				{
					*group_id = file_gid;		/* use file group */
					/* Since owner_in_file_group is FALSE, we need to make sure owner permissions
					 * are transferred to "other" in the new file.
					 */
					new_other_perms |= (new_owner_perms >> 6);
					new_owner_perms = new_group_perms << 3;
				} else
				{
					/* Use default group; Use 'other' permissions for owner */
					new_owner_perms = (new_other_perms << 6);
					new_group_perms = (new_group_perms | (new_other_perms << 3));
					new_other_perms = new_group_perms >> 3;
				}
				*perm = new_owner_perms | new_group_perms | new_other_perms;
			}
		}
	}
	if (target_type & PERM_EXEC)
		*perm |= ((*perm & 0444) >> 2);	/* Grab the read bits, shift them to the exec bit position, and add them back in. */
	assert(*perm);
	if (!(this_uid_is_root) && (!(*perm & 0600)
				|| (!(this_uid_is_file_owner) && !(this_uid_in_file_group) && !(0006 & st_mode))))
	{	/* Either permissions don't include owner or could not ascertain group membership */
		pdd->this_uid = this_uid;
		pdd->this_gid = this_gid;
		pdd->file_uid = file_uid;	/* get owning file uid */
		pdd->file_gid = file_gid;	/* get owning file gid */
		pdd->lib_gid  = gtm_dist_gid;
		SNPRINTF(pdd->file_perm, SIZEOF(pdd->file_perm), "%04o", st_mode & 07777);
		SNPRINTF(pdd->lib_perm, SIZEOF(pdd->lib_perm), "%04o", dir_mode  & 07777);
		if (gid_list_len > 0)
		{
			strtop = pdd->print_gid_list + MAX_PRINT_GID_LEN;
			strnow = pdd->print_gid_list;
			for (i = 0; (strnow < strtop) && (i < gid_list_len); i++)
				if (this_gid != gid_list[i]) /* Avoid printing the primary GID */
					strnow += SNPRINTF(strnow, (strtop - strnow), ",%d", (int) gid_list[i]);
			if (strnow >= strtop)
			{	/* It's too long, terminate with an ellipsis*/
				strnow = pdd->print_gid_list + MAX_PRINT_GID_LEN - GID_ELLIPSIS_LEN;
				SNPRINTF(strnow, GID_ELLIPSIS_LEN, GID_ELLIPSIS);
			}
			pdd->print_gid_list_len = strlen(pdd->print_gid_list);
			assert(pdd->print_gid_list_len < MAX_PRINT_GID_LEN);
		} else
			pdd->print_gid_list_len = 0;
		return FALSE;
	}
	return TRUE;
}
