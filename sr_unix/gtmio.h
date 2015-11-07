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

/* Define macros to do our IO and restart as appropriate
 *
 * IOCTL	Loop until ioctl call succeeds or fails with other than EINTR.
 * OPENFILE	Loop until open succeeds or fails with other than EINTR.
 * OPENFILE_SYNC
 *		Loop until open succeeds or fails with other than EINTR.
 *		Opens with O_DSYNC/O_SYNC in direct mode where possible.
 *		Else opens only with O_DSYNC or O_SYNC.
 * OPEN_OBJECT_FILE
 *		Opens the object file and waits till it gets a lock(shr/excl).
 *		Sets default perms if it creates a new file.
 * CLOSE_OBJECT_FILE - close the object file after releasing the lock on it.
 * CLOSEFILE	Loop until close succeeds for fails with other than EINTR.
 * CLOSEFILE_RESET
 * 		Loop until close succeeds for fails with other than EINTR.
 * 		At end reset channel to FD_INVALID unconditionally (even if close was not successful).
 * CONVERT_OBJECT_LOCK - convert type of lock held on object file
 * LSEEKREAD	Performs either pread() or an lseek()/ read() combination. In
 *		the latter case, sets global variable to warn off async IO routines.
 * LSEEKREAD_AVAILABLE	Same as LSEEKREAD except it has an extra parameter where the number of bytes
 *			ACTUALLY READ are stored irrespective of whether all REQUESTED BYTES were read or not.
 * LSEEKWRITE	Same as LSEEKREAD but for WRITE.
 * DOREADRC	Performs read, returns code 0 if okay, otherwise returns errno.
 * DOREADRL	Performs read but returns length read or -1 if errno is set.
 * DOREADRLTO2	Same as DOREADRL but has a timeout flag to poll on interrupts.
 * DOWRITE	Performs write with no error checking/return.
 * DOWRITERC	Performs write, returns code 0 if okay, otherwise returns errno.
 * DOWRITERL	Performs write but returns length written or -1 if errno is set.
 * WRITEPIPE	Performs write to FIFO/pipe but individual writes (within loop) limited to system pipe buffer size
 */

#ifndef GTMIO_Included
#define GTMIO_Included

#ifndef GTMIO_MINIMAL		/* Avoid pulling in includes that make gtm_icu.c uncompilable */
# include <sys/types.h>
# include "gtm_stat.h"
# include "gtm_unistd.h"
# include "gtm_fcntl.h"
# include "eintr_wrappers.h"
# include "min_max.h"
# include "wbox_test_init.h"
#endif

#ifdef __linux__
#include <sys/vfs.h>
#endif

error_def(ERR_PREMATEOF);

#ifdef KEEP_zOS_EBCDIC
#define DOWRITE_A	__write_a
#define DOREAD_A	__read_a
#define	DOWRITERL_A	#error need to create as part of z/OS port and make __write_a return status and good errno
#else
#define DOWRITE_A	DOWRITE
#define DOREAD_A	read
#define	DOWRITERL_A	DOWRITERL
#endif

#define MAX_FILE_OPEN_TRIES	20  /* polling count */
#define WAIT_FOR_FILE_TIME	100 /* msec */
#define WAIT_FOR_BLOCK_TIME	100 /* msec */

#define IOCTL(FDESC, REQUEST, ARG, RC)			\
{							\
	do						\
	{						\
		RC = ioctl(FDESC, REQUEST, ARG);	\
	} while(-1 == RC && EINTR == errno);		\
	if (-1 != RC)					\
		RC = 0;					\
	else						\
		RC = errno;				\
}

#define OPENFILE(FNAME, FFLAGS, FDESC)			\
{							\
	do						\
	{						\
		FDESC = OPEN(FNAME, FFLAGS);		\
	} while(-1 == FDESC && EINTR == errno);		\
}

#define OPENFILE3(FNAME, FFLAGS, FMODE, FDESC)		\
{							\
	do						\
	{						\
		FDESC = OPEN3(FNAME, FFLAGS, FMODE);	\
	} while(-1 == FDESC && EINTR == errno);		\
}

/* OPENFILE4 not needed - io_open_try handles interrupts */

#define FSTYPE_ADVFS	"advfs"
#define FSTYPE_UFS	"ufs"

#if defined(_AIX)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_DIRECT | O_DSYNC, FDESC);
/* Note: putting the DIRECTIO_FLAG definition before the #if and then redefining it below for the two exceptions
 * causes a warning on sparc - "macro redefined: DIRECTIO_FLAG" so it is repeated in each part of the big #define
 */
