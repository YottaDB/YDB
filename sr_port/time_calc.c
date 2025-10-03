/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

error_def(ERR_TIMEROVFL);

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
{	/* some callers of this function rely on it correctly handling the case where atps and atpd point to the same structure */
	int4	ival_sec, ival_usec;

	if (ival < 0)
	{	/* Negative values won't work properly; they're probably
		 * also an indication of arithmetic overflow when
		 * multiplying by 1000 to convert from seconds to
		 * milliseconds.
		 */
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TIMEROVFL);
	}
	ival_sec  = ival / MILLISECS_IN_SEC;					/* milliseconds -> seconds */
	ival_usec = (ival - (ival_sec * MILLISECS_IN_SEC)) * MICROSECS_IN_MSEC;	/* microsecond remainder */
	if ((atpd->at_usec = atps->at_usec + ival_usec) >= MICROSECS_IN_SEC)    /* WARNING: assignment */
	{       /* microsecond overflow */
		atpd->at_usec -= MICROSECS_IN_SEC;
		ival_sec  += 1;							/* carry */
	}
	atpd->at_sec = atps->at_sec + ival_sec;
}
