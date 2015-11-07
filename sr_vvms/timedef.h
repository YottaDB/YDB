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

typedef struct
{
	int4 mask1;
	int4 mask2;
}quad_mask;

#define quadgtr(a,b) (a[1] > b[1] || a[1] == b[1] && a[0] > b[0])
#define time_low(t) ((t % 430) * 10000000) /* time in 100 nanosecond intervals */
#define time_high(t) (t / 430)
	/* Convert time in msecs. to Quadword Hi and Low */
#define time_low_ms(t) ((t % 429496) * 10000)
#define time_high_ms(t) (t / 429496)
#define status_normal(a) ((norm_stat = (a)) == SS$_NORMAL ? TRUE : rts_error(norm_stat))
