/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gtm_time.h"	/* needed for "clock_gettime" */
#include "filestruct.h" /* needed for unix_file_info */

typedef gtm_int64_t		jnl_proc_time;
typedef	int			fd_type;
typedef unix_file_info		fi_type;

/* in disk blocks but jnl file addresses are kept by byte so limited by uint4 for now */
#ifndef OFF_T_LONG
#define JNL_ALLOC_MAX		4194304  /* 2GB */
#else
#define JNL_ALLOC_MAX		8388607  /* 4GB - 512 Bytes */
#endif
/* Since the journal buffer size always gets rounded up to the next multiple of
 * MIN(MAX_IO_BLOCK_SIZE, csd->blk_size) / DISK_BLOCK_SIZE), make the default journal
 * buffer size a multiple of default-block-size-to-512 ratio, which equals 8 (default block size = 4K).
 * That value might need readjustment for block sizes > 4K
 */
#define JNL_BUFFER_DEF		ROUND_UP(JNL_BUFFER_MIN, 8)
#define NOJNL			FD_INVALID_NONPOSIX
#define MID_TIME(W)		W
#define EXTTIMEVMS(T)
#define EXTINTVMS(I)
#define EXTTXTVMS(T,L)

/* The two macros below deliberately use "clock_gettime(CLOCK_REALTIME)" and not the cheaper "time()".
 * On Linux BOTH are served from the vDSO, with no system call, so the difference is not kernel entry.
 * It is what each one reads. "time()" returns a seconds value the kernel refreshes once per timekeeping
 * tick, while "clock_gettime(CLOCK_REALTIME)" reads the hardware clocksource live. Reading a snapshot is
 * why "time()" is the cheaper of the two, and it is also why it can report a second that is up to one
 * tick behind what "clock_gettime(CLOCK_REALTIME)" reports (measured at ~1.6 milliseconds on a 6.12
 * kernel).
 *
 * $HOROLOG, $ZHOROLOG and $ZUT ("op_zhorolog"/"op_zut") and the HANG command ("hiber_start_wall_time")
 * all use "clock_gettime(CLOCK_REALTIME)". If journal record timestamps came from "time()", an M program
 * that records $HOROLOG, does a HANG 1 and then updates a global could get a journal record stamped with
 * the same second as the recorded $HOROLOG (instead of a later second). A MUPIP JOURNAL -SINCE/-BEFORE
 * using that recorded $HOROLOG value would then incorrectly include the update done after the HANG.
 */
#define JNL_SHORT_TIME(S)				\
{							\
	struct timespec	temp_ts;			\
							\
	clock_gettime(CLOCK_REALTIME, &temp_ts);	\
	S = (int4)temp_ts.tv_sec;			\
}

#define JNL_WHOLE_FROM_SHORT_TIME(W, S)	W = (S)
#define	JNL_WHOLE_TIME(W)				\
{							\
	struct timespec	temp_ts;			\
							\
	clock_gettime(CLOCK_REALTIME, &temp_ts);	\
	W = temp_ts.tv_sec;				\
}
#define UNIX_TIME_T_OVERFLOW_WARN_THRESHOLD	0x7acdd140 /* Mon Apr 16 00:00:00 2035 EST */

#define SECONDS_PER_EPOCH_SECOND	1
#define SECOND2EPOCH_SECOND(s)		(s)
#define EPOCH_SECOND2SECOND(e)		(e)
#define JNL_EXT_DEF			"*.mjl"
#define DEF_DB_EXT_NAME			"dat"
#define DEF_JNL_EXT_NAME		".mjl"

uint4 jnl_file_open(gd_region *reg, boolean_t init);

#endif /* JNLSP_H_INCLUDED */
