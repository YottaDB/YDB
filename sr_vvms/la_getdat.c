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

#include <ssdef.h>
#include <descrip.h>

#include "ladef.h"

/* la_getdat.c: Prompts for a date string until correct and within range
   used in    : la_interact.c
*/

void	la_getdat(uint4 kid, int4 code, date *date_ptr, uint4 lo, uint4 hi)
/* kid - virt. keyb. ID		*/
/* code - promt msg code	*/
/* date_ptr - date/time returned	*/
/* lo - min value of date	*/
/* hi - max value of date	*/
{
	int		status;
	bool		valid;
	char		pro[64], res[64];
	unsigned short	length;			/* res. string length	*/
	int4		mlen = 32;		/* max length of result */
	int		smg$read_string(), sys$getmsg();
	int		sys$asctim();
	$DESCRIPTOR(dini, res);			/* initial string	*/
	$DESCRIPTOR(dres, res);			/* resuting string	*/
	$DESCRIPTOR(dprm, pro);			/* prompt string	*/
	error_def(LA_INVAL);

	dprm.dsc$w_length = 64;
	status = sys$getmsg(code, &length, &dprm, 1, 0);
	if (SS$_NORMAL != status)
		lib$signal(status);
	dprm.dsc$w_length = length;
	dres.dsc$w_length = 12;
	if (0 == (*date_ptr)[1])
	{	/* no initial date */
		res[0] = 0;
		length = 0;
	} else
	{
		status = sys$asctim(&length, &dres, date_ptr, 0);
		if (SS$_NORMAL != status)
			lib$signal(status);
	}
	dres.dsc$w_length = 64;
	valid = FALSE;
	while ((SS$_NORMAL != status) || !valid)
	{
		res[length] = ' ';
		dini.dsc$w_length = length;
		status = smg$read_string(&kid, &dres, &dprm, &mlen, 0, 0, 0, &length, 0, 0, &dini);
		if (SS$_NORMAL != status)
			lib$signal(status);
		else  if (0 == length)
		{	/* no datstatus/time */
			(*date_ptr)[0] = (*date_ptr)[1] = lo = 0;
			hi = 1;
		} else  if (0 != length)
		{	/* date/time entered */
			status = lib$convert_date_string(&dres, date_ptr);
			if (SS$_NORMAL != status)
				la_putmsgs(status);
		}
		valid = ((*date_ptr)[1] >= lo || (*date_ptr)[1] < hi);
		if (!valid)
			la_putmsgu(LA_INVAL, 0, 0);
	}
}
