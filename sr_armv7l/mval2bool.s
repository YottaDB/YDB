/* mval2bool.s */
/*	Convert mval to bool */
/*		on entry: r1 - pointer to mval to convert	*/

	
	.title	mval2bool.s

.include "linkage.si"
.include "mval_def.si"
.include "debug.si"
	
	.sbttl	mval2bool
	
	.text
.extern	s2n
.extern underr

ENTRY mval2bool
	push	{r6, lr}
	CHKSTKALIGN				/* Verify stack alignment */
	mv_force_defined r1
	mov	r6, r1
	mv_force_num r1
	mov	r1, r6
	ldr	r0, [r1, #+mval_l_m1]
	cmp	r0, #0
	pop	{r6, pc}
	
	
.end
