/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
GBLDEF uint4		bml_save_dollar_tlevel; /* if non-zero holds actual dollar_tlevel */
GBLDEF uint4		dollar_trestart;
GBLDEF unsigned char	*tpstackbase, *tpstacktop, *tpstackwarn, *tp_sp;
GBLDEF block_id		tp_allocation_clue;
GBLDEF boolean_t	tp_kill_bitmaps;
