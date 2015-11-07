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

#ifndef __GT_TIMER_H__
#define __GT_TIMER_H__

/*
 * -----------------------------------------------------
 * System dependent include file for gtm timer package
 * -----------------------------------------------------
 */

typedef int4 TID;		/* Timer ID type */

#define sighold(x)
#define sigrelse(x)

/*
 * -----------------------------------------------------
 * Gtm timer package uses ABS_TIME structure to carry
 * the time information irrelevantly of operating system
 * specifics. The time in this structure is stored as
 * absolute time - elapsed time since some major historic
 * event, or some fixed date in the past. Different
 * operating systems have different time reference points.
 * The time is converted from the OS time format to
 * a format, given in this file, and from then on, all
 * timer related calls reffer to this time.
 * -----------------------------------------------------
 */
typedef struct tag_abs_time {
	int4	at_sec;		/* seconds */
	int4	at_usec;	/* and microseconds */
} ABS_TIME;

typedef	long	gtm_tv_usec_t;

/*
 * -----------------------------------------------------
 * All timer request are placed into a linked list, or
 * a queue of * pending requests in a time order.
 * The first timer in this queue is currently the
 * active timer, and expires first.
 * -----------------------------------------------------
 */
typedef struct tag_ts {
	TID		tid;		/* Timer id */
	ABS_TIME	expir_time;	/* Absolute Time when timer expires */
	void		(*handler)();	/* Pointer to handler routine */
	struct tag_ts	*next;		/* Pointer to next */
} GT_TIMER;

#define GT_WAKE sys$wake(0,0)
#define hiber_start_wait_any hiber_start

int4 abs_time_comp(ABS_TIME *atp1, ABS_TIME *atp2);
void add_int_to_abs_time(ABS_TIME *atps, int4 ival, ABS_TIME *atpd);
void cancel_timer(TID tid);
void hiber_start(uint4 hiber);
void start_timer(TID tid, int4 time_to_expir, void(* handler)(), int4 data_length, void *handler_data);
ABS_TIME sub_abs_time(ABS_TIME *atp1, ABS_TIME *atp2);
void sys_get_curr_time(ABS_TIME *atp);

#endif
