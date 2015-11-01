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

#include "mdef.h"

/*	timeout2msec - convert timeout seconds to milliseconds; check for range of value.
 *
 *	According to the M(UMPS) standard, a negative timeout value should be equivalent to a zero timeout value.
 *
 *	input:
 *
 *		timeout -	timeout value in seconds
 *
 *
 *	return value:
 *
 *		if timeout
 *			<= 0		zero
 *			> MAXPOSINT4	MAXPOSINT4, and indicate error (arithmetic overflow)
 *			else		timeout*1000 (timeout value in milliseconds)
 */

int4	timeout2msec (int4 timeout)
{
	int4	retval;

	error_def (ERR_TIMEROVFL);


	if (timeout <= 0)
		retval = 0;
	else if (timeout > MAXPOSINT4/1000)
	{
		/* If multiplying by 1000 would cause arithmetic overflow out of a 4-byte signed integer, indicate an error.  */
		rts_error (VARLSTCNT(1) ERR_TIMEROVFL);
		retval = MAXPOSINT4;	/* probably won't get here, but largest possible value, just in case */
	} else
		retval = timeout*1000;	/* seconds -> milliseconds */
	return retval;
}
