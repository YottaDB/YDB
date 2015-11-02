/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Define macros to do our IO and restart as appropriate
 *
 * IOCTL        Loop until ioctl call succeeds or fails with other than EINTR.
 * OPENFILE	Loop until open succeeds or fails with other than EINTR.
 * OPENFILE_SYNC
 *		Loop until open succeeds or fails with other than EINTR.
 *		Opens with O_DSYNC/O_SYNC in direct mode where possible.
 *		Else opens only with O_DSYNC or O_SYNC.
 * OPEN_OBJECT_FILE
 *              Opens the object file and waits till it gets a lock(shr/excl).
 *              Sets default perms if it creates a new file.
 * CLOSE_OBJECT_FILE - close the object file after releasing the lock on it.
 * CLOSEFILE	Loop until close succeeds for fails with other than EINTR.
 * CONVERT_OBJECT_LOCK - convert type of lock held on object file
 * LSEEKREAD	Performs either pread() or an lseek()/ read() combination. In
 *		the latter case, sets global variable to warn off async IO routines.
 * LSEEKREAD_AVAILABLE	Same as LSEEKREAD except it has an extra parameter where the number of bytes
 *			ACTUALLY READ are stored irrespective of whether all REQUESTED BYTES were read or not.
 * LSEEKWRITE	Same as LSEEKREAD but for WRITE.
 * DOREADRC	Performs read, returns code 0 if okay, otherwise returns errno.
 * DOREADRL     Performs read but returns length read or -1 if errno is set.
 * DOREADRLTO	Same as DOREADRL but has a timeout flag to poll on interrupts.
 * DOWRITE	Performs write with no error checking/return.
 * DOWRITERC	Performs write, returns code 0 if okay, otherwise returns errno.
 * DOWRITERL	Performs write but returns length written or -1 if errno is set.
 */

#ifndef GTMIO_Included
#define GTMIO_Included

#include <sys/types.h>
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "eintr_wrappers.h"

#ifdef __linux__
#include <sys/vfs.h>
#endif

#ifdef __MVS__
#define DOWRITE_A	__write_a
#define DOREAD_A	__read_a
#define	DOWRITERL_A	#error need to create as part of z/OS port and make __write_a return status and good errno
#else
#define DOWRITE_A	DOWRITE
#define DOREAD_A	read
#define	DOWRITERL_A	DOWRITERL
#endif

#define MAX_FILE_OPEN_TRIES 	20  /* polling count */
#define WAIT_FOR_FILE_TIME 	100 /* msec */

#define IOCTL(FDESC, REQUEST, ARG, RC) \
{ \
	do \
	{ \
		RC = ioctl(FDESC, REQUEST, ARG); \
	} while(-1 == RC && EINTR == errno); \
	if (-1 != RC) \
	        RC = 0; \
	else \
		RC = errno; \
}

#define OPENFILE(FNAME, FFLAGS, FDESC) \
{ \
	do \
	{ \
		FDESC = OPEN(FNAME, FFLAGS); \
	} while(-1 == FDESC && EINTR == errno); \
}

#define OPENFILE3(FNAME, FFLAGS, FMODE, FDESC) \
{ \
	do \
	{ \
		FDESC = OPEN3(FNAME, FFLAGS, FMODE); \
	} while(-1 == FDESC && EINTR == errno); \
}

/* OPENFILE4 not needed - io_open_try handles interrupts */

#define FSTYPE_ADVFS	"advfs"
#define FSTYPE_UFS	"ufs"

