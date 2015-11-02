/****************************************************************
 *                                                              *
 *      Copyright 2007, 2008 Fidelity Information Services, Inc	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef FIX_XFER_ENTRY_INCLUDED
#define FIX_XFER_ENTRY_INCLUDED

GBLREF xfer_entry_t     xfer_table[];

#ifdef __ia64
GBLREF char     	xfer_table_desc[];
#endif /* __ia64 */

#if defined(__ia64) || defined(__x86_64__)
#include "xfer_desc.i"
#endif

#ifndef __ia64
#define FIX_XFER_ENTRY(indx, func) 				\
{ 								\
	xfer_table[indx] = (xfer_entry_t)&func; 		\
}
#else  /* __ia64 */
#define FIX_XFER_ENTRY(indx, func) 				\
{ 								\
        xfer_table[indx] = (xfer_entry_t)CODE_ADDRESS(func);	\
	xfer_table_desc[indx] = func##_FUNCTYPE;  		\
}

#endif  /* __ia64 */

#endif /* FIX_XFER_ENTRY_INCLUDED */
