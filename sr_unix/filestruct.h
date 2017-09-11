/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/* Check O_DIRECT alignment requirements for each supported platform */
#define	DIO_ALIGNSIZE(udi)	((udi)->db_fs_block_size)

#define	BUFF_IS_DIO_ALIGNED(udi, buff)	(0 == (((UINTPTR_T)(buff)) % DIO_ALIGNSIZE(udi)))
#define	OFFSET_IS_DIO_ALIGNED(udi, off)	(0 == ((off) % DIO_ALIGNSIZE(udi)))
#define	SIZE_IS_DIO_ALIGNED(udi, size)	(0 == ((size) % DIO_ALIGNSIZE(udi)))

#define	DIO_BUFF_NO_OVERFLOW(DIO_BUFF, SIZE)										\
	(((UINTPTR_T)(DIO_BUFF).aligned + (SIZE)) <= ((UINTPTR_T)(DIO_BUFF).unaligned + (DIO_BUFF).unaligned_size))

/* DIO buffer alignment is needed in almost all cases of writing to the database file. The only exception is if
 * we are about to create the database file AND we should not have opened the fd with O_DIRECT. Take that into account.
 */
#define	ASSERT_NO_DIO_ALIGN_NEEDED(UDI)	assert(in_mu_cre_file && !(UDI)->fd_opened_with_o_direct)

#define	DBG_CHECK_DIO_ALIGNMENT(udi, offset, buff, size)							\
MBSTART {													\
	DEBUG_ONLY(DCL_THREADGBL_ACCESS);									\
														\
	DEBUG_ONLY(SETUP_THREADGBL_ACCESS);									\
	assert((NULL == (udi)) || !(udi)->fd_opened_with_o_direct || (NULL != (TREF(dio_buff)).aligned));	\
	assert((NULL == (udi)) || !(udi)->fd_opened_with_o_direct || OFFSET_IS_DIO_ALIGNED(udi, offset));	\
	assert((NULL == (udi)) || !(udi)->fd_opened_with_o_direct || BUFF_IS_DIO_ALIGNED(udi, buff));		\
	assert((NULL == (udi)) || !(udi)->fd_opened_with_o_direct || SIZE_IS_DIO_ALIGNED(udi, size));		\
	/* If we are using the global variable "dio_buff.aligned", then we better not be executing in timer	\
	 * code or in threaded code (as we have only ONE buffer to use). Assert that.				\
	 */													\
	assert(((TREF(dio_buff)).aligned != (char *)(buff)) || (!timer_in_handler && !multi_thread_in_use));	\
	/* Assert that we are not exceeding allocated buffer bounds in case of "dio_buff.aligned" */		\
	assert(((TREF(dio_buff)).aligned != (char *)(buff)) || DIO_BUFF_NO_OVERFLOW((TREF(dio_buff)), size));	\
} MBEND

typedef struct
{
	int	unaligned_size; /* size of allocated (and potentially) unaligned buffer */
	char	*unaligned;	/* pointer to start of allocated buffer */
	char	*aligned;	/* pointer to start of aligned buffer (i.e. aligned >= unaligned) */
} dio_buff_t;

#ifdef DEBUG
GBLREF	boolean_t		in_mu_cre_file;
GBLREF	volatile boolean_t	timer_in_handler;
GBLREF	boolean_t		multi_thread_in_use;
#endif

/* Returns a buffer that has space to hold at least SIZE bytes where the buffer start is OS_PAGE_SIZE aligned. */
#define	DIO_BUFF_EXPAND_IF_NEEDED(UDI, SIZE, DIO_BUFF)							\
MBSTART {												\
	assert(UDI->fd_opened_with_o_direct);								\
	assert(DIO_ALIGNSIZE(UDI));									\
	DIO_BUFF_EXPAND_IF_NEEDED_NO_UDI(SIZE, DIO_ALIGNSIZE(UDI), DIO_BUFF);				\
} MBEND

