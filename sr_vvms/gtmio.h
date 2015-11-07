/****************************************************************
 *								*
 *	Copyright 2003, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTMIO_Included
#define GTMIO_Included

/* Include necessary files for system calls
 * CHANNEL : The channel to read
 * OFFSET  : Disk offset in bytes. Should be divisible by DISK_BLOCK_SIZE.
 * BUFF    : Buffer to read data.
 * LEN     : Length in bytes to read.
 * STATUS1 : Status of system call
 * STATUS2 : Secondary status of system call
 */
#define DO_FILE_READ(CHANNEL, OFFSET, READBUFF, LEN, STATUS1, STATUS2)			\
{											\
	off_jnl_t		start;							\
	io_status_block_disk	iosb;							\
											\
	STATUS2 = SS_NORMAL;								\
	assert(0 == (OFFSET) % DISK_BLOCK_SIZE);					\
	start = OFFSET >> LOG2_DISK_BLOCK_SIZE;						\
	STATUS1 = sys$qiow(EFN$C_ENF, CHANNEL, IO$_READVBLK, &iosb, NULL, 0,		\
		 	(uchar_ptr_t)(READBUFF), LEN, start + 1, 0, 0, 0);		\
	if (1 & STATUS1)								\
		STATUS1 = iosb.cond;							\
}

#define DB_DO_FILE_WRITE(CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1, STATUS2)		\
	DO_FILE_WRITE(CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1, STATUS2)

#define JNL_DO_FILE_WRITE(CSA, JNL_FN, CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1, STATUS2)	\
	DO_FILE_WRITE(CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1, STATUS2)

#define DO_FILE_WRITE(CHANNEL, OFFSET, WRITEBUFF, LEN, STATUS1, STATUS2)		\
{											\
	off_jnl_t		start;							\
	io_status_block_disk	iosb;							\
											\
	STATUS2 = SS_NORMAL;								\
	assert(0 == (OFFSET) % DISK_BLOCK_SIZE);					\
	start = OFFSET >> LOG2_DISK_BLOCK_SIZE;						\
	STATUS1 = sys$qiow(EFN$C_ENF, CHANNEL, IO$_WRITEVBLK, &iosb, NULL, 0,		\
		 	(uchar_ptr_t)(WRITEBUFF), LEN, start + 1, 0, 0, 0);		\
	if (1 & STATUS1)								\
		STATUS1 = iosb.cond;							\
}

#define DOREADRC(FDESC, FBUFF, FBUFF_LEN, RC) 						\
{ 											\
	ssize_t		gtmioStatus; 							\
	size_t		gtmioBuffLen; 							\
	sm_uc_ptr_t 	gtmioBuff;							\
	gtmioBuffLen = FBUFF_LEN; 							\
	gtmioBuff = (sm_uc_ptr_t)(FBUFF); 						\
	for (;;) 									\
        { 										\
		if (-1 != (gtmioStatus = read(FDESC, gtmioBuff, gtmioBuffLen))) 	\
	        { 									\
			gtmioBuffLen -= gtmioStatus; 					\
			if (0 == gtmioBuffLen || 0 == gtmioStatus) 			\
				break; 							\
			gtmioBuff += gtmioStatus; 					\
	        } 									\
		else if (EINTR != errno) 						\
			break; 								\
        } 										\
	if (-1 == gtmioStatus)	    	/* Had legitimate error - return it */ 		\
		RC = errno; 								\
	else if (0 == gtmioBuffLen) 							\
	        RC = 0; 								\
	else 										\
		RC = -1;		/* Something kept us from reading what we wanted */ \
}

/* Use CLOSEFILE for those files (or sockets) that are opened using UNIX (POSIX) system calls */
#define CLOSEFILE(FDESC, RC)					\
{								\
	do							\
	{							\
		RC = close(FDESC);				\
	} while(-1 == RC && EINTR == errno);			\
	if (-1 == RC)	/* Had legitimate error - return it */	\
		RC = errno;					\
}

/* CLOSEFILE_RESET
 * 	Loop until close succeeds for fails with other than EINTR.
 * 	At end reset channel to FD_INVALID unconditionally (even if close was not successful).
 */
#define	CLOSEFILE_RESET(FDESC, RC)	\
{					\
	CLOSEFILE(FDESC, RC);		\
	FDESC = FD_INVALID;		\
}

#endif
