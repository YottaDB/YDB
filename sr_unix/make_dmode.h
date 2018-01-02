/****************************************************************
 *								*
 * Copyright 2003, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MAKE_DMODE_INCLUDED
#define MAKE_DMODE_INCLUDED

rhdtyp *make_dmode(void);

#define DM_MODE 0

#define	CODE_LINES	3
#define	CODE_SIZE	(CODE_LINES * CALL_SIZE + SIZEOF(uint4) * EXTRA_INST)

#include "make_dmode_sp.h"

#endif