#if defined(_AIX)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_DIRECT | O_DSYNC, FDESC);
#elif defined(__osf__)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC) 										\
{															\
	struct statvfs	statvfs_buf;											\
	int		macro_errno;											\
	boolean_t	dio_success = TRUE;										\
	if (-1 == statvfs(FNAME, &statvfs_buf))										\
	{														\
		macro_errno = errno;											\
		util_out_print("Error finding FS type for file, !AD :!AD", OPER, 					\
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
#elif defined(__sparc)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)		 								\
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
	if (dio_success && -1 != FDESC) 										\
	{														\
		if (-1 == directio(FDESC, DIRECTIO_ON))									\
		{													\
			macro_errno = errno;										\
			util_out_print("Failed to set DIRECT IO option for !AD, reverting to normal IO. !AD ",		\
					OPER, LEN_AND_STR(FNAME), LEN_AND_STR(STRERROR(macro_errno)));			\
		}													\
	}														\
}
#elif defined(__MVS__)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_SYNC, FDESC);
#elif defined(__linux__)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_DIRECT | O_DSYNC, FDESC);
#else
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_DSYNC, FDESC);
#endif

#if defined (Linux390)
/* fcntl on Linux390 2.2.16 sometimes returns EINVAL */
#define OPEN_OBJECT_FILE(FNAME, FFLAG, FDESC) \
{ \
	int status; \
	struct flock lock;    /* arg to lock the file thru fnctl */   \
	while (-1 == (FDESC = OPEN3(FNAME, FFLAG, 0666)) && EINTR == errno)    \
	;  \
}
#define CONVERT_OBJECT_LOCK(FDESC, FFLAG, RC)
#else
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
 * read-lock are 1. open the file, and 2. read lock it.  If a process creates the initial, empty version
 * of the file (with the OPEN3), but has not yet write-locked it, and meanwhile, another process does its
 * open and gets a read-lock, then a later read within incr_link() will end up reading an empty file. To
 * avoid that problem, readers have to poll for a non-empty object file before reading.  If the read lock
 * is obtained, but the file is empty, then release the read lock, sleep for a while, and retry the file open.
 */
#define OPEN_OBJECT_FILE(FNAME, FFLAG, FDESC)									\
{														\
	int		status;											\
	struct flock	lock;    /* arg to lock the file thru fnctl */						\
	int		cntr;											\
	struct stat	stat_buf;										\
	pid_t		l_pid;											\
														\
	l_pid = getpid();											\
	for (cntr = 0; cntr < MAX_FILE_OPEN_TRIES; cntr++)							\
	{													\
		while (-1 == (FDESC = OPEN3(FNAME, FFLAG, 0666)) && EINTR == errno)				\
			;											\
		if (-1 != FDESC)										\
		{												\
			LOCK_IS_ALLOWED(FDESC, status);								\
			if (-2 != status)									\
			{											\
				do {										\
					lock.l_type = (((FFLAG) & O_WRONLY) || ((FFLAG) & O_RDWR))		\
						? F_WRLCK : F_RDLCK;						\
					lock.l_whence = SEEK_SET;	/*locking offsets from file beginning*/	\
					lock.l_start = lock.l_len = 0;	/* lock the whole file */ 		\
					lock.l_pid = l_pid;							\
				} while (-1 == (status = fcntl(FDESC, F_SETLKW, &lock)) && EINTR == errno); 	\
			}											\
			if (-1 != status)									\
			{											\
				if ((FFLAG) & O_CREAT)								\
				{										\
					FTRUNCATE(FDESC, 0, status);						\
				} else										\
				{										\
					FSTAT_FILE(FDESC, &stat_buf, status); 					\
					if (status || (0 == stat_buf.st_size))					\
					{									\
						CLOSE_OBJECT_FILE(FDESC, status);				\
						FDESC = -1;							\
						SHORT_SLEEP(WAIT_FOR_FILE_TIME);				\
						continue;							\
					}									\
				}										\
			}											\
			if (-1 == status)									\
			{											\
				CLOSEFILE(FDESC, status);/* can't fail - no writes, no writes-behind */ 	\
				FDESC = -1;									\
			}											\
		}												\
		break;												\
	}													\
}
#define CONVERT_OBJECT_LOCK(FDESC, FFLAG, RC)						\
{											\
	struct flock	lock;    /* arg to lock the file thru fnctl */			\
	pid_t		l_pid;								\
											\
	l_pid = getpid();								\
	do {										\
		lock.l_type = FFLAG;							\
		lock.l_whence = SEEK_SET;	/*locking offsets from file beginning*/	\
		lock.l_start = lock.l_len = 0;	/* lock the whole file */ 		\
		lock.l_pid = l_pid;							\
	} while (-1 == (RC = fcntl(FDESC, F_SETLKW, &lock)) && EINTR == errno); 	\
}
#endif

#ifndef Linux390
#define CLOSE_OBJECT_FILE(FDESC, RC) \
{ \
	struct flock lock;    /* arg to unlock the file thru fnctl */   \
	do { \
		lock.l_type = F_UNLCK; \
		lock.l_whence = SEEK_SET; \
		lock.l_start = lock.l_len = 0; /* unlock the whole file */\
		lock.l_pid = getpid(); \
	} while (-1 == (RC = fcntl(FDESC, F_SETLK, &lock)) && EINTR == errno); \
	CLOSEFILE(FDESC, RC); \
}
#else
#define CLOSE_OBJECT_FILE(FDESC, RC) \
{ \
	CLOSEFILE(FDESC, RC); \
	}
#endif

#define CLOSEFILE(FDESC, RC)					\
{								\
	do							\
	{							\
		RC = close(FDESC);				\
	} while(-1 == RC && EINTR == errno);			\
	if (-1 == RC)	/* Had legitimate error - return it */	\
		RC = errno;					\
}

#if defined(__osf__) || defined(_AIX) || defined(__sparc) || defined(__linux__) || defined(__hpux) || defined(__CYGWIN__)
/* These platforms are known to support pread/pwrite. MVS is unknown and so gets the old support.

   Note !! pread and pwrite do NOT (on most platforms) set the file pointer like lseek/read/write would
   so they are NOT a drop in replacement !!
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

#define LSEEKREAD(FDESC, FPTR, FBUFF, FBUFF_LEN, RC) \
{ \
	ssize_t			gtmioStatus; \
	size_t			gtmioBuffLen; \
	off_t			gtmioPtr; \
	sm_uc_ptr_t 		gtmioBuff; \
	gtmioBuffLen = FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	gtmioPtr = (off_t)(FPTR); \
	for (;;) \
        { \
		if (-1 != (gtmioStatus = pread(FDESC, gtmioBuff, gtmioBuffLen, gtmioPtr))) \
		{ \
			gtmioBuffLen -= gtmioStatus; \
			if (0 == gtmioBuffLen || 0 == gtmioStatus) \
			        break; \
			gtmioBuff += gtmioStatus; \
			gtmioPtr += gtmioStatus; \
			continue; \
		} \
		if (EINTR != errno) \
			break; \
        } \
	if (0 == gtmioBuffLen) \
		RC = 0; \
	else if (-1 == gtmioStatus)    	/* Had legitimate error - return it */ \
		RC = errno; \
	else \
		RC = -1;		/* Something kept us from reading what we wanted */ \
}

