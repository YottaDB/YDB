/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MAKE_MODE_INCLUDED
#define MAKE_MODE_INCLUDED

rhdtyp *make_mode (int mode_index);

#define DM_MODE 0
#define CI_MODE 1

#define	CODE_LINES	3
#define	CODE_SIZE	(CODE_LINES * CALL_SIZE + sizeof(uint4) * EXTRA_INST)

#include "make_mode_sp.h"

#endif
