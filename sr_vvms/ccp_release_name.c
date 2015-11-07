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

/* ccp_release_name.c : defines product name and version for licensing

*/
#include "mdef.h"

LITDEF char	ccp_prd_name[] = "GT.CX" ;
LITDEF int4	ccp_prd_len    = SIZEOF(ccp_prd_name) - 1 ;
LITDEF char	ccp_ver_name[] = "V120";
LITDEF int4	ccp_ver_len    = SIZEOF(ccp_ver_name) - 1 ;
