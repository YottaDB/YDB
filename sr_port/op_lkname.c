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


#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include <varargs.h>
#include "mlk_force_str.h"
#include "mlk_pvtblk_create.h"
#include "op.h"

GBLREF gd_addr	*gd_header;

void	op_lkname(va_alist)
va_dcl
{
	va_list	var;

	if (!gd_header)
		gvinit();

	VAR_START(var);
	mlk_pvtblk_create(var);
}
