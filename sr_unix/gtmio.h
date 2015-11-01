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
 * POLL_OBJECT_FILE - polls to open a non-empty object file for reading.
 * CLOSEFILE	Loop until close succeeds for fails with other than EINTR.
 * LSEEKREAD	Performs an lseek followed by read and sets global variable to warn off
 *		async IO routines.
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
#ifdef	__sparc
#include <unistd.h>
#endif
#include "gtm_fcntl.h"

#ifdef __linux__
#include <sys/vfs.h>
#endif

#ifdef __MVS__
#define DOWRITE_A	__write_a
#define DOREAD_A	__read_a
#else
#define DOWRITE_A	DOWRITE
#define DOREAD_A	read
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
		util_out_send_oper("Error finding FS type for file, !AD :!AD", 						\
				TRUE, LEN_AND_STR(FNAME), LEN_AND_STR(STRERROR(macro_errno)));				\
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
		util_out_send_oper("Error finding FS type for file, !AD. !AD", 						\
				TRUE, LEN_AND_STR(FNAME), LEN_AND_STR(STRERROR(macro_errno)));				\
		dio_success = FALSE;											\
	} else if (strcmp(FSTYPE_UFS, statvfs_buf.f_basetype))								\
			dio_success = FALSE;										\
	OPENFILE(FNAME, FFLAGS | O_DSYNC, FDESC);									\
	if (dio_success && -1 != FDESC) 										\
	{														\
		if (-1 == directio(FDESC, DIRECTIO_ON))									\
		{													\
			macro_errno = errno;										\
			util_out_send_oper("Failed to set DIRECT IO option for !AD, reverting to normal IO. !AD ",	\
					TRUE, LEN_AND_STR(FNAME), LEN_AND_STR(STRERROR(macro_errno)));			\
		}													\
	}														\
}
#elif defined(__MVS__)
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_SYNC, FDESC);
#else
#define OPENFILE_SYNC(FNAME, FFLAGS, FDESC)	OPENFILE(FNAME, FFLAGS | O_DSYNC, FDESC);
#endif


#if !defined(__linux__)
#define OPEN_OBJECT_FILE(FNAME, FFLAG, FDESC) \
{ \
	int status; \
	struct flock lock;    /* arg to lock the file thru fnctl */   \
	while (-1 == (FDESC = OPEN(FNAME, FFLAG, 0666)) && EINTR == errno)    \
	;  \
	if (-1 != FDESC)  {  \
		do { \
			lock.l_type = (((FFLAG) & O_WRONLY) || ((FFLAG) & O_RDWR)) ? F_WRLCK : F_RDLCK;\
			lock.l_whence = SEEK_SET; /*locking offsets from file beginning*/ \
			lock.l_start = lock.l_len = 0; /* lock the whole file */\
			lock.l_pid = getpid(); \
		} while (-1 == (status = fcntl(FDESC, F_SETLKW, &lock)) && EINTR == errno); \
		if (-1 == status) { \
			CLOSEFILE(FDESC, status); /* can never fail - no writes & no writes-behind */ \
			FDESC = -1; \
		} \
	} \
}
#elif defined (Linux390)
/* fcntl on Linux390 2.2.16 sometimes returns EINVAL */
#define OPEN_OBJECT_FILE(FNAME, FFLAG, FDESC) \
{ \
	int status; \
	struct flock lock;    /* arg to lock the file thru fnctl */   \
	while (-1 == (FDESC = OPEN(FNAME, FFLAG, 0666)) && EINTR == errno)    \
	;  \
}
#else
/* A special handling was needed for linux due to its inability to lock
   over NFS.  The only difference in code is an added check for NFS file
   thru fstatfs */

/* should ideally include <linux/nfs_fs.h> for NFS_SUPER_MAGIC.
   However this header file doesn't seem to be standard and gives lots of
   compilation errors and hence defining again here.  The constant value
   seems to be portable across all linuxes (courtesy 'statfs' man pages) */
