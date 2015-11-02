/****************************************************************
 *                                                              *
 *      Copyright 2007 Fidelity Information Services, Inc        *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef FIX_XFER_ENTRY_INCLUDED
#define FIX_XFER_ENTRY_INCLUDED

#include "mdef.h"

GBLREF xfer_entry_t     xfer_table[];
#ifdef __ia64
GBLREF char* xfer_text[];
int   function_type(char*);
#endif /* __ia64 */

#define C 1
#define ASM 2

#ifndef __ia64
#define FIX_XFER_ENTRY(indx, func) \
{ \
	xfer_table[indx] = (xfer_entry_t)&func; \
}
#else  /* _-ia64 */
#define FIX_XFER_ENTRY(indx, func) \
{ \
        if (function_type(#func) == ASM) \
                xfer_table[indx] = (xfer_entry_t) CODE_ADDRESS_ASM(func);\
        else \
                xfer_table[indx] = (xfer_entry_t) CODE_ADDRESS_C(func);\
	\
                xfer_text[indx] = #func;\
}
#endif  /* _-ia64 */

#endif /* FIX_XFER_ENTRY_INCLUDED */