#define DIRECTIO_FLAG	O_DIRECT
#elif defined(__osf__)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)										\
{															\
	struct statvfs	statvfs_buf;											\
	int		macro_errno;											\
	boolean_t	dio_success = TRUE;										\
	if (-1 == statvfs(FNAME, &statvfs_buf))										\
	{														\
		macro_errno = errno;											\
		util_out_print("Error finding FS type for file, !AD :!AD", OPER,					\
				LEN_AND_STR(FNAME), LEN_AND_STR(STRERROR(macro_errno)));				\
		dio_success = FALSE;											\
	} else if (strcmp(FSTYPE_ADVFS, statvfs_buf.f_basetype))							\
			dio_success = FALSE;										\
	if (dio_success)												\
	{														\
		OPENFILE(FNAME, FFLAGS | O_DIRECTIO | O_DSYNC, FDESC);							\
	} else														\
	{														\
		OPENFILE(FNAME, FFLAGS | O_DSYNC, FDESC);								\
	}														\
}
#define DIRECTIO_FLAG	O_DIRECTIO
#elif defined(__sparc)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)										\
{															\
	struct statvfs	statvfs_buf;											\
	int		macro_errno;											\
	boolean_t	dio_success = TRUE;										\
	if (-1 == statvfs(FNAME, &statvfs_buf))										\
	{														\
		macro_errno = errno;											\
		util_out_print("Error finding FS type for file, !AD. !AD", OPER,					\
				LEN_AND_STR(FNAME), LEN_AND_STR(STRERROR(macro_errno)));				\
		dio_success = FALSE;											\
	} else if (strcmp(FSTYPE_UFS, statvfs_buf.f_basetype))								\
			dio_success = FALSE;										\
	OPENFILE(FNAME, FFLAGS | O_DSYNC, FDESC);									\
	if (dio_success && (FD_INVALID != FDESC))									\
	{														\
		if (-1 == directio(FDESC, DIRECTIO_ON))									\
		{													\
			macro_errno = errno;										\
			util_out_print("Failed to set DIRECT IO option for !AD, reverting to normal IO. !AD ",		\
					OPER, LEN_AND_STR(FNAME), LEN_AND_STR(STRERROR(macro_errno)));			\
		}													\
	}														\
}
#define DIRECTIO_FLAG	0
#elif defined(__MVS__)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_SYNC, FDESC);
#define DIRECTIO_FLAG	0
#elif defined(__linux__)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_DIRECT | O_DSYNC, FDESC);
#define DIRECTIO_FLAG	O_DIRECT
#elif defined(__hpux)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_DIRECT | O_DSYNC, FDESC);
#define DIRECTIO_FLAG	O_DIRECT
#else
#error UNSUPPORTED PLATFORM
#endif

#if defined( __linux__)
/* A special handling was needed for linux due to its inability to lock
 * over NFS.  The only difference in code is an added check for NFS file
 * thru fstatfs
 *
 * This should ideally include <linux/nfs_fs.h> for NFS_SUPER_MAGIC.
 * However, this header file doesn't seem to be standard and gives lots of
 * compilation errors and hence defining again here.  The constant value
 * seems to be portable across all linuxes (courtesy 'statfs' man pages)
 */
#define NFS_SUPER_MAGIC 0x6969
#define LOCK_IS_ALLOWED(FDESC, STATUS)								\
{												\
	struct statfs buf;									\
	STATUS = ((-1 != fstatfs(FDESC, &buf)) && (NFS_SUPER_MAGIC != buf.f_type)) ? 0 : -2;	\
}
#else
#define LOCK_IS_ALLOWED(FDESC, STATUS)	STATUS = 0
#endif
/* The for loop is the workaround for a glitch in read locking in zlink.  The primary steps to acquire a
 * read-lock are 1. open the file, and 2. read lock it.	 If a process creates the initial, empty version
 * of the file (with the OPEN3), but has not yet write-locked it, and meanwhile, another process does its
 * open and gets a read-lock, then a later read within incr_link() will end up reading an empty file. To
 * avoid that problem, readers have to poll for a non-empty object file before reading.	 If the read lock
 * is obtained, but the file is empty, then release the read lock, sleep for a while, and retry the file open.
 */
