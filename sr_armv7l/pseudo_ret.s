/* pseudo_ret */
/*	PSEUDO_RET calls opp_ret (which doesn't return).  It executes in a
	GT.M MUMPS stack frame and is, in fact, normally entered via a
	getframe/ret instruction sequence. */

	.title	pseudo_ret.s

.include "linkage.si"
.include "debug.si"

	.sbttl	pseudo_ret


ENTRY pseudo_ret
	CHKSTKALIGN					/* Verify stack alignment */
	bl	opp_ret

.end
