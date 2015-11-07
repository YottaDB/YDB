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

/* la_exit.c : returns RMS$_EOF to terminate license adm or license man
   used in   : license_adm.c license_man.c
 */

#include "mdef.h"
#include <rmsdef.h>
int la_exit()
{
	return(RMS$_EOF) ;
}