/* The below macro is almost the same as LSEEKREAD except it has an extra parameter where the number of
 * bytes ACTUALLY READ are stored irrespective of whether all REQUESTED BYTES were read or not.
 */
#define LSEEKREAD_AVAILABLE(FDESC, FPTR, FBUFF, FBUFF_LEN, ACTUAL_READLEN, RC)			\
{												\
	ssize_t			gtmioStatus;							\
	size_t			gtmioBuffLen;							\
	off_t			gtmioPtr;							\
	sm_uc_ptr_t 		gtmioBuff;							\
												\
	gtmioBuffLen = (FBUFF_LEN); 								\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); 							\
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
	else if (-1 == gtmioStatus)    	/* Had legitimate error - return it */			\
		RC = errno;									\
	else											\
		RC = -1;		/* Something kept us from reading what we wanted */	\
}

#define LSEEKWRITE(FDESC, FPTR, FBUFF, FBUFF_LEN, RC) \
{ \
	ssize_t			gtmioStatus; \
	size_t			gtmioBuffLen; \
	off_t			gtmioPtr; \
	sm_uc_ptr_t 		gtmioBuff; \
	gtmioBuffLen = FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	gtmioPtr = (off_t)(FPTR); \
	for (;;) \
        { \
		if (-1 != (gtmioStatus = pwrite(FDESC, gtmioBuff, gtmioBuffLen, gtmioPtr))) \
		{ \
			gtmioBuffLen -= gtmioStatus; \
			if (0 == gtmioBuffLen) \
			        break; \
			gtmioBuff += gtmioStatus; \
			gtmioPtr += gtmioStatus; \
			continue; \
		} \
		if (EINTR != errno) \
			break; \
        } \
	if (0 == gtmioBuffLen) \
		RC = 0; \
	else if (-1 == gtmioStatus)    	/* Had legitimate error - return it */ \
		RC = errno; \
	else \
		RC = -1;		/* Something kept us from writing what we wanted */ \
}

