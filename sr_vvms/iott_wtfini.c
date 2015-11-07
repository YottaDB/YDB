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
#include "gtm_wake.h"

void iott_wtfini(d_tt_struct *tt_ptr)
{
	tt_ptr->io_pending = tt_ptr->sb_pending->start_addr +
					tt_ptr->sb_pending->iosb_val.char_ct;
	if (tt_ptr->io_pending == tt_ptr->io_buftop)
		tt_ptr->io_pending = tt_ptr->io_buffer;
	tt_ptr->sb_pending++;
	if (tt_ptr->sb_pending == tt_ptr->sb_buftop)
		tt_ptr->sb_pending = tt_ptr->sb_buffer;
	gtm_wake(0,0);
	return;
}