#define OPEN_OBJECT_FILE(FNAME, FFLAG, FDESC)									\
{														\
	int		status;											\
	struct flock	lock;	 /* arg to lock the file thru fnctl */						\
	int		cntr;											\
	struct stat	stat_buf;										\
	pid_t		l_pid;											\
	ZOS_ONLY(int	realfiletag;)										\
														\
	l_pid = getpid();											\
	for (cntr = 0; cntr < MAX_FILE_OPEN_TRIES; cntr++)							\
	{													\
		while (FD_INVALID == (FDESC = OPEN3(FNAME, FFLAG, 0666)) && EINTR == errno)			\
			;											\
		if (-1 != FDESC)										\
		{												\
			LOCK_IS_ALLOWED(FDESC, status);								\
			if (-2 != status)									\
			{											\
				do {										\
					lock.l_type = ((O_WRONLY == ((FFLAG) & O_ACCMODE)) ||			\
						( O_RDWR == ((FFLAG) & O_ACCMODE))) ? F_WRLCK : F_RDLCK;	\
					lock.l_whence = SEEK_SET;	/*locking offsets from file beginning*/	\
					lock.l_start = lock.l_len = 0;	/* lock the whole file */		\
					lock.l_pid = l_pid;							\
				} while (-1 == (status = fcntl(FDESC, F_SETLKW, &lock)) && EINTR == errno);	\
			}											\
			if (-1 != status)									\
			{											\
				if ((FFLAG) & O_CREAT)								\
				{										\
					FTRUNCATE(FDESC, 0, status);						\
					ZOS_ONLY(								\
						status = gtm_zos_set_tag(FDESC, TAG_BINARY, TAG_NOTTEXT, TAG_FORCE, &realfiletag); \
					)									\
				} else										\
				{										\
					FSTAT_FILE(FDESC, &stat_buf, status);					\
					if (status || (0 == stat_buf.st_size))					\
					{									\
						CLOSE_OBJECT_FILE(FDESC, status);				\
						SHORT_SLEEP(WAIT_FOR_FILE_TIME);				\
						continue;							\
					}									\
					ZOS_ONLY(								\
						status = gtm_zos_tag_to_policy(FDESC, TAG_BINARY, &realfiletag);	\
					)									\
				}										\
			}											\
			if (-1 == status)									\
				CLOSEFILE_RESET(FDESC, status);/* can't fail - no writes, no writes-behind */ 	\
		}												\
		break;												\
	}													\
}
#define CONVERT_OBJECT_LOCK(FDESC, FFLAG, RC)						\
{											\
	struct flock	lock;	 /* arg to lock the file thru fnctl */			\
	pid_t		l_pid;								\
											\
	l_pid = getpid();								\
	do {										\
		lock.l_type = FFLAG;							\
		lock.l_whence = SEEK_SET;	/*locking offsets from file beginning*/	\
		lock.l_start = lock.l_len = 0;	/* lock the whole file */		\
		lock.l_pid = l_pid;							\
	} while (-1 == (RC = fcntl(FDESC, F_SETLKW, &lock)) && EINTR == errno);		\
}

#define CLOSE_OBJECT_FILE(FDESC, RC)						\
{										\
	struct flock lock;    /* arg to unlock the file thru fnctl */		\
	do {									\
		lock.l_type = F_UNLCK;						\
		lock.l_whence = SEEK_SET;					\
		lock.l_start = lock.l_len = 0; /* unlock the whole file */	\
		lock.l_pid = getpid();						\
	} while (-1 == (RC = fcntl(FDESC, F_SETLK, &lock)) && EINTR == errno);	\
	CLOSEFILE_RESET(FDESC, RC);						\
}

#define CLOSEFILE(FDESC, RC)					\
{								\
	do							\
	{							\
		RC = close(FDESC);				\
	} while(-1 == RC && EINTR == errno);			\
	if (-1 == RC)	/* Had legitimate error - return it */	\
		RC = errno;					\
}

#define	CLOSEFILE_RESET(FDESC, RC)	\
{					\
	CLOSEFILE(FDESC, RC)		\
	FDESC = FD_INVALID;		\
}

/* Close file only if we have it open. Use FCNTL to check if we have it open */
#define CLOSEFILE_IF_OPEN(FDESC, RC)							\
{											\
	int	flags;									\
											\
	FCNTL2(FDESC, F_GETFL, flags);							\
	if ((-1 != flags) || (EBADF != errno))						\
		CLOSEFILE(FDESC, RC);	/* file is a valid descriptor. Close it */	\
}

#if defined(__osf__) || defined(_AIX) || defined(__sparc) || defined(__linux__) || defined(__hpux) || \
	defined(__CYGWIN__) || defined(__MVS__)
/* These platforms are known to support pread/pwrite.
 * !!!!!!!!!!!!!! Note !!!!!!!!!!!!!!
 * pread and pwrite do NOT (on most platforms) set the file pointer like lseek/read/write would,
 * so they are NOT a drop-in replacement !!
 */

#define NOPIO_ONLY(X)

#define GET_LSEEK_FLAG(FDESC, VAL)

/* If definitions are flying around for pread/pread64, don't override them. Otherwise on HPUX
   we need to run the 64bit versions of these calls because the POSIX version linkage to the
   64 bit variation is broken. SE 07/2005.
*/
#if (defined(__hpux) && !defined(__ia64))
# if !defined(pread) && !defined(pread64)
#  define pread pread64
#  define pwrite pwrite64
# else
#  error "** Interference with pread/pwrite defines - HPUX may have fixed their problem **"
# endif
#endif