#else /* real lseek and read/write - still need to protect against interrupts inbetween calls */

#define NOPIO_ONLY(X) X

/* Note array is not initialized but first IO to a given file descriptor will initialize that element */
#define GET_LSEEK_FLAGS_ARRAY						\
{									\
	GBLREF	boolean_t	*lseekIoInProgress_flags;		\
	if ((boolean_t *)0 == lseekIoInProgress_flags)								\
		lseekIoInProgress_flags = (boolean_t *)malloc(sysconf(_SC_OPEN_MAX) * sizeof(boolean_t));	\
}

#define GET_LSEEK_FLAG(FDESC, VAL) \
{ \
	GBLREF	boolean_t	*lseekIoInProgress_flags; \
	GET_LSEEK_FLAGS_ARRAY; \
	VAL = lseekIoInProgress_flags[(FDESC)]; \
}

#define SET_LSEEK_FLAG(FDESC, VAL) \
{ \
	GBLREF	boolean_t	*lseekIoInProgress_flags; \
	GET_LSEEK_FLAGS_ARRAY; \
	lseekIoInProgress_flags[(FDESC)] = VAL; \
}

#define LSEEKREAD(FDESC, FPTR, FBUFF, FBUFF_LEN, RC) \
{ \
	GBLREF boolean_t	*lseekIoInProgress_flags; \
	ssize_t			gtmioStatus; \
	size_t			gtmioBuffLen; \
	off_t			gtmioPtr; \
	sm_uc_ptr_t 		gtmioBuff; \
	SET_LSEEK_FLAG(FDESC, TRUE); \
	gtmioBuffLen = FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	gtmioPtr = (off_t)(FPTR); \
	for (;;) \
        { \
		if (-1 != (gtmioStatus = (ssize_t)lseek(FDESC, gtmioPtr, SEEK_SET))) \
		{ \
			if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen))) \
			{ \
				gtmioBuffLen -= gtmioStatus; \
				if (0 == gtmioBuffLen || 0 == gtmioStatus) \
				        break; \
				gtmioBuff += gtmioStatus; \
				gtmioPtr += gtmioStatus; \
				continue; \
			} \
		} \
		if (EINTR != errno) \
			break; \
        } \
	if (0 == gtmioBuffLen) \
		RC = 0; \
	else if (-1 == gtmioStatus)    	/* Had legitimate error - return it */ \
		RC = errno; \
	else \
		RC = -1;		/* Something kept us from reading what we wanted */ \
	SET_LSEEK_FLAG(FDESC, FALSE);	/* Reason this is last is so max optimization occurs */ \
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
	sm_uc_ptr_t 		gtmioBuff;							\
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
	else if (-1 == gtmioStatus)    	/* Had legitimate error - return it */			\
		RC = errno;									\
	else											\
		RC = -1;		/* Something kept us from reading what we wanted */	\
	SET_LSEEK_FLAG(FDESC, FALSE);	/* Reason this is last is so max optimization occurs */	\
}

