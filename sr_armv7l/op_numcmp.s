/* op_numcmp.s */
/*
	op_numcmp calls numcmp to compare two mvals

	entry:
		r0	mval *u
		r1	mval *v

	exit:
		condition codes set according to value of
			numcmp (u, v)
*/

	.title	op_numcmp.s
	
	.sbttl	op_numcmp
	
.include "linkage.si"
.include "debug.si"

	.text
.extern	numcmp
	

ENTRY op_numcmp
	push	{r4, lr}				/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN					/* Verify stack alignment */
	bl	numcmp
	cmp	r0, #0					/* set flags according to result from numcmp */
	pop	{r4, pc}


.end
