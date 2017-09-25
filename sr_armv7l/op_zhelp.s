/* op_zhelp.s */
	
	.title	op_zhelp.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl op_zhelp.s

	.data
.extern	frame_pointer

	.text
.extern	op_zhelp_xfr
	
 
ENTRY op_zhelp
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]
	bl	op_zhelp_xfr
	getframe
	bx	lr

.end
