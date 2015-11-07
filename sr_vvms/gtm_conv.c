/****************************************************************
 *                                                              *
 *      Copyright 2006 Fidelity Information Services, Inc 	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "gtm_conv.h"

/* These are dummy routines to allow code to compile/link on VMS. These routines are not
   used but elimination of them from the code is detremental to the clarify of the UNIX
   codebase and since Alpha/VMS is a dead platform, these dummy routines also have a
   limited lifespan.. 9/2006 SE
*/

LITDEF mstr		chset_names[CHSET_MAX_IDX];
GBLDEF UConverter	*chset_desc[CHSET_MAX_IDX];

UConverter* get_chset_desc(const mstr *chset)
{
	GTMASSERT;
}

int gtm_conv(UConverter* from, UConverter* to, mstr* src, char* dstbuff, int* bufflen)
{
	GTMASSERT;
}

int verify_chset(const mstr *parm)
{
	GTMASSERT;
}
