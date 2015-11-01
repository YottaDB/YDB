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
#include "gtm_string.h"
#include "io.h"
#include "iottdef.h"

GBLREF bool write_filter;

void iott_wteol(short v,io_desc *io_ptr)
{
	mstr	temp;
	short s;

	temp.len = strlen(NATIVE_TTEOL);
	temp.addr = (char *)NATIVE_TTEOL;
	io_ptr->esc_state = START;
	for (s = 0; s++ < v ; )
	{
		if (io_ptr->dollar.x > 0)
		{
			UNIX_ONLY(assert(io_ptr->dollar.x >= strlen(NATIVE_TTEOL));)
			io_ptr->dollar.x -= strlen(NATIVE_TTEOL);
		}
		iott_write(&temp);
	}
	if (!(write_filter & CHAR_FILTER))
	{
		io_ptr->dollar.x = 0;
		io_ptr->dollar.y += v;
		if (io_ptr->length)
			io_ptr->dollar.y %= io_ptr->length;
	}
	return;
}
