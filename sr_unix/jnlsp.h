/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef JNLSP_H_INCLUDED
#define JNLSP_H_INCLUDED

/* Start jnlsp.h - platform-specific journaling definitions.  */

#include <errno.h>

typedef gtm_int64_t		jnl_proc_time;
typedef	int			fd_type;
typedef unix_file_info		fi_type;

/* in disk blocks but jnl file addresses are kept by byte so limited by uint4 for now */
#ifndef OFF_T_LONG
#define JNL_ALLOC_MAX		4194304  /* 2GB */
#else
#define JNL_ALLOC_MAX		8388607  /* 4GB - 512 Bytes */
#endif
#define JNL_BUFFER_DEF		ROUND_UP2(128, MAX_IO_BLOCK_SIZE / DISK_BLOCK_SIZE)
#define NOJNL			FD_INVALID_NONPOSIX
#define MID_TIME(W)		W
#define EXTTIMEVMS(T)
#define EXTINTVMS(I)
#define EXTTXTVMS(T,L)

#ifndef GTM64
#define	JNL_SHORT_TIME(S)		(time((time_t *)&S))
#else
#define JNL_SHORT_TIME(S)		\
{					\
	time_t temp_t; 			\
	time(&temp_t); 			\
	S = (int4) temp_t;		\
}
#endif

#define JNL_WHOLE_FROM_SHORT_TIME(W, S)	W = (S)
#define	JNL_WHOLE_TIME(W)		\
{					\
	time_t temp_t; 			\
	time(&temp_t); 			\
	W = temp_t;			\
}
#define UNIX_TIME_T_OVERFLOW_WARN_THRESHOLD	0x7acdd140 /* Mon Apr 16 00:00:00 2035 EST */

#define SECONDS_PER_EPOCH_SECOND	1
#define SECOND2EPOCH_SECOND(s)		(s)
#define EPOCH_SECOND2SECOND(e)		(e)
#define JNL_EXT_DEF			"*.mjl"
#define DEF_DB_EXT_NAME			"dat"
#define DEF_JNL_EXT_NAME		".mjl"

uint4 jnl_file_open(gd_region *reg, bool init, void *dummy);

#endif /* JNLSP_H_INCLUDED */