#define NFS_SUPER_MAGIC 0x6969
#define OPEN_OBJECT_FILE(FNAME, FFLAG, FDESC) \
{ \
	struct statfs buf; \
	int status; \
	struct flock lock;    /* arg to lock the file thru fnctl */   \
	while (-1 == (FDESC = OPEN(FNAME, FFLAG, 0666)) && EINTR == errno)    \
	;  \
	if (-1 != FDESC)  {  \
		if (-1 != fstatfs(FDESC, &buf) && NFS_SUPER_MAGIC != buf.f_type) /*is not on NFS?*/\
		{ \
			do { \
				lock.l_type = (((FFLAG) & O_WRONLY) || ((FFLAG) & O_RDWR)) ? F_WRLCK : F_RDLCK;\
				lock.l_whence = SEEK_SET; /*locking offsets from file beginning*/ \
				lock.l_start = lock.l_len = 0; /* lock the whole file */\
				lock.l_pid = getpid(); \
			} while (-1 == (status = fcntl(FDESC, F_SETLKW, &lock)) && EINTR == errno); \
			if (-1 == status) { \
				CLOSEFILE(FDESC, status); /* can never fail - no writes & no writes-behind */ \
				FDESC = -1; \
			} \
		} \
	} \
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

/* This is the workaround for a glitch in read locking in zlink:
 * OPEN_OBJECT_FILE -> 1. open the file, and 2. read lock it.
 * If some other process newly created the file and not yet write-locked it and
 * this process got read lock (step 2), then later incr_link will end up reading
 * an empty file. So here, polling for a non-empty object file before reading.
 */
#define POLL_OBJECT_FILE(FNAME, FDESC) \
for (cntr=0; cntr < MAX_FILE_OPEN_TRIES; cntr++) \
{ \
	OPEN_OBJECT_FILE(FNAME, O_RDONLY, FDESC); \
	if (-1 == FDESC) \
		break; \
	FSTAT_FILE(FDESC, &stat_buf, status); \
	if (!status && 0 < stat_buf.st_size) \
		break; \
	CLOSE_OBJECT_FILE(FDESC, status); \
	FDESC = -1; \
	SHORT_SLEEP(WAIT_FOR_FILE_TIME); \
}

#define CLOSEFILE(FDESC, RC) \
{ \
	do \
	{ \
		RC = close(FDESC); \
	} while(-1 == RC && EINTR == errno); \
}

#define GET_LSEEK_FLAGS_ARRAY \
{ \
	GBLREF	boolean_t	*lseekIoInProgress_flags; \
	if ((boolean_t *)0 == lseekIoInProgress_flags) \
		lseekIoInProgress_flags = (boolean_t *)malloc(sysconf(_SC_OPEN_MAX) * sizeof(boolean_t)); \
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
		RC = -1;		/* Something kept us from reading what we wanted */ \
	SET_LSEEK_FLAG(FDESC, FALSE);	/* Reason this is last is so max optimization occurs */ \
}

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
		RLEN = FBUFF_LEN - gtmioBuffLen; 	/* Return length actually read */ \
	else 	    					/* Had legitimate error - return it */ \
		RLEN = -1; \
}

#define DOREADRLTO(FDESC, FBUFF, FBUFF_LEN, TOFLAG, RLEN) \
{ \
	ssize_t		gtmioStatus; \
	size_t		gtmioBuffLen; \
	sm_uc_ptr_t	gtmioBuff; \
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
		else if (EINTR != errno || TOFLAG) \
		  break; \
        } \
	if (-1 != gtmioStatus) \
		RLEN = FBUFF_LEN - gtmioBuffLen; 	/* Return length actually read */ \
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
		RLEN = FBUFF_LEN - gtmioBuffLen; 	/* Return length actually written */ \
	else 	    					/* Had legitimate error - return it */ \
		RLEN = -1; \
}

typedef struct {
int	fd;
mstr	v;
}file_pointer;

#endif
