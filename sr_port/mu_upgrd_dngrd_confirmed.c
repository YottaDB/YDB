/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef UNIX
#include "gtm_stdio.h"
#else
#include <descrip.h>
#endif
#include "gtm_string.h"
#include "gtm_ctype.h"
#include "gtmmsg.h"
#include "util.h"
#include "mu_upgrd_dngrd_confirmed.h"

#define MAX_RESP_LEN		80

#define CONTINUEMSG "Are you ready to continue the operation [y/n] ? "

/* Asks user for confirmation, before doing the operation.
 * Returns: TRUE if confirmed, FALSE if not confirmed */
boolean_t mu_upgrd_dngrd_confirmed(void)
{
	char		local_str[MAX_RESP_LEN + 1], *resp;
	int		intchar;

#ifdef VMS
	$DESCRIPTOR     (dres, local_str);
	$DESCRIPTOR     (contprm, CONTINUEMSG);
	unsigned short	result_len;
#endif

	util_out_print("!AD", TRUE, LEN_AND_LIT("You must have a backup before you proceed!!"));
	util_out_print("!AD", TRUE, LEN_AND_LIT("An abnormal termination will damage the database during the operation !!"));
#ifdef VMS
	lib$get_input(&dres, &contprm, &result_len);
	local_str[MAX_RESP_LEN < dres.dsc$w_length ? MAX_RESP_LEN : dres.dsc$w_length] = 0;
	resp = local_str;
#else
	util_out_print("!_!_!AD", TRUE, LEN_AND_LIT(CONTINUEMSG));
	FGETS(local_str, MAX_RESP_LEN, stdin, resp);
#endif
	if (NULL == resp)
		return FALSE;
	return ('y' == local_str[0] || 'Y' == local_str[0]);
}
