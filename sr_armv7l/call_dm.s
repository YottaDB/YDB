/* call_dm.s */
	
/* call_dm - call direct mode
 *	CALL_DM controls execution of GT.M direct mode.  It executes in a
 *	GT.M MUMPS stack frame and is, in fact, normally entered via a
 *	getframe/ret instruction sequence.  CALL_DM invokes OPP_DMODE
 *	for each input line.
 */
	
	.title	call_dm.s
	.sbttl	call_dm
	
.include "linkage.si"
.include "debug.si"

	.text
.extern opp_dmode
.extern op_oldvar


ENTRY call_dm
newcmd:
	CHKSTKALIGN			/* Verify stack alignment */
	bl	opp_dmode
	bl	op_oldvar
	b	newcmd


.end
