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

#include "mdef.h"

#include "gtm_string.h"

#include "urx.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include <auto_zlink.h>

GBLREF unsigned char    *obpt;          /* output buffer index */
GBLREF stack_frame	*frame_pointer;

IA64_ONLY(GBLREF uint8 		imm14;)
IA64_ONLY(GBLREF char 		asm_mode;)

/* Due to the complex instruction set of x86_64, we require a function to implement the macro VALID_CALLING_SEQUENCE
   and this function will calculate both the offsets (rtnhdr & labaddr) and store them into these global variables */
#if defined(__x86_64__) || defined(__MVS__) || defined(Linux390)
GBLDEF int4		rtnhdr_off;
GBLDEF int4		labaddr_off;
#endif

rhdtyp	*auto_zlink(mach_inst *pc, lnr_tabent ***line)
{
	lnk_tabent	*A_rtnhdr;
	lnk_tabent	*A_labaddr;
	mstr		rname;
	mident_fixed	rname_buff;
	mval		rtn;
	rhdtyp		*rhead;
	urx_rtnref	*rtnurx;

	error_def	(ERR_LABELUNKNOWN);
	error_def	(ERR_ROUTINEUNKNOWN);

/* (ASSUMPTION)
 * The instructions immediately preceding the current mpc form a transfer table call.
 *	There will be two arguments to this call:
 *		the address of a routine header and
 *		the address of a pointer to a value which is the offset from the beginning of the routine header
 *			to the address of the first instruction to be executed in the routine to be called, as
 *			specified by the [label][+offset] field (or the beginning of the routine if label and
 *			offset are not specified)
 *	We compute the linkage table offsets of both parameters by disassembling the load instructions that were
 *	generated to load arguments. The entire instruction sequence is platform dependent (see auto_zlink_sp.h).
 *
 *	N.B., the instruction sequence above occurs in compiled object modules; in direct mode, the argument
 *	registers are loaded via "load" instructions.  However, this routine should never be invoked from direct
 *	mode because the applicable routine should have been ZLINK'ed by prior calls to op_labaddr and op_rhdaddr.
 */

	NON_IA64_ONLY(if (!VALID_CALLING_SEQUENCE(pc)))
		NON_IA64_ONLY(GTMASSERT;)

	/* Calling sequence O.K.; get address(address(routine header)) and address(address(label offset)).  */
#	ifdef __ia64 /* __ia64 */
	{
		uint8 imm;
		int8 *buf2, i;
		ia64_bundle  bundle;
		uint8 dummy, dummy2, dummy1;
		int isMovl = 0;

		RTNHDR_PV_OFF(pc,imm);
		A_rtnhdr  = (lnk_tabent *)(imm + frame_pointer->ctxt);
		dummy1 = dummy; /* For silly compiler warning */
		LABADDR_PV_OFF(pc, imm);
		A_labaddr = (lnk_tabent *)(imm + frame_pointer->ctxt);
	}
#	endif /* __ia64 */

        NON_IA64_ONLY(A_rtnhdr  = (lnk_tabent *)(RTNHDR_PV_OFF(pc) + frame_pointer->ctxt);)
        NON_IA64_ONLY(A_labaddr = (lnk_tabent *)(LABADDR_PV_OFF(pc) + frame_pointer->ctxt);)

	if (azl_geturxrtn((char *)A_rtnhdr, &rname, &rtnurx))
	{
		assert(rname.len <= MAX_MIDENT_LEN);
		assert(0 != rname.addr);

		/* Copy rname into local storage because azl_geturxrtn sets rname.addr to an address that is
		 * free'd during op_zlink and before the call to find_rtn_hdr.
		 */
                memcpy(rname_buff.c, rname.addr, rname.len);
                rname.addr = rname_buff.c;

		assert(0 != rtnurx);
		assert(azl_geturxlab((char *)A_labaddr, rtnurx));
		assert(0 == find_rtn_hdr(&rname));
		rtn.mvtype = MV_STR;
		rtn.str.len = rname.len;
		rtn.str.addr = rname.addr;
		op_zlink(&rtn, 0);
		if (0 != (rhead = find_rtn_hdr(&rname)))
		{	/* Pull the linkage table reference out and return it to caller */
			*line = (lnr_tabent **)(A_labaddr->ext_ref);
			if (0 == *line)
				rts_error(VARLSTCNT(1) ERR_LABELUNKNOWN);
			return rhead;
		}
	}
	rts_error(VARLSTCNT(1) ERR_ROUTINEUNKNOWN);
	return NULL;	/* Compiler happiness phase */
}