#define LSEEKREAD(FDESC, FPTR, FBUFF, FBUFF_LEN, RC)						\
{												\
	ssize_t			gtmioStatus;							\
	size_t			gtmioBuffLen;							\
	off_t			gtmioPtr;							\
	sm_uc_ptr_t		gtmioBuff;							\
	gtmioBuffLen = FBUFF_LEN;								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	gtmioPtr = (off_t)(FPTR);								\
	for (;;)										\
	{											\
		if (-1 != (gtmioStatus = pread(FDESC, gtmioBuff, gtmioBuffLen, gtmioPtr)))	\
		{										\
			gtmioBuffLen -= gtmioStatus;						\
			if (0 == gtmioBuffLen || 0 == gtmioStatus)				\
				break;								\
			gtmioBuff += gtmioStatus;						\
			gtmioPtr += gtmioStatus;						\
			continue;								\
		}										\
		if (EINTR != errno)								\
			break;									\
	}											\
	if (0 == gtmioBuffLen)									\
		RC = 0;										\
	else if (-1 == gtmioStatus)	/* Had legitimate error - return it */			\
		RC = errno;									\
	else											\
		RC = -1;		/* Something kept us from reading what we wanted */	\
}

/* The below macro is almost the same as LSEEKREAD except it has an extra parameter where the number of
 * bytes ACTUALLY READ are stored irrespective of whether all REQUESTED BYTES were read or not.
 */
#define LSEEKREAD_AVAILABLE(FDESC, FPTR, FBUFF, FBUFF_LEN, ACTUAL_READLEN, RC)			\
{												\
	ssize_t			gtmioStatus;							\
	size_t			gtmioBuffLen;							\
	off_t			gtmioPtr;							\
	sm_uc_ptr_t		gtmioBuff;							\
												\
	gtmioBuffLen = (FBUFF_LEN);								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	gtmioPtr = (off_t)(FPTR);								\
	for (;;)										\
	{											\
		if (-1 != (gtmioStatus = pread(FDESC, gtmioBuff, gtmioBuffLen, gtmioPtr)))	\
		{										\
			gtmioBuffLen -= gtmioStatus;						\
			if (0 == gtmioBuffLen || 0 == gtmioStatus)				\
				break;								\
			gtmioBuff += gtmioStatus;						\
			gtmioPtr += gtmioStatus;						\
			continue;								\
		}										\
		if (EINTR != errno)								\
			break;									\
	}											\
	(ACTUAL_READLEN) = (FBUFF_LEN) - gtmioBuffLen;						\
	if (0 == gtmioBuffLen)									\
		RC = 0;										\
	else if (-1 == gtmioStatus)	/* Had legitimate error - return it */			\
		RC = errno;									\
	else											\
		RC = -1;		/* Something kept us from reading what we wanted */	\
}

#define LSEEKWRITE(FDESC, FPTR, FBUFF, FBUFF_LEN, RC)						\
{												\
	ssize_t			gtmioStatus;							\
	size_t			gtmioBuffLen;							\
	off_t			gtmioPtr;							\
	sm_uc_ptr_t		gtmioBuff;							\
	gtmioBuffLen = FBUFF_LEN;								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	gtmioPtr = (off_t)(FPTR);								\
	for (;;)										\
	{											\
		if (-1 != (gtmioStatus = pwrite(FDESC, gtmioBuff, gtmioBuffLen, gtmioPtr)))	\
		{										\
			gtmioBuffLen -= gtmioStatus;						\
			if (0 == gtmioBuffLen)							\
				break;								\
			gtmioBuff += gtmioStatus;						\
			gtmioPtr += gtmioStatus;						\
			continue;								\
		}										\
		if (EINTR != errno)								\
			break;									\
	}											\
	if (0 == gtmioBuffLen)									\
		RC = 0;										\
	else if (-1 == gtmioStatus)	/* Had legitimate error - return it */			\
		RC = errno;									\
	else											\
		RC = -1;		/* Something kept us from writing what we wanted */	\
}

#else /* real lseek and read/write - still need to protect against interrupts inbetween calls */
/* Using lseek/read/write path instead of faster pread/pwrite path */

#ifndef __MVS__
#warning "Using lseek/read/write path instead of faster pread/pwrite path"
#endif

#define NOPIO_ONLY(X) X

/* Note array is not initialized but first IO to a given file descriptor will initialize that element */
#define GET_LSEEK_FLAGS_ARRAY									\
{												\
	GBLREF	boolean_t	*lseekIoInProgress_flags;					\
	int4	sc_open_max;									\
	if ((boolean_t *)0 == lseekIoInProgress_flags)						\
	{											\
		SYSCONF(_SC_OPEN_MAX, sc_open_max);						\
		lseekIoInProgress_flags = (boolean_t *)malloc(sc_open_max * SIZEOF(boolean_t));	\
	}											\
}

