/* mint2mval.s */
/*	Convert to int to mval
 *		r0 - pointer to mval to receive value
 *		r1 - int to convert
 */
	
	.title	mint2mval.s
	.sbttl	mint2mval
	
.include "linkage.si"
.include "debug.si"

	.text
 
ENTRY mint2mval
	push	{r4, lr}				/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN					/* Verify stack alignment */
	bl	i2mval
	pop	{r4, pc}


.end

	
