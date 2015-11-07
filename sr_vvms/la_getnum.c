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

#include "mdef.h"

#include <ssdef.h>
#include <descrip.h>

#include "ladef.h"
#include "min_max.h"

/* la_getnum.c: Prompts for a decimal input until valid.  Provides an initial value.
   used in    : la_interact.c
*/

#define MAXLEN		16
#define MAXDECLEN	12				/* max length of a num in decimal digits */

void	la_getnum (uint4 kid, int4 code, int4 *num_ptr, int4 lo, int4 hi)
/* kid - virt. keyb. ID		*/
/* code - prompt code		*/
/* num_ptr - result returned	*/
/* lo - min value of result	*/
/* hi - max value of result 	*/
{
	bool		valid;
	char		buf[32], *ini = NULL, pro[64], *res = NULL;
	unsigned short  length;				/* res. string length   */
	int4		smg$read_string(), sys$getmsg();
	int4		bin, status;
	int4		mlen = MAXLEN;			/* max length of res	*/
	int		cnt0;
	$DESCRIPTOR	(dini, ini);			/* initial string 	*/
	$DESCRIPTOR	(dres, res);			/* resulting string 	*/
	$DESCRIPTOR	(dprm, pro);			/* prompt string	*/
	error_def	(LA_INVAL);

	bin = *num_ptr;
	cnt0 = MAXLEN;
	while (0 != bin)
	{
		cnt0--;
		buf[cnt0] = bin % 10 + '0';
		bin = bin / 10;
	}
	res = buf + cnt0;
	dres.dsc$a_pointer = dini.dsc$a_pointer = res;
	dres.dsc$w_length = MAXLEN;
	dprm.dsc$w_length = SIZEOF(pro);
	status = sys$getmsg(code, &length, &dprm, 1, 0);
	dprm.dsc$w_length = length;
	length = MAXLEN - cnt0;
	valid = FALSE;
	while ((status & 1) && !valid)
	{
		memset(res + length, ' ', cnt0);
		dini.dsc$w_length = MIN(length, MAXDECLEN);
		status = smg$read_string(&kid, &dres, &dprm, &mlen, 0, 0, 0, &length, 0, 0, &dini);
		if (status & 1)
		{
			status = lib$cvt_dtb((int4)length, res, num_ptr);
			valid = ((*num_ptr >= lo) && (*num_ptr < hi) && (SS$_NORMAL == status));
			if (!valid)
				la_putmsgu(LA_INVAL, 0, 0);
		}
	}
	if (!(status & 1))
		lib$signal(status);
}