#define GET_LSEEK_FLAG(FDESC, VAL)				\
{								\
	GBLREF	boolean_t	*lseekIoInProgress_flags;	\
	GET_LSEEK_FLAGS_ARRAY;					\
	VAL = lseekIoInProgress_flags[(FDESC)];			\
}

#define SET_LSEEK_FLAG(FDESC, VAL)				\
{								\
	GBLREF	boolean_t	*lseekIoInProgress_flags;	\
	GET_LSEEK_FLAGS_ARRAY;					\
	lseekIoInProgress_flags[(FDESC)] = VAL;			\
}

#define LSEEKREAD(FDESC, FPTR, FBUFF, FBUFF_LEN, RC)					\
{											\
	GBLREF boolean_t	*lseekIoInProgress_flags;				\
	ssize_t			gtmioStatus;						\
	size_t			gtmioBuffLen;						\
	off_t			gtmioPtr;						\
	sm_uc_ptr_t		gtmioBuff;						\
	SET_LSEEK_FLAG(FDESC, TRUE);							\
	gtmioBuffLen = FBUFF_LEN;							\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);						\
	gtmioPtr = (off_t)(FPTR);							\
	for (;;)									\
	{										\
		if (-1 != (gtmioStatus = (ssize_t)lseek(FDESC, gtmioPtr, SEEK_SET)))	\
		{									\
			if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen)))	\
			{								\
				gtmioBuffLen -= gtmioStatus;				\
				if (0 == gtmioBuffLen || 0 == gtmioStatus)		\
					break;						\
				gtmioBuff += gtmioStatus;				\
				gtmioPtr += gtmioStatus;				\
				continue;						\
			}								\
		}									\
		if (EINTR != errno)							\
			break;								\
	}										\
	if (0 == gtmioBuffLen)								\
		RC = 0;									\
	else if (-1 == gtmioStatus)	/* Had legitimate error - return it */		\
		RC = errno;								\
	else										\
		RC = -1;		/* Something kept us from reading what we wanted */	\
	SET_LSEEK_FLAG(FDESC, FALSE);	/* Reason this is last is so max optimization occurs */	\
}

/* The below macro is almost the same as LSEEKREAD except it has an extra parameter where the number of
 * bytes ACTUALLY READ are stored irrespective of whether all REQUESTED BYTES were read or not.
 */
#define LSEEKREAD_AVAILABLE(FDESC, FPTR, FBUFF, FBUFF_LEN, ACTUAL_READLEN, RC)			\
{												\
	GBLREF boolean_t	*lseekIoInProgress_flags;					\
	ssize_t			gtmioStatus;							\
	size_t			gtmioBuffLen;							\
	off_t			gtmioPtr;							\
	sm_uc_ptr_t		gtmioBuff;							\
												\
	SET_LSEEK_FLAG(FDESC, TRUE);								\
	gtmioBuffLen = FBUFF_LEN;								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	gtmioPtr = (off_t)(FPTR);								\
	for (;;)										\
	{											\
		if (-1 != (gtmioStatus = (ssize_t)lseek(FDESC, gtmioPtr, SEEK_SET)))		\
		{										\
			if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen)))		\
			{									\
				gtmioBuffLen -= gtmioStatus;					\
				if (0 == gtmioBuffLen || 0 == gtmioStatus)			\
					break;							\
				gtmioBuff += gtmioStatus;					\
				gtmioPtr += gtmioStatus;					\
				continue;							\
			}									\
		}										\
		if (EINTR != errno)								\
			break;									\
	}											\
	(ACTUAL_READLEN) = (FBUFF_LEN) - gtmioBuffLen;						\
	if (0 == gtmioBuffLen)									\
		RC = 0;										\
	else if (-1 == gtmioStatus)	/* Had legitimate error - return it */			\
		RC = errno;									\
	else											\
		RC = -1;		/* Something kept us from reading what we wanted */	\
	SET_LSEEK_FLAG(FDESC, FALSE);	/* Reason this is last is so max optimization occurs */	\
}

#define LSEEKWRITE(FDESC, FPTR, FBUFF, FBUFF_LEN, RC) 						\
{												\
	GBLREF boolean_t	*lseekIoInProgress_flags;					\
	ssize_t			gtmioStatus;							\
	size_t			gtmioBuffLen;							\
	off_t			gtmioPtr;							\
	sm_uc_ptr_t		gtmioBuff;							\
	SET_LSEEK_FLAG(FDESC, TRUE);								\
	gtmioBuffLen = FBUFF_LEN;								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	gtmioPtr = (off_t)(FPTR);								\
	for (;;)										\
	{											\
		if (-1 != (gtmioStatus = (ssize_t)lseek(FDESC, gtmioPtr, SEEK_SET)))		\
		{										\
			if (-1 != (gtmioStatus = write(FDESC, gtmioBuff, gtmioBuffLen)))	\
			{									\
				gtmioBuffLen -= gtmioStatus;					\
				if (0 == gtmioBuffLen)						\
					break;							\
				gtmioBuff += gtmioStatus;					\
				gtmioPtr += gtmioStatus;					\
				continue;							\
			}									\
		}										\
		if (EINTR != errno)								\
			break;									\
	}											\
	if (0 == gtmioBuffLen)									\
		RC = 0;										\
	else if (-1 == gtmioStatus)	/* Had legitimate error - return it */			\
		RC = errno;									\
	else											\
		RC = -1;		/* Something kept us from writing what we wanted */	\
	SET_LSEEK_FLAG(FDESC, FALSE);	/* Reason this is last is so max optimization occurs */	\
}
#endif /* if old lseekread/writes */

