/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "gtm_sizeof.h"

#define rts_error lib$signal
#define error_def(x) globalvalue x
#ifdef DEBUG
error_def(CMI_ASSERT);
#define assert(x) ((x) ? 1 : rts_error(CMI_ASSERT, 3, SIZEOF(__FILE__) - 1, __FILE__, __LINE__))
#else
#define assert(x)
#endif
#define GBLDEF globaldef
#define GBLREF globalref
#define LITDEF readonly globaldef
#define LITREF readonly globalref
typedef char bool;
#define ALIGN_QUAD _align(quadword)
#define TRUE 1
#define FALSE 0

