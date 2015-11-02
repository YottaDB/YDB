/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"

GBLDEF uint4		dollar_tlevel;
GBLDEF uint4		dollar_trestart;
GBLDEF unsigned char	*tpstackbase, *tpstacktop, *tpstackwarn, *tp_sp;
GBLDEF block_id		tp_allocation_clue;
GBLDEF boolean_t	tp_kill_bitmaps;
