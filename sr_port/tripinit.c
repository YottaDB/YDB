/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

GBLDEF mvar *mvartab;
GBLDEF mvax *mvaxtab,*mvaxtab_end;
GBLDEF mlabel *mlabtab;
GBLREF mliteral literal_chain;
GBLDEF mline mline_root;
GBLDEF mline *mline_tail;
GBLDEF short int block_level;
GBLDEF triple t_orig;
GBLDEF int mvmax, mlmax, mlitmax;

GBLREF bool run_time;
GBLREF int mcavail;
GBLREF char **mcavailptr, **mcavailbase;
GBLREF unsigned short int expr_depth;
GBLREF triple *expr_start, *expr_start_orig;
GBLREF bool shift_gvrefs;

void tripinit(void)
{
	if (!mcavailbase)
	{
		mcavailbase = (char **)malloc(MC_DSBLKSIZE);
		*mcavailbase = 0;
	}
	mcavailptr = mcavailbase;
	/* Use of char_ptr_t here is so we get the correct alignment. On some platforms,
	   this is an 8 byte pointer giving us 8 byte alignment (e.g. Tru64). This behavior
	   is echoed in mcalloc().
	*/
	mcavail = MC_DSBLKSIZE - sizeof(char_ptr_t);
	memset(((char *)mcavailptr) + sizeof(char_ptr_t), 0, mcavail);
	expr_depth = 0;
	expr_start = expr_start_orig = 0;
	shift_gvrefs = FALSE;
	mlitmax = mlmax = mvmax = 0;
	mlabtab = 0;
	mvartab = 0;
	mvaxtab = mvaxtab_end = 0;
	dqinit(&t_orig,exorder);
	dqinit(&(t_orig.backptr), que);
	dqinit(&literal_chain, que);
	memset(&mline_root, 0, sizeof(mline_root));
	mline_tail = & mline_root;
	block_level = -1;
	setcurtchain(&t_orig);
	return;
}