#define DOREADRC(FDESC, FBUFF, FBUFF_LEN, RC)							\
{												\
	ssize_t		gtmioStatus;								\
	size_t		gtmioBuffLen;								\
	sm_uc_ptr_t	gtmioBuff;								\
	gtmioBuffLen = FBUFF_LEN;								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	for (;;)										\
	{											\
		if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen)))			\
		{										\
			gtmioBuffLen -= gtmioStatus;						\
			if (0 == gtmioBuffLen || 0 == gtmioStatus)				\
				break;								\
			gtmioBuff += gtmioStatus;						\
		}										\
		else if (EINTR != errno)							\
			break;									\
	}											\
	if (-1 == gtmioStatus)		/* Had legitimate error - return it */			\
		RC = errno;									\
	else if (0 == gtmioBuffLen)								\
		RC = 0;										\
	else											\
		RC = -1;		/* Something kept us from reading what we wanted */	\
}

#define DOREADRL(FDESC, FBUFF, FBUFF_LEN, RLEN)							\
{												\
	ssize_t		gtmioStatus;								\
	size_t		gtmioBuffLen;								\
	sm_uc_ptr_t	gtmioBuff;								\
	gtmioBuffLen = FBUFF_LEN;								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	for (;;)										\
	{											\
		if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen)))			\
		{										\
			gtmioBuffLen -= gtmioStatus;						\
			if (0 == gtmioBuffLen || 0 == gtmioStatus)				\
				break;								\
			gtmioBuff += gtmioStatus;						\
		}										\
		else if (EINTR != errno)							\
		  break;									\
	}											\
	if (-1 != gtmioStatus)									\
		RLEN = (int)(FBUFF_LEN - gtmioBuffLen); /* Return length actually read */	\
	else						/* Had legitimate error - return it */	\
		RLEN = -1;									\
}

#define DOREADRLTO2(FDESC, FBUFF, FBUFF_LEN, TOFLAG, BLOCKED_IN, ISPIPE, FLAGS, RLEN,				\
		    TOT_BYTES_READ, TIMER_ID, MSEC_TIMEOUT, PIPE_ZERO_TIMEOUT, UTF_VAR_PF, PIPE_OR_FIFO)	\
{														\
	ssize_t		gtmioStatus;										\
	int		skip_read = FALSE;									\
	int		tfcntl_res;										\
	size_t		gtmioBuffLen;										\
	sm_uc_ptr_t	gtmioBuff;										\
	gtmioBuffLen =	(size_t)FBUFF_LEN;									\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);									\
	for (;;)												\
	{													\
		/* if it is a read x:0 on a pipe and it is not blocked (always the case when starting a read)	\
		   then try and read one char.	If it succeeds then turn on blocked io and read the rest	\
		   of the line.*/										\
		if (ISPIPE && (FALSE == *BLOCKED_IN))								\
		{												\
			for (;;)										\
			{											\
				if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, 1)))				\
				{										\
					if (0 == gtmioStatus) /* end of file */					\
					{									\
						skip_read = TRUE;						\
						break;								\
					}									\
					FCNTL3(FDESC, F_SETFL, FLAGS, tfcntl_res);				\
					if (0 > tfcntl_res)							\
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 		\
							5, LEN_AND_LIT("fcntl"), CALLFROM, errno);		\
					*BLOCKED_IN = TRUE;							\
					if (PIPE_ZERO_TIMEOUT)							\
					{									\
						TOFLAG = FALSE;							\
						/* Set a timer for 1 sec so atomic read x:0 will still work on	\
						   loaded systems but timeout on incomplete reads.  		\
						   Any characters						\
						   read to this point will be returned. */ 			\
						*MSEC_TIMEOUT = timeout2msec(1);				\
						start_timer(TIMER_ID, *MSEC_TIMEOUT, wake_alarm, 0, NULL);	\
					}									\
					gtmioBuffLen -= gtmioStatus;						\
					gtmioBuff += gtmioStatus;						\
					/* if only asked to read 1 character then skip additional read */	\
					if (0 == gtmioBuffLen)							\
						skip_read = TRUE;						\
					break;									\
				} else if (EINTR != errno || TOFLAG)						\
				{										\
					skip_read = TRUE;							\
					break;									\
				}										\
			}											\
		}												\
		/* if we didn't read 1 character or it's an error don't read anymore now */			\
		if (TRUE == skip_read) break;									\
		if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen)))					\
		{												\
			gtmioBuffLen -= gtmioStatus;								\
			if (0 == gtmioBuffLen || 0 == gtmioStatus)						\
				break;										\
			gtmioBuff += gtmioStatus;								\
		/* If it is pipe or fifo, read data that is currently available. If pipe contains data less than 		\
		   the CHUNK_SIZE, no need to read once again since it is in BLOCKING mode, in which case it will return -1.	\
		   So in the first read itself (after a successful read) break from the infinite loop. This variable is TRUE    \
		   if DOREADRLTO2 macro is called for a CHUNK_SIZE read. In other places (eg: iorm_get) it will be FALSE.	\
		   */											 	\
			if (UTF_VAR_PF)										\
				break;										\
		} else if (EINTR != errno || TOFLAG)								\
			break;											\
		if (PIPE_OR_FIFO && outofband)									\
			break;											\
	}													\
	if (-1 != gtmioStatus)											\
		RLEN = (int)(FBUFF_LEN - gtmioBuffLen);		/* Return length actually read */		\
	else						/* Had legitimate error - return it */			\
	{													\
		/* Store the number of bytes read in this invocation before we error out */			\
		*TOT_BYTES_READ = (int)(FBUFF_LEN - gtmioBuffLen);						\
		RLEN = -1;											\
	}													\
}

