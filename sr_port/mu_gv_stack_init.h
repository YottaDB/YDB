/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_GV_STACK_DEFINED
#define  MU_GV_STACK_DEFINED

#include "obj_file.h"		/* Needed for SECTION_ALIGN_BOUNDARY */

#define UTIL_BASE_FRAME_NAME	"UTIL.BASE.FRAME"
#define UTIL_BASE_LABEL_NAME	"running"
#define UTIL_BASE_FRAME_CODE	"N/A"
#define RLEN			SECTION_ALIGN_BOUNDARY	/* "Routine" length (pseudo not-so-executable) maintains alignment */

void mu_gv_stack_init(void);

#endif

