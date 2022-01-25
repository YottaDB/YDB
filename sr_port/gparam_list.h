/****************************************************************
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Portions Copyright (c) 2001-2019 Fidelity National		*
 * Information Services, Inc. and/or its subsidiaries.		*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************
 * While FIS did not write this module, it was derived from FIS code in "sr_port/callg.h"
 * and "sr_port/mdef.h". Moved to this header file to support wrapper access to these fields.
 */

#ifndef _GPARAM_LIST_DEFINED
#define _GPARAM_LIST_DEFINED

#define PUSH_PARM_OVERHEAD	4	/* This extra space in the array is needed because push_parm() is capable of handling 32
					 * arguments, but callg needs to accomidate space for 4 items, namely argument count
					 * (repeated twice), return value, and the truth value. As of this writing ojchildparms.c is
					 * the only module that relies on the extra space.
					 */
#define	MAX_ACTUALS		32	/* Maximum number of arguments allowed in an actuallist. This value also determines
					 * how many parameters are allowed to be passed between M and C.
					 */
#define MAX_GPARAM_LIST_ARGS	(MAX_ACTUALS + PUSH_PARM_OVERHEAD) /* Max args in gparam_list structure */

typedef struct gparam_list_struct
{
	intptr_t	n;				/* Count of parameter/arguments */
	void    	*arg[MAX_GPARAM_LIST_ARGS];	/* Parameter/argument array */
} gparam_list;

#endif
