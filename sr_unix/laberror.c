/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include <rtnhdr.h>
#include "stack_frame.h"

GBLREF stack_frame	*frame_pointer;

error_def(ERR_LABELMISSING);

/* Add simple declaration to suppress warning - routine only used by assembler routines so no need for
 * definition in header file.
 */
void laberror(int lblindx);

/* Routine that allows assembler routines to more easily put out the LABELMISSING error message with the
 * appropriate label name argument.
 *
 * Argument:
 *
 *   lblindx - index into linkage_adr and linkage_names
 */
void laberror(int lblindx)
{
	mstr	lblname;
	int	skiplen;
	char	*cptr, *maxcptr;

#	ifdef AUTORELINK_SUPPORTED
	assertpro(0 <= lblindx);
	assert(lblindx <= frame_pointer->rvector->linkage_len);
	lblname = frame_pointer->rvector->linkage_names[lblindx];		/* Make copy of possibly shared mstr */
	lblname.addr += (INTPTR_T)frame_pointer->rvector->literal_text_adr;	/* Relocate addr appropriately */
	/* Label name is in form of "rtnname.labelname" so forward space past routine name and '.' */
	maxcptr = lblname.addr + lblname.len;
	for (cptr = lblname.addr; ('.' != *cptr) && (cptr < maxcptr); cptr++)
		;
	assert('.' == *cptr);
	skiplen = cptr - lblname.addr + 1;					/* + 1 to skip past '.' separator */
	assert(skiplen < lblname.len);
	lblname.len -= skiplen;
	lblname.addr += skiplen;
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_LABELMISSING, 2, RTS_ERROR_MSTR(&lblname));
#	else
	assertpro(FALSE);
#	endif
	return;
}
