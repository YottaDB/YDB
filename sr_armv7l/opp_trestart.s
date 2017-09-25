/* opp_trestart.s */

/*
 * void	op_trestart_set_cdb_code(void)
 */

	.title	opp_trestart.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_trestart

	.data
.extern	frame_pointer

	.text
.extern	op_trestart

 
ENTRY opp_trestart
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_trestart
	getframe
	bx	lr


.end
