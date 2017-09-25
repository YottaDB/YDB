/* mval2mint.s */
/*	Convert mval to mint	*/
/*		on entry: r1 - pointer to mval to convert	*/

	
	.title	mval2mint.s
	
.include "linkage.si"
.include "mval_def.si"
.include "debug.si"

	.sbttl	mval2mint

	.text
.extern	mval2i
.extern	s2n
.extern underr

ENTRY mval2mint
	push	{r6, lr}
	CHKSTKALIGN				/* Verify stack alignment */
	mv_force_defined r1
	mov	r6, r1
	mv_force_num r1
	mov	r0, r6
	bl	mval2i
	cmp	r0, #0
	pop	{r6, pc}


.end
