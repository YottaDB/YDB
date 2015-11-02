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

#include "mdef.h"

#include "gtm_string.h"

#include "opcode.h"
#include "compiler.h"
#include "mmemory.h"

/* Create triple size operand reference. Used by op_exfun and its mprof counterpart.
   Similar to OCNT_REF in that its value is used by op_exfun to increment the return address
   which is stored in the new M stack frame that is built for the local routine being called.
   This sets the return point for that routine. Then op_exfun returns to the normal return address
   which is branch instruction sequence to the actual subroutine. When that subroutine completes,
   the return point comes back to the point just past the branch sequence. This operand lets us
   get the size of the branch to the subroutine so op_exfun can properly increment the return
   address. This creates the triple and initializes it but the address of the triple must still
   be set into it at a later point.

   Note the #ifdef is needed because this triple is not defined on unshared binary platforms
*/
#if defined(USHBIN_SUPPORTED) || defined(VMS)
oprtype put_tsiz(void)
{
	triple 		*ref;
	tripsize	*tsize;

	ref = newtriple(OC_TRIPSIZE);
	ref->operand[0].oprclass = TSIZ_REF;
	ref->operand[0].oprval.tsize = tsize = (tripsize *)mcalloc(SIZEOF(tripsize));
	tsize->ct = NULL;
	tsize->size = 0;
	return put_tref(ref);
}
#endif
