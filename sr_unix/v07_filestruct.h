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

#ifndef dev_t
#include <sys/types.h>
#endif

#define GDS_LABEL "GDSDYNUNX02" /* This string must be of length GDS_LABEL_SZ */


typedef unix_file_id gd_id;       /* defined in gdsroot.h */

typedef struct unix_db_info_struct
{	sgmnt_addrs	s_addrs;
	char		*fn;
	int		fd;
	unix_file_id	fileid;
	int		sems;
	int		shmid;
        key_t           key;
	bool		raw;
} unix_db_info;

typedef struct unix_file_info_struct
{	int	file;
	int	fn_len;
	char	*fn;
} unix_file_info;

#define FILE_INFO(reg)	((unix_db_info *)(reg)->dyn.addr->file_cntl->file_info)

#define GDS_INFO unix_db_info

#define MM_ADDR(x) (x->master_map)
#if defined(__osf__) || defined(_AIX)
#define REG_EQUAL(fileinfo,reg) ((fileinfo)->fileid.inode == FILE_INFO(reg)->fileid.inode && (fileinfo)->fileid.device == FILE_INFO(reg)->fileid.device && (fileinfo)->fileid.st_gen == FILE_INFO(reg)->fileid.st_gen)
#else
#define REG_EQUAL(fileinfo,reg) ((fileinfo)->fileid.inode == FILE_INFO(reg)->fileid.inode && (fileinfo)->fileid.device == FILE_INFO(reg)->fileid.device)
#endif
