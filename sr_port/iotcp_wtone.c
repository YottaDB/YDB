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
#include "gtm_stdio.h"
#include "io.h"

void iotcp_wtone(int ch)
{
	mstr	temp;
	char	c;

#ifdef DEBUG_TCP
    PRINTF("%s >>>\n",__FILE__);
#endif
    	c = (char)ch;
	temp.len = 1;
	temp.addr = &c;
	iotcp_write(&temp);
#ifdef DEBUG_TCP
	PRINTF("%s <<<\n",__FILE__);
#endif
	return;
}
