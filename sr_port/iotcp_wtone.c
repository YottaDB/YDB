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
#include "io.h"

void iotcp_wtone(unsigned char ch)
{
	mstr	temp;

#ifdef DEBUG_TCP
    PRINTF("%s >>>\n",__FILE__);
#endif
	temp.len = 1;
	temp.addr = (char *)&ch;
	iotcp_write(&temp);
#ifdef DEBUG_TCP
    PRINTF("%s <<<\n",__FILE__);
#endif
	return;
}
