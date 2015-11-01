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
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"

/* Include prototypes */
#include "bit_set.h"

uint4 bml_free(uint4 setfree, sm_uc_ptr_t map)
{
	uint4 wasfree, ret;

	setfree *= BML_BITS_PER_BLK;
	wasfree = 0;

	ret = bit_set(setfree, map);
	if (!ret)
		wasfree = bit_set(setfree + 1, map);

	assert(!wasfree);	/* until 10 becomes a legal bm value, this must be false */
	return ret;
}
