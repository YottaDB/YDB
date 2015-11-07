/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

#ifndef SS$_NORMAL
#include <ssdef.h>
#endif

typedef	short			fd_type;
typedef vms_file_info		fi_type;

/* in disk blocks but jnl file addresses are kept by byte so limited by uint4 for now */
#define JNL_ALLOC_MAX		8388607
#define JNL_BUFFER_DEF		128
#define NOJNL			FD_INVALID_NONPOSIX
/*
 * In VMS, time is a 64-bit quantity whose unit is in 100 nanoseconds.
 * To convert it to a unit of time that has more relevance to the user (the most desirable would be a second)
 * 	and not lose many CPU cycles in the process, we extract bits 23 thru 55 (0 is lsb, 63 is msb), a total of
 * 	33 bits. This gives us a unit (which we now refer to as an epoch-second) which is 0.8388608 of a second.
 * 	Since we want to store the extracted bits in a uint4 and since bit 55 is 1 until sometime in year 2087,
 * 	we ignore bit 55 and assume 1 in that position while maintaining whole (64 bit) time quantities.
 * 	Bit 55 toggles approximately every 41700 days (114 years). System time on VMS has crossed this
 * 	period once. So, bit 55 is 1 now. Bits 56 thru 63 are 0 now.
 */

/* we'll zap 9 higher order bits and 23 lower order bits of 64 bit time */

#define JNL_MIDTIME_ZEROBITS		7			/* 23 - SIZEOF(low_time) */
#define JNL_HITIME_MASK			(short)0x007F 		/* (short)((1 << (JNL_MIDTIME_ZEROBITS)) - 1) */
#define JNL_HITIME_ADJUST_MASK		(short)(JNL_HITIME_MASK + 1)
#define JNL_MIDTIME_MASK		(uint4)0xFFFFFF80	/* (unsigned int)(~JNL_HITIME_MASK) */
#define JNL_LOWTIME_MASK		(unsigned short)0x0000	/* (unsigned short)(0) */
#define JNL_HITIME_WARN_THRESHOLD	(short)0x00FD		/* warn 980 days before our algorithm breaks */
#define	JNL_LOTIME_ADJUST_MASK		(uint4)(1 << 22)	/* instead of filling in 0 lowtime, fill in mean value */

#ifdef INT8_SUPPORTED

typedef gtm_int64_t jnl_proc_time;

#define JNL_SHORTTIME_MASK		0x007FFFFFFF800000ll
	/* ((JNL_HITIME_MASK << SIZEOF(midtime)) | JNL_MIDTIME_MASK) << SIZEOF(lowtime) */
#define MID_TIME(W)			(uint4)(((W) & JNL_SHORTTIME_MASK) >> 23) /* (masked W) >> (JNL_MIDTIME_ZEROBITS + 16) */
#define JNL_WHOLE_FROM_SHORT_TIME(W, S)	W = (((jnl_proc_time)(S) << 23) 				\
						| ((jnl_proc_time)(JNL_HITIME_ADJUST_MASK) << 48))
#define JNL_FULL_HI_TIME(W)		(short)((W) >> 48) /* whole_time >> (SIZEOF(mid_time) + SIZEOF(low_time)) */

#else /* ! INT8_SUPPORTED */

#ifdef __ALPHA			/* We know that ALPHA is a 64 bit chip (INT8_SUPPORTED) and so is covered by the if case, but */
# pragma member_alignment save  /* we are keeping the alignment pragma to serve as an indicator for any future platforms */
# pragma nomember_alignment	/* As of today, only VAX is not INT8_SUPPORTED and VAX anyway guarantees nomember_alignment for */
#endif				/* jnl_proc_time structure. Vinaya Feb 28, 2002 */

typedef	struct
{
	unsigned short		low_time;
	uint4			mid_time;
	short			hi_time;
} jnl_proc_time;

#ifdef __ALPHA
# pragma member_alignment restore
#endif

#define MID_TIME(W)		(((uint4)((W).hi_time  & JNL_HITIME_MASK)  << (32 - JNL_MIDTIME_ZEROBITS)) | \
				 ((uint4)((W).mid_time & JNL_MIDTIME_MASK) >> JNL_MIDTIME_ZEROBITS) | \
				 ((uint4)((W).low_time & JNL_LOWTIME_MASK))) /* to make the definition complete; compiler is
									      * hopefully smart enough that this sub-expression
									      * is not evaluated */
#define JNL_WHOLE_FROM_SHORT_TIME(W, S)								\
{												\
	(W).hi_time  = (short)((S) >> (32 - JNL_MIDTIME_ZEROBITS)) | JNL_HITIME_ADJUST_MASK;	\
	(W).mid_time = (uint4)((S) << JNL_MIDTIME_ZEROBITS);					\
	(W).low_time = (unsigned short)((S) & JNL_LOWTIME_MASK);				\
}
#define JNL_FULL_HI_TIME(W)	((W).hi_time)

#endif /* INT8_SUPPORTED */

#define	JNL_SHORT_TIME(S)			\
{						\
	jnl_proc_time	quad_time; 		\
	int4		status; 		\
						\
	status = sys$gettim(&quad_time);	\
	assert(status & 1);			\
	S = MID_TIME(quad_time);		\
}

#define	JNL_WHOLE_TIME(W)			\
{						\
	int4 status; 				\
						\
	status = sys$gettim(&(W));		\
	assert(status & 1);			\
}

/* Note that the user interface for epoch-interval is in seconds both in terms of input and output. Only the internal
 * representation is different.
 */
#define SECONDS_PER_EPOCH_SECOND	0.8388608 /* 2**64BIT_TIME_23_LSB_IGNORED * 100 ns = (2**23) * (100 * 10**-9) */
/* the following macros need #include <math.h> for the prototypes as otherwise it will give arbitrary results */
#define SECOND2EPOCH_SECOND(s)	(uint4)floor((double)(s) / SECONDS_PER_EPOCH_SECOND) /* int approximation will always be less than
										      * exact, so we might write epochs more
										      * frequently than user specified. */
#define EPOCH_SECOND2SECOND(e)	(uint4)ceil((double)(e) * SECONDS_PER_EPOCH_SECOND) /* for values from 1 thru 32K-1, we've noticed
										     * that conversion from seconds to epoch seconds
										     * and back does not lose precision
										     */
#define JNL_EXT_DEF		".MJL"
#define DEF_DB_EXT_NAME		"DAT"
#define DEF_JNL_EXT_NAME	JNL_EXT_DEF

#define EXTTIMEVMS(T)		EXTTIME(T)
#define	EXTINTVMS(I)		EXTINT(I)
#define	EXTTXTVMS(T,L)		EXTTXT(T,L)

uint4	jnl_file_open(gd_region *reg, bool init, void oper_ast());
void	jnl_mm_timer_write(gd_region *reg);

#endif /* JNLSP_H_INCLUDED */
