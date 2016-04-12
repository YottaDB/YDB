/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __FGNCAL_H__
#define __FGNCAL_H__

mval *fgncal_lookup(mval *x);
void fgncal_unwind(void);
void fgncal_rundown(void);

#include "fgncalsp.h"

/* Checks whether the last bit of the passed mask M is 1. This is how we decide whether an argument is of input-only, input-output,
 * or output-only type. To be specific, if an argument is of input-only type, its bit will be set in the input mask but not the
 * output mask; if it is output-only, then the bit is only set in the output mask; finally, if it is input-output, then its bit is
 * set in the both masks. Note that after checking the mask bit for one argument, the mask needs to be binary shifted, such that the
 * last bit contains the status of the next argument, and so on.
 */
#define MASK_BIT_ON(M)	(M & 1)

/* Checks whether V is defined and marked for use in a specific input/output direction, depending on whether M is an input or output
 * mask (see the comment for MASK_BIT_ON), which is what determines if the actual or default value should be used.
 */
#define MV_ON(M, V)	(MASK_BIT_ON(M) && MV_DEFINED(V))

#endif
