/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include "lv_val.h"	/* needed for "callg.h" */
#include "callg.h"

#define VAR_ARGS32(ar)	ar[0], ar[1], ar[2], ar[3], ar[4], ar[5], ar[6], ar[7], ar[8], ar[9], ar[10], ar[11], ar[12],	\
			ar[13], ar[14], ar[15], ar[16], ar[17], ar[18], ar[19], ar[20], ar[21], ar[22], ar[23], ar[24],	\
			ar[25], ar[26], ar[27], ar[28], ar[29], ar[30], ar[31]

#define VAR_ARGS28(ar)	ar[0], ar[1], ar[2], ar[3], ar[4], ar[5], ar[6], ar[7], ar[8], ar[9], ar[10], ar[11], ar[12],	\
			ar[13], ar[14], ar[15], ar[16], ar[17], ar[18], ar[19], ar[20], ar[21], ar[22], ar[23], ar[24],	\
			ar[25], ar[26], ar[27]

#define VAR_ARGS24(ar)	ar[0], ar[1], ar[2], ar[3], ar[4], ar[5], ar[6], ar[7], ar[8], ar[9], ar[10], ar[11], ar[12],	\
			ar[13], ar[14], ar[15], ar[16], ar[17], ar[18], ar[19], ar[20], ar[21], ar[22], ar[23]

#define VAR_ARGS20(ar)	ar[0], ar[1], ar[2], ar[3], ar[4], ar[5], ar[6], ar[7], ar[8], ar[9], ar[10], ar[11], ar[12],	\
			ar[13], ar[14], ar[15], ar[16], ar[17], ar[18], ar[19]

#define VAR_ARGS16(ar)	ar[0], ar[1], ar[2], ar[3], ar[4], ar[5], ar[6], ar[7], ar[8], ar[9], ar[10], ar[11], ar[12],	\
			ar[13], ar[14], ar[15]

#define VAR_ARGS12(ar)	ar[0], ar[1], ar[2], ar[3], ar[4], ar[5], ar[6], ar[7], ar[8], ar[9], ar[10], ar[11]

#define VAR_ARGS8(ar)	ar[0], ar[1], ar[2], ar[3], ar[4], ar[5], ar[6], ar[7]

#define VAR_ARGS4(ar)	ar[0], ar[1], ar[2], ar[3]

/* Note selection of doing 4 parms per case block is based on the fact that Itanium can do at most 4 parm loads
   at one time due to instruction bundle restrictions and the fact that most calls made are 4 or fewer parms.

   NOTE: Although this module can be used on other platforms it is efficient **ONLY** on Itanium in its present
         form and is not suggested for use on other platforms.
*/

INTPTR_T callg(callgfnptr fnptr, gparam_list *paramlist)
{
	assert(32 == (SIZEOF(paramlist->arg) / SIZEOF(void *)));
	switch(paramlist->n)
	{
		case 0:
			return (fnptr)(0);
		case 1:
		case 2:
		case 3:
		case 4:
			return (fnptr)(paramlist->n, VAR_ARGS4(paramlist->arg));
		case 5:
		case 6:
		case 7:
		case 8:
			return (fnptr)(paramlist->n, VAR_ARGS8(paramlist->arg));
		case 9:
		case 10:
		case 11:
		case 12:
			return (fnptr)(paramlist->n, VAR_ARGS12(paramlist->arg));
		case 13:
		case 14:
		case 15:
		case 16:
			return (fnptr)(paramlist->n, VAR_ARGS16(paramlist->arg));
		case 17:
		case 18:
		case 19:
		case 20:
			return (fnptr)(paramlist->n, VAR_ARGS20(paramlist->arg));
		case 21:
		case 22:
		case 23:
		case 24:
			return (fnptr)(paramlist->n, VAR_ARGS24(paramlist->arg));
		case 25:
		case 26:
		case 27:
		case 28:
			return (fnptr)(paramlist->n, VAR_ARGS28(paramlist->arg));
		case 29:
		case 30:
		case 31:
		case 32:
			return (fnptr)(paramlist->n, VAR_ARGS32(paramlist->arg));
		default:
			GTMASSERT;
	}
	return 0;
}
