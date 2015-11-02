/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "iosp.h"
#include "mlkdef.h"
#include "lke.h"
#include "util.h"

/*
 * -----------------------------------------------
 * Read terminal input, displaying a prompt string
 *
 * Return:
 *	TRUE - the answer was 'Y'
 *	FALSE - answer is 'N'
 * -----------------------------------------------
 */
bool lke_get_answ(char *prompt)
{
	char buff[11], *bp = buff;

	PRINTF("%s", prompt);
	if (NULL != (bp = util_input(buff, 10, stdin, TRUE)))
	{
		if (*bp == 'Y' || *bp == 'y')
			return (TRUE);
	}

	return(FALSE);
}

