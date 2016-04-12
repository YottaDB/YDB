/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gdsdbver.h"

#define GDS_LABEL_GENERIC 	"GDSDYNUNX"
#define GDS_LABEL 		GDS_LABEL_GENERIC GDS_CURR	/* This string must be of length GDS_LABEL_SZ */
#define GDS_RPL_LABEL 		"GDSRPLUNX04" 	/* format of journal pool and receive pool (must be of length GDS_LABEL_SZ) */

typedef struct unix_db_info_struct
{
	sgmnt_addrs	s_addrs;
	char		*fn;
	int		fd;
	gd_id		fileid;
	int		semid;
	time_t		gt_sem_ctime;
	int		shmid;
	time_t		gt_shm_ctime;
	int		ftok_semid;
	boolean_t	new_shm;
	boolean_t	new_sem;
	boolean_t	grabbed_ftok_sem;
	boolean_t	grabbed_access_sem;
	boolean_t	counter_acc_incremented;
	boolean_t	counter_ftok_incremented;
        key_t           key;
	bool		raw;
} unix_db_info;

typedef struct unix_file_info_struct
{
	int	file;
	int	fn_len;
	char	*fn;
} unix_file_info;

#define FILE_INFO(reg)	((unix_db_info *)(reg)->dyn.addr->file_cntl->file_info)
#define FILE_ID(reg)	((unix_db_info *)(reg)->dyn.addr->file_cntl->file_info)->fileid
#define GDS_INFO unix_db_info
#define FI_FN(file_info)	((unix_file_info *)(file_info))->fn
#define FI_FN_LEN(file_info)	((unix_file_info *)(file_info))->fn_len

#if defined(__osf__) || defined(_AIX)
#define REG_EQUAL(fileinfo,reg) ((fileinfo)->fileid.inode == FILE_INFO(reg)->fileid.inode		\
					&& (fileinfo)->fileid.device == FILE_INFO(reg)->fileid.device	\
					&& (fileinfo)->fileid.st_gen == FILE_INFO(reg)->fileid.st_gen)
#else
#define REG_EQUAL(fileinfo,reg) ((fileinfo)->fileid.inode == FILE_INFO(reg)->fileid.inode		\
					&& (fileinfo)->fileid.device == FILE_INFO(reg)->fileid.device)
#endif

#define WRT_STRT_PNDNG (unsigned short)65534 /* the code assumes this is non-zero, even,
					and that VMS never uses its value for iosb.cond */

#define CSD2UDI(CSD, UDI)				\
{							\
	UDI->shmid = CSD->shmid;			\
	UDI->semid = CSD->semid;			\
	UDI->gt_sem_ctime = CSD->gt_sem_ctime.ctime;	\
	UDI->gt_shm_ctime = CSD->gt_shm_ctime.ctime;	\
}

#define UDI2CSD(UDI, CSD)				\
{							\
	CSD->shmid = UDI->shmid;			\
	CSD->semid = UDI->semid;			\
	CSD->gt_sem_ctime.ctime = UDI->gt_sem_ctime;	\
	CSD->gt_shm_ctime.ctime = UDI->gt_shm_ctime;	\
}