#define	DIO_BUFF_EXPAND_IF_NEEDED_NO_UDI(SIZE, ALIGNSIZE, DIO_BUFF)					\
MBSTART {												\
	int	needed;											\
													\
	needed = ROUND_UP2(SIZE, ALIGNSIZE) + OS_PAGE_SIZE;						\
	if (needed > (DIO_BUFF)->unaligned_size)							\
	{												\
		if (NULL != (DIO_BUFF)->unaligned)							\
		{											\
			free((DIO_BUFF)->unaligned);							\
			(DIO_BUFF)->unaligned = NULL;							\
		}											\
		(DIO_BUFF)->unaligned = (char *)malloc(needed);						\
		(DIO_BUFF)->aligned = (char *)ROUND_UP2((UINTPTR_T)(DIO_BUFF)->unaligned, OS_PAGE_SIZE);\
		(DIO_BUFF)->unaligned_size = needed;							\
	}												\
} MBEND

typedef struct unix_db_info_struct
{
	sgmnt_addrs	s_addrs;
	char		*fn;
	int		fd;
	gd_addr		*owning_gd;	/* Used by the linux kernel interface for AIO. As long as owning_gd
					 * is NULL we can assume no writes to this database file on disk
					 * have happened yet.
					 */
	gd_id		fileid;
	int		semid;
	time_t		gt_sem_ctime;
	int		shmid;
	time_t		gt_shm_ctime;
	int		ftok_semid;
        key_t           key;
	boolean_t	raw;
	uint4		db_fs_block_size;
	unsigned int	shm_created : 1;
	unsigned int	shm_deleted : 1;
	unsigned int	sem_created : 1;
	unsigned int	sem_deleted : 1;
	unsigned int	grabbed_ftok_sem : 1;
	unsigned int	grabbed_access_sem : 1;
	unsigned int	counter_acc_incremented : 1;
	unsigned int	counter_ftok_incremented : 1;
	unsigned int	fd_opened_with_o_direct : 1;	/* If dbfilop(FC_OPEN) happened with O_DIRECT */
} unix_db_info;

typedef struct unix_file_info_struct
{
	int	file;
	int	fn_len;
	char	*fn;
} unix_file_info;

#define	FC2UDI(FILE_CNTL)	((unix_db_info *)(FILE_CNTL->file_info))
#define	FILE_CNTL(reg)		(reg)->dyn.addr->file_cntl
#define	FILE_INFO(reg)		((unix_db_info *)(reg)->dyn.addr->file_cntl->file_info)
#define	FILE_ID(reg)		((unix_db_info *)(reg)->dyn.addr->file_cntl->file_info)->fileid
#define	GDS_INFO		unix_db_info
#define	FI_FN(file_info)	((unix_file_info *)(file_info))->fn
#define	FI_FN_LEN(file_info)	((unix_file_info *)(file_info))->fn_len

#if defined(__osf__) || defined(_AIX)
#define REG_EQUAL(fileinfo,reg) ((fileinfo)->fileid.inode == FILE_INFO(reg)->fileid.inode		\
					&& (fileinfo)->fileid.device == FILE_INFO(reg)->fileid.device	\
					&& (fileinfo)->fileid.st_gen == FILE_INFO(reg)->fileid.st_gen)
#else
#define REG_EQUAL(fileinfo,reg) ((fileinfo)->fileid.inode == FILE_INFO(reg)->fileid.inode		\
					&& (fileinfo)->fileid.device == FILE_INFO(reg)->fileid.device)
#endif

#define CSD2UDI(CSD, UDI)				\
MBSTART {						\
	UDI->shmid = CSD->shmid;			\
	UDI->semid = CSD->semid;			\
	UDI->gt_sem_ctime = CSD->gt_sem_ctime.ctime;	\
	UDI->gt_shm_ctime = CSD->gt_shm_ctime.ctime;	\
} MBEND

#define UDI2CSD(UDI, CSD)				\
MBSTART {						\
	CSD->shmid = UDI->shmid;			\
	CSD->semid = UDI->semid;			\
	CSD->gt_sem_ctime.ctime = UDI->gt_sem_ctime;	\
	CSD->gt_shm_ctime.ctime = UDI->gt_shm_ctime;	\
} MBEND
