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
#include "io.h"
#include "iottdef.h"

GBLREF bool write_filter;

void ionl_wteol(short v,io_desc *iod)
{
	mstr	temp;
	short s;
	char ptr[2];

	temp.len = 2;
	ptr[0] = NATIVE_CR;
	ptr[1] = NATIVE_LF;
	temp.addr = ptr;
	iod->esc_state = START;
	for (s = 0; s++ < v ; )
	{
		iod->dollar.x -= 2;
		ionl_write(&temp);
	}
	if (!(write_filter & CHAR_FILTER))
	{
		iod->dollar.x = 0;
		iod->dollar.y += v;
		if (iod->length)
			iod->dollar.y %= iod->length;
	}
	return;
}
