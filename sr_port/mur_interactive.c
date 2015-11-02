/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_ctype.h"
#include "gtm_string.h"
#ifdef UNIX
#include "gtm_stdio.h"
#else
#include <descrip.h>
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "util.h"



#define PROCEED_PROMPT	"Proceed? [Y/N]: "
#define CORRECT_PROMPT	"Please enter Y or N: "
#define YES_STRING	"YES"
#define NO_STRING	"NO"


boolean_t mur_interactive(void)
{
	boolean_t	done = FALSE, mur_error_allowed;
	unsigned short	len;
	int		index;
	char		res[8];
	UNIX_ONLY(char *fgets_res;)
	VMS_ONLY($DESCRIPTOR (dres, res);)
	VMS_ONLY($DESCRIPTOR (dprm, PROCEED_PROMPT);)

	while (FALSE == done)
	{
		VMS_ONLY(lib$get_input(&dres, &dprm, &len);)
		UNIX_ONLY(util_out_print(PROCEED_PROMPT, TRUE);
			FGETS(res, 8, stdin, fgets_res);
			fgets_res = util_input(res, sizeof(res), stdin, FALSE);
			if (NULL != fgets_res) {
			len = strlen(res);)
		for (index = 0; index < len; index++)
			res[index] = TOUPPER(res[index]);
		if (0 == memcmp(res, YES_STRING, len))
		{
			done = TRUE;
			mur_error_allowed = TRUE;
			break;
		} else if (0 == memcmp(res, NO_STRING, len))
		{
			done = TRUE;
			mur_error_allowed = FALSE;
			break;
		} else
		{
			util_out_print(CORRECT_PROMPT, TRUE);
			continue;
		}
		UNIX_ONLY(} else util_out_print(CORRECT_PROMPT, TRUE);)
	}
	if (FALSE == mur_error_allowed)
		util_out_print("Recovery terminated by operator", TRUE);

	return (mur_error_allowed);
}