#define LSEEKWRITE(FDESC, FPTR, FBUFF, FBUFF_LEN, RC) \
{ \
	GBLREF boolean_t	*lseekIoInProgress_flags; \
	ssize_t			gtmioStatus; \
	size_t			gtmioBuffLen; \
	off_t			gtmioPtr; \
	sm_uc_ptr_t 		gtmioBuff; \
	SET_LSEEK_FLAG(FDESC, TRUE); \
	gtmioBuffLen = FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	gtmioPtr = (off_t)(FPTR); \
	for (;;) \
        { \
		if (-1 != (gtmioStatus = (ssize_t)lseek(FDESC, gtmioPtr, SEEK_SET))) \
		{ \
			if (-1 != (gtmioStatus = write(FDESC, gtmioBuff, gtmioBuffLen))) \
			{ \
				gtmioBuffLen -= gtmioStatus; \
				if (0 == gtmioBuffLen) \
				        break; \
				gtmioBuff += gtmioStatus; \
				gtmioPtr += gtmioStatus; \
				continue; \
			} \
		} \
		if (EINTR != errno) \
			break; \
        } \
	if (0 == gtmioBuffLen) \
		RC = 0; \
	else if (-1 == gtmioStatus)    	/* Had legitimate error - return it */ \
		RC = errno; \
	else \
		RC = -1;		/* Something kept us from writing what we wanted */ \
	SET_LSEEK_FLAG(FDESC, FALSE);	/* Reason this is last is so max optimization occurs */ \
}
#endif /* if old lseekread/writes */

#define DOREADRC(FDESC, FBUFF, FBUFF_LEN, RC) \
{ \
	ssize_t		gtmioStatus; \
	size_t		gtmioBuffLen; \
	sm_uc_ptr_t 	gtmioBuff; \
	gtmioBuffLen = FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	for (;;) \
        { \
		if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen))) \
	        { \
			gtmioBuffLen -= gtmioStatus; \
			if (0 == gtmioBuffLen || 0 == gtmioStatus) \
				break; \
			gtmioBuff += gtmioStatus; \
	        } \
		else if (EINTR != errno) \
			break; \
        } \
	if (-1 == gtmioStatus)	    	/* Had legitimate error - return it */ \
		RC = errno; \
	else if (0 == gtmioBuffLen) \
	        RC = 0; \
	else \
		RC = -1;		/* Something kept us from reading what we wanted */ \
}

#define DOREADRL(FDESC, FBUFF, FBUFF_LEN, RLEN) \
{ \
	ssize_t		gtmioStatus; \
	size_t		gtmioBuffLen; \
	sm_uc_ptr_t 	gtmioBuff; \
	gtmioBuffLen = FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	for (;;) \
        { \
		if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen))) \
	        { \
			gtmioBuffLen -= gtmioStatus; \
			if (0 == gtmioBuffLen || 0 == gtmioStatus) \
				break; \
			gtmioBuff += gtmioStatus; \
	        } \
		else if (EINTR != errno) \
		  break; \
        } \
	if (-1 != gtmioStatus) \
		RLEN = (int)(FBUFF_LEN - gtmioBuffLen); /* Return length actually read */ \
	else 	    					/* Had legitimate error - return it */ \
		RLEN = -1; \
}

