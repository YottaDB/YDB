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

/*
 * --------------------------------------------------------------
 * This file contains system independent routines for time handling
 * --------------------------------------------------------------
 */

#include "mdef.h"

#include "gt_timer.h"


/*
 * ----------------------------------
 * Compare 2 absolute times
 *
 * Return:
 *	-1	- atp1 < atp2
 *	0	- atp1 = atp2
 *	1	- atp1 > atp2
 * ----------------------------------
 */
int4	abs_time_comp(ABS_TIME *atp1, ABS_TIME *atp2)
{
	if (atp1->at_sec > atp2->at_sec)
		return (1);
	if (atp1->at_sec < atp2->at_sec)
		return (-1);
	if (atp1->at_usec == atp2->at_usec)
		return (0);
	if (atp1->at_usec > atp2->at_usec)
		return (1);
	return (-1);
}


/*
 * ---------------------------------------------------------------------
 * Add integer to absolute time
 *	Absolute time structure is seconds & microseconds.
 *	Integer value is in milliseconds.
 *
 * Arguments:
 *	atps	- source time structure
 *	ival	- integer to be added to source structure (milliseconds)
 *	atpd	- destination time structure
 * ---------------------------------------------------------------------
 */
void	add_int_to_abs_time(ABS_TIME *atps, int4 ival,ABS_TIME *atpd)
{
	int4	ival_sec, ival_usec;
	error_def (ERR_TIMEROVFL);

	if (ival < 0)
	{
		/* Negative values won't work properly; they're probably
		 * also an indication of arithmetic overflow when
		 * multiplying by 1000 to convert from seconds to
		 * milliseconds.
		 */
		rts_error(VARLSTCNT(1) ERR_TIMEROVFL);
	}

	ival_sec  = ival / 1000;			/* milliseconds -> seconds */
	ival_usec = (ival - (ival_sec * 1000)) * 1000;	/* microsecond remainder */

	atpd->at_sec = atps->at_sec + ival_sec;
	if ((atpd->at_usec = atps->at_usec + ival_usec) >= 1000000)
	{
		/* microsecond overflow */
		atpd->at_usec -= 1000000;
		atpd->at_sec  += 1;		/* carry */
	}
}


/*
 * ------------------------------------------------------
 * Substract absolute time atp2 from absolute time atp1
 *	Absolute time structure is seconds & microseconds.
 *	Integer value is in milliseconds.
 *
 * Arguments:
 *	atp1	- source time structure
 *	atp2	- destination time structure
 *
 * Return:
 *	difference time structure
 * ------------------------------------------------------
 */
ABS_TIME	sub_abs_time(ABS_TIME *atp1, ABS_TIME *atp2)
{
	ABS_TIME	dat;
	int4		ival;

	dat.at_sec = atp1->at_sec - atp2->at_sec;
	dat.at_usec = atp1->at_usec - atp2->at_usec;

	if (atp2->at_usec > atp1->at_usec)
	{
		dat.at_usec += 1000000;
		dat.at_sec--;
	}
	return (dat);
}