#define DOWRITE(FDESC, FBUFF, FBUFF_LEN)						\
{											\
	ssize_t		gtmioStatus;							\
	size_t		gtmioBuffLen;							\
	sm_uc_ptr_t	gtmioBuff;							\
	gtmioBuffLen = FBUFF_LEN;							\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);						\
	assert(0 != gtmioBuffLen);							\
	for (;;)									\
	{										\
		if (-1 != (gtmioStatus = write(FDESC, gtmioBuff, gtmioBuffLen)))	\
		{									\
			gtmioBuffLen -= gtmioStatus;					\
			if (0 == gtmioBuffLen)						\
				break;							\
			gtmioBuff += gtmioStatus;					\
		}									\
		else if (EINTR != errno)						\
		  break;								\
	}										\
	/* GTMASSERT? */								\
}

#define DOWRITERC(FDESC, FBUFF, FBUFF_LEN, RC)							\
{												\
	GBLREF	int	gtm_non_blocked_write_retries;						\
	ssize_t		gtmioStatus;								\
	size_t		gtmioBuffLen;								\
	sm_uc_ptr_t	gtmioBuff;								\
	int		block_cnt = 0;								\
	gtmioBuffLen = FBUFF_LEN;								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	for (;;)										\
	{											\
		if (-1 != (gtmioStatus = write(FDESC, gtmioBuff, gtmioBuffLen)))		\
		{										\
			gtmioBuffLen -= gtmioStatus;						\
			if (0 == gtmioBuffLen)							\
				break;								\
			gtmioBuff += gtmioStatus;						\
		}										\
		else if (EINTR != errno && EAGAIN != errno)					\
			break;									\
		else if (EAGAIN == errno)							\
		{										\
			if (gtm_non_blocked_write_retries <= block_cnt)				\
				break;								\
			SHORT_SLEEP(WAIT_FOR_BLOCK_TIME);					\
			block_cnt++;								\
		}										\
	}											\
	if (-1 == gtmioStatus)		/* Had legitimate error - return it */			\
		RC = errno;									\
	else if (0 == gtmioBuffLen)								\
		RC = 0;										\
	else											\
		RC = -1;		/* Something kept us from writing what we wanted */	\
}

#define DOLLAR_DEVICE_SET(DEVPTR,STATUS)							\
{												\
	len = SIZEOF(ONE_COMMA) - 1;								\
	memcpy(DEVPTR->dollar.device, ONE_COMMA, len);					\
	errptr = (char *)STRERROR(STATUS);							\
	/* make sure there is room for the 1, and the null at the end */			\
	errlen = MIN(STRLEN(errptr), SIZEOF(DEVPTR->dollar.device) - SIZEOF(ONE_COMMA));	\
	memcpy(&DEVPTR->dollar.device[len], errptr, errlen);				\
	DEVPTR->dollar.device[len + errlen] = '\0';					\
}

#define DOLLAR_DEVICE_WRITE(DEVPTR,STATUS)						\
{											\
	int	len;									\
	int	errlen;									\
	char	*errptr;								\
	/* save error in $device */							\
	if (EAGAIN == STATUS)								\
	{										\
		len = SIZEOF(ONE_COMMA_UNAVAILABLE);					\
		memcpy(DEVPTR->dollar.device, ONE_COMMA_UNAVAILABLE, len);		\
	} else										\
		DOLLAR_DEVICE_SET(DEVPTR,STATUS);					\
}

