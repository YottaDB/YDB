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
#include "min_max.h"

/* la_getstr.c: Prompts for string input until correct; provides initial
		string
   used in    : la_interact.c
*/

short	la_getstr(uint4 kid, int4 code, char *res, int lo, int hi)
/* kid - virt. keyb. ID		*/
/* code - prompt message code	*/
/* res - result returned	*/
/* lo - min length of result	*/
/* hi - max length of result	*/
{
	boolean_t	valid;
	char		buf[4 * ADDR], pro[80];
	char		*ini = NULL;
	unsigned short  w_short;		/* res. string length   */
	int 		cnt;
	int4		status;
	int4		smg$read_string(), sys$getmsg();
	$DESCRIPTOR	(dini, ini);		/* initial string 	*/
	$DESCRIPTOR	(dbuf, buf);		/* resuting string 	*/
	$DESCRIPTOR	(dpro, pro);		/* prompt string	*/
	error_def	(LA_INVAL);

	dpro.dsc$w_length = 80;
	status = sys$getmsg(code, &w_short, &dpro, 1, 0);
	if (SS$_NORMAL != status)
		lib$signal(status);
	dpro.dsc$w_length = w_short;
	for (w_short = 0;  !w_short || ((0 != res[w_short]) && (hi > w_short));  w_short++)
		buf[w_short] = res[w_short];
	dini.dsc$a_pointer = buf;
	dbuf.dsc$w_length = hi;
	valid = FALSE;
	while ((SS$_NORMAL != status) || !valid)
	{
		buf[w_short] = ' ';
		dini.dsc$w_length = w_short;
		status = smg$read_string(&kid, &dbuf, &dpro, &hi, 0, 0, 0, &w_short, 0, 0, &dini);
		if (SS$_NORMAL != status)
			lib$stop(status);
		valid = ((w_short >= lo) && (w_short < hi)) || ((1 == w_short) && (1 == hi));
		if (!valid)
		{
			w_short = MIN(w_short, hi - 1);
 			la_putmsgu(LA_INVAL, 0, 0);
		}
	}
	for (cnt = 0;  cnt < hi;  cnt++)
		res[cnt] = (cnt < w_short) ? buf[cnt] : 0;
	return w_short;
}
