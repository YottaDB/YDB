/* opp_tcommit.s */

/*
 * enum cdb_sc	op_tcommit(void)
 */

	.title	opp_tcommit.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_tcommit

	.data
.extern	frame_pointer

	.text
.extern	op_tcommit

 
ENTRY opp_tcommit
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_tcommit
	getframe
	bx	lr

.end
