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
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "error.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "mu_upgrd.h"

#define NOFLUSH 0
#define FLUSH 1

/*-------------------------------
   Handles mupip upgrade ctrl C
 ________________________________*/
void mu_upgrd_ctrlc_handler(int sig)
{

	error_def(ERR_MUNOUPGRD);

	if (mu_upgrd_confirmed(FALSE))
	{
		util_out_print("Upgrade canceled by user", FLUSH);
		rts_error(VARLSTCNT(1) ERR_MUNOUPGRD);
	}
	util_out_print("Do not interrupt to avoid damage in database", FLUSH);
	util_out_print("Mupip upgrade resumed ...!/", FLUSH);
}


/*-------------------------------------------------------------------------
 Asks user for confirmation, because mupip upgrade may damage the database,
 in case of failure.
  -------------------------------------------------------------------------*/
bool mu_upgrd_confirmed(bool flag)
{
	char str[4], ustr[4];
	char cstr[2][10]={"abort", "continue"};

	util_out_print("-------------------------------------------------------------------------", FLUSH);
	if (flag) util_out_print("You must have a backup before you proceed!!", FLUSH);
	util_out_print("An abnormal termination will damage the database while doing the upgrade !!", FLUSH);
	util_out_print("-------------------------------------------------------------------------", FLUSH);
	util_out_print("Are you ready to !AD?(Yes/No):", FLUSH, LEN_AND_STR(cstr[flag]));
	SCANF("%s",str);
	lower_to_upper((uchar_ptr_t)ustr, (uchar_ptr_t)str, 4);
	if (strncmp(ustr,"YES",3)==0)
		return TRUE;
	else
		return FALSE;
}

