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

#include "mdef.h"
#include <syidef.h>
#include <ssdef.h>
#include <efndef.h>


#include "vmsdtype.h"
#include "send_msg.h"
#include "set_num_additional_processors.h"

GBLREF int	num_additional_processors;

void	set_num_additional_processors(void)
{
	struct
	{
		item_list_3	item[1];
		int4		terminator;
	}		item_list;
	unsigned short	iosb[4];
	uint4	mode, status, dummy, numcpus = 1;
	error_def(ERR_NUMPROCESSORS);

	item_list.item[0].buffer_length		= SIZEOF(numcpus);
	item_list.item[0].item_code		= SYI$_ACTIVECPU_CNT;
	item_list.item[0].buffer_address	= &numcpus;
	item_list.item[0].return_length_address	= &dummy;

	item_list.terminator = 0;

	if ((status = sys$getsyiw(EFN$C_ENF, NULL, NULL, &item_list, iosb, NULL, 0)) != SS$_NORMAL  ||
	    (status = iosb[0]) != SS$_NORMAL)
	{
		numcpus = 1;
		send_msg(VARLSTCNT(3) ERR_NUMPROCESSORS, 0, status);
	}

	num_additional_processors = numcpus - 1;
}