#define DOWRITERL(FDESC, FBUFF, FBUFF_LEN, RLEN)						\
{												\
	ssize_t		gtmioStatus;								\
	size_t		gtmioBuffLen;								\
	sm_uc_ptr_t	gtmioBuff;								\
	int		block_cnt = 0;								\
												\
	GBLREF	int	gtm_non_blocked_write_retries;						\
												\
	gtmioBuffLen = FBUFF_LEN;								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	for (;;)										\
	{											\
		if (-1 != (gtmioStatus = write(FDESC, gtmioBuff, gtmioBuffLen)))		\
		{										\
			gtmioBuffLen -= gtmioStatus;						\
			if (0 == gtmioBuffLen)							\
				break;								\
			gtmioBuff += gtmioStatus;						\
		}										\
		else if (EINTR != errno && EAGAIN != errno)					\
			break;									\
		else if (EAGAIN == errno)							\
		{										\
			if (gtm_non_blocked_write_retries <= block_cnt)				\
				break;								\
			SHORT_SLEEP(WAIT_FOR_BLOCK_TIME);					\
			block_cnt++;								\
		}										\
	}											\
	if (-1 != gtmioStatus)									\
		RLEN = (int)(FBUFF_LEN - gtmioBuffLen); /* Return length actually written */	\
	else						/* Had legitimate error - return it */	\
		RLEN = -1;									\
}

#define DO_FILE_READ(CHANNEL, OFFSET, READBUFF, LEN, STATUS1, STATUS2)		\
{										\
	STATUS2 = SS_NORMAL;							\
	LSEEKREAD(CHANNEL, OFFSET, READBUFF, LEN, STATUS1);			\
	if (-1 == STATUS1)							\
		STATUS1 = ERR_PREMATEOF;					\
}

#define DB_DO_FILE_WRITE(CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1, STATUS2)	\
{										\
	STATUS2 = SS_NORMAL;							\
	DB_LSEEKWRITE(NULL, NULL, CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1);	\
	if (-1 == STATUS1)							\
		STATUS1 = ERR_PREMATEOF;					\
}

#define JNL_DO_FILE_WRITE(CSA, JNL_FN, CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1, STATUS2)	\
{												\
	STATUS2 = SS_NORMAL;									\
	JNL_LSEEKWRITE(CSA, JNL_FN, CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1);			\
	if (-1 == STATUS1)									\
		STATUS1 = ERR_PREMATEOF;							\
}

typedef struct
{
	int	fd;
	mstr	v;
} file_pointer;

/* WRITEPIPE is a work-around for a problem found in z/OS where the kernel doesn't seem to
 * break up writes into small enough pieces, requiring that we do it ourselves.  The fix is
 * applied to all Unix platforms, even though all except z/OS seem to work with monster writes.
 */
#define WRITEPIPE(FDESC, PIPESZ, FBUFF, FBUFF_LEN, RC)						\
{												\
	GBLREF	int	gtm_non_blocked_write_retries;						\
	ssize_t		gtmioStatus;								\
	size_t		gtmioBuffLen;								\
	size_t		shortBuffLen;								\
	sm_uc_ptr_t	gtmioBuff;								\
	int		block_cnt = 0;								\
	gtmioBuffLen = FBUFF_LEN;								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF);							\
	for (;;)										\
	{											\
		shortBuffLen = MIN(PIPESZ, gtmioBuffLen);					\
		if (-1 != (gtmioStatus = write(FDESC, gtmioBuff, shortBuffLen)))		\
		{										\
			gtmioBuffLen -= gtmioStatus;						\
			if (0 == gtmioBuffLen)							\
				break;								\
			gtmioBuff += gtmioStatus;						\
		}										\
		else if (EINTR != errno && EAGAIN != errno)					\
			break;									\
		else if (EAGAIN == errno)							\
		{										\
			if (gtm_non_blocked_write_retries <= block_cnt)				\
				break;								\
			SHORT_SLEEP(WAIT_FOR_BLOCK_TIME);					\
			block_cnt++;								\
		}										\
	}											\
	if (-1 == gtmioStatus)		/* Had legitimate error - return it */			\
		RC = errno;									\
	else if (0 == gtmioBuffLen)								\
		RC = 0;										\
	else											\
		RC = -1;		/* Something kept us from writing what we wanted */	\
}

#define FFLUSH(STREAM)					\
{							\
	DEFER_INTERRUPTS(INTRPT_IN_FFLUSH);		\
	fflush(STREAM);					\
	ENABLE_INTERRUPTS(INTRPT_IN_FFLUSH);		\
}

#endif
