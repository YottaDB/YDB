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

GBLDEF short		dollar_tlevel;
GBLDEF unsigned short	dollar_trestart;
GBLDEF unsigned char	*tpstackbase, *tpstacktop, *tpstackwarn, *tp_sp;
GBLDEF int		tp_allocation_clue;
GBLDEF bool		tp_kill_bitmaps;
