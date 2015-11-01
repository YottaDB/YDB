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
#include "io_params.h"

/* io_params_size contains a table of argument sizes for each io_param	 */
/* enumerated in io_params. 				      		 */

#define IOP_DESC(a,b,c,d,e) c

LITDEF unsigned char io_params_size[] =
{
#include "iop.h"
};
