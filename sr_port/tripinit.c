/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "mmemory.h"
#include "compiler.h"
#include "mdq.h"
#include "hashtab_str.h"

GBLREF mcalloc_hdr		*mcavailptr, *mcavailbase;
GBLREF int			mcavail;
GBLREF int			mvmax, mlmax, mlitmax;
GBLREF mlabel			*mlabtab;
GBLREF mline			*mline_tail;
GBLREF mline			mline_root;
GBLREF mliteral			literal_chain;
GBLREF mvar			*mvartab;
GBLREF mvax			*mvaxtab,*mvaxtab_end;
GBLREF short int		block_level;
GBLREF triple			t_orig;

void tripinit(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	COMPILE_HASHTAB_CLEANUP;
	if (!mcavailbase)
	{
		mcavailbase = (mcalloc_hdr *)malloc(MC_DSBLKSIZE);
		mcavailbase->link = NULL;
		mcavailbase->size = (int4)(MC_DSBLKSIZE - MCALLOC_HDR_SZ);
	}
	mcavailptr = mcavailbase;
	mcavail = mcavailptr->size;
	memset(&mcavailptr->data[0], 0, mcavail);
	TREF(expr_depth) = 0;
	TREF(expr_start) = TREF(expr_start_orig) = NULL;
	TREF(saw_side_effect) = TREF(shift_side_effects) = FALSE;
	mlitmax = mlmax = mvmax = 0;
	mlabtab = NULL;
	mvartab = NULL;
	mvaxtab = mvaxtab_end = NULL;
	dqinit(&t_orig,exorder);
	dqinit(&(t_orig.backptr), que);
	dqinit(&literal_chain, que);
	memset(&mline_root, 0, SIZEOF(mline_root));
	mline_tail = & mline_root;
	block_level = -1;
	setcurtchain(&t_orig);
	return;
}
