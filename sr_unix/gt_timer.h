/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GT_TIMER_H__
#define __GT_TIMER_H__

/*
 * -----------------------------------------------------
 * System dependent include file for gtm timer package
 * -----------------------------------------------------
 */
#include <signal.h>

typedef INTPTR_T TID;		/* Timer ID type */

/*
 * -----------------------------------------------------
 * Gtm timer package uses ABS_TIME structure to carry
 * the time information in operating system independent
 * manner. The time in this structure is stored as
 * an absolute time - elapsed time since some major historic
 * event, or some other fixed date in the past. Different
 * operating systems have different time reference points.
 *
 * The time is converted from the OS time format to
 * the ABS_TIME format, and from then on, all
 * timer related code uses this format.
 * -----------------------------------------------------
 */
typedef struct tag_abs_time {
	long	at_sec;		/* seconds */
	long	at_usec;	/* and microseconds */
} ABS_TIME;

/*
 * -----------------------------------------------------
 * All timer requests are placed into a linked list, or
 * a queue of pending requests in a time order.
 * The first timer in this queue is the currently
 * active timer, and expires first.
 * -----------------------------------------------------
 */
typedef struct tag_ts {
	ABS_TIME	expir_time;	/* Absolute Time when timer expires */
	void		(*handler)();	/* Pointer to handler routine */
	struct tag_ts	*next;		/* Pointer to next */
        TID             tid;            /* Timer id */
        int4            safe;           /* just sets flags, no real work */
	int4		hd_len_max;	/* Max length this blk can hold */
	int4		hd_len;		/* Handler data length */
  	GTM64_ONLY(int4 padding;)       /* Padding for 8 byte alignment of hd_data. Remove if hd_data
					 * is made to starts on a 8 byte boundary (for GTM64)
					 */
	char		hd_data[1];	/* Handler data */
} GT_TIMER;

#define GT_WAKE

#ifdef __STDC__
int4 abs_time_comp(ABS_TIME *atp1, ABS_TIME *atp2);
void add_int_to_abs_time(ABS_TIME *atps, int4 ival, ABS_TIME *atpd);
void cancel_timer(TID tid);
void hiber_start(uint4 hiber);
void hiber_start_wait_any(uint4 hiber);
void start_timer(TID tid, int4 time_to_expir, void(* handler)(), int4 data_length, void *handler_data);
ABS_TIME sub_abs_time(ABS_TIME *atp1, ABS_TIME *atp2);
void sys_get_curr_time(ABS_TIME *atp);
void uninit_timers(void);
void prealloc_gt_timers(void);
void check_for_timer_pops(void);
#else
int4 abs_time_comp();
void add_int_to_abs_time();
void cancel_timer();
void hiber_start();
void hiber_start_wait_any();
void start_timer();
ABS_TIME sub_abs_time();
void sys_get_curr_time();
void uninit_timers();
void prealloc_gt_timers();
void check_for_timer_pops();
#endif

#endif


