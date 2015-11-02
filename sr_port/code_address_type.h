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

#ifndef CODE_ADDRESS_TYPE_INCLUDE
#define CODE_ADDRESS_TYPE_INCLUDE

#include "mdef.h"

#ifdef __ia64
GBLREF int   function_type(char*);
#endif /* __ia64 */

#define C 1
#define ASM 2

#ifndef __ia64
#define CODE_ADDRESS_TYPE(func) &func
#else  /* _-ia64 */
#define CODE_ADDRESS_TYPE(func) ((function_type(#func) == ASM)? CODE_ADDRESS_ASM(func) : CODE_ADDRESS_C(func) )
#endif  /* _-ia64 */

#endif /* CODE_ADDRESS_TYPE_INCLUDE */