#define DOREADRLTO(FDESC, FBUFF, FBUFF_LEN, TOFLAG, RLEN) \
{ \
	ssize_t		gtmioStatus; \
	size_t		gtmioBuffLen; \
	sm_uc_ptr_t	gtmioBuff; \
	gtmioBuffLen =  (size_t)FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	for (;;) \
        { \
		if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen))) \
	        { \
			gtmioBuffLen -= gtmioStatus; \
			if (0 == gtmioBuffLen || 0 == gtmioStatus) \
				break; \
			gtmioBuff += gtmioStatus; \
	        } \
		else if (EINTR != errno || TOFLAG) \
		  break; \
        } \
	if (-1 != gtmioStatus) \
		RLEN = (int)(FBUFF_LEN - gtmioBuffLen); 	/* Return length actually read */ \
	else 	    					/* Had legitimate error - return it */ \
		RLEN = -1; \
}

#define DOWRITE(FDESC, FBUFF, FBUFF_LEN) \
{ \
	ssize_t		gtmioStatus; \
	size_t		gtmioBuffLen; \
	sm_uc_ptr_t 	gtmioBuff; \
	gtmioBuffLen = FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	assert(0 != gtmioBuffLen);	\
	for (;;) \
        { \
		if (-1 != (gtmioStatus = write(FDESC, gtmioBuff, gtmioBuffLen))) \
	        { \
			gtmioBuffLen -= gtmioStatus; \
			if (0 == gtmioBuffLen) \
				break; \
			gtmioBuff += gtmioStatus; \
	        } \
		else if (EINTR != errno) \
		  break; \
        } \
	/* GTMASSERT? */ \
}

#define DOWRITERC(FDESC, FBUFF, FBUFF_LEN, RC) \
{ \
	ssize_t		gtmioStatus; \
	size_t		gtmioBuffLen; \
	sm_uc_ptr_t	gtmioBuff; \
	gtmioBuffLen = FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	for (;;) \
        { \
		if (-1 != (gtmioStatus = write(FDESC, gtmioBuff, gtmioBuffLen))) \
	        { \
			gtmioBuffLen -= gtmioStatus; \
			if (0 == gtmioBuffLen) \
				break; \
			gtmioBuff += gtmioStatus; \
	        } \
		else if (EINTR != errno) \
		  break; \
        } \
	if (-1 == gtmioStatus)	    	/* Had legitimate error - return it */ \
		RC = errno; \
	else if (0 == gtmioBuffLen) \
	        RC = 0; \
	else \
		RC = -1;		/* Something kept us from writing what we wanted */ \
}

#define DOWRITERL(FDESC, FBUFF, FBUFF_LEN, RLEN) \
{ \
	ssize_t		gtmioStatus; \
	size_t		gtmioBuffLen; \
	sm_uc_ptr_t 	gtmioBuff; \
	gtmioBuffLen = FBUFF_LEN; \
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); \
	for (;;) \
        { \
		if (-1 != (gtmioStatus = write(FDESC, gtmioBuff, gtmioBuffLen))) \
	        { \
			gtmioBuffLen -= gtmioStatus; \
			if (0 == gtmioBuffLen) \
			        break; \
			gtmioBuff += gtmioStatus; \
	        } \
		else if (EINTR != errno) \
		        break; \
        } \
	if (-1 != gtmioStatus) \
		RLEN = (int)(FBUFF_LEN - gtmioBuffLen); 	/* Return length actually written */ \
	else 	    					/* Had legitimate error - return it */ \
		RLEN = -1; \
}

#define DO_FILE_READ(CHANNEL, OFFSET, READBUFF, LEN, STATUS1, STATUS2)		\
{										\
	STATUS2 = SS_NORMAL;							\
	LSEEKREAD(CHANNEL, OFFSET, READBUFF, LEN, STATUS1);			\
	if (-1 == STATUS1)							\
		STATUS1 = ERR_PREMATEOF;					\
}

#define DO_FILE_WRITE(CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1, STATUS2)	\
{										\
	STATUS2 = SS_NORMAL;							\
	LSEEKWRITE(CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1);			\
	if (-1 == STATUS1)							\
		STATUS1 = ERR_PREMATEOF;					\
}

typedef struct {
int	fd;
mstr	v;
}file_pointer;

#endif
