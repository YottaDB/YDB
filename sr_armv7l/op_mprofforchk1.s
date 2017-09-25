/* op_mprofforchk1.s */
/*
	Called with arguments
		lr - call return address
*/
	
	.title	op_mprofforchk1.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	op_mprofforchk1

	.data

	.text
.extern	forchkhandler

	
/*
 * This is the M profiling version which calls different routine(s) for M profiling purposes.
 */
ENTRY op_mprofforchk1
	push	{r4, lr}			/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN				/* Verify stack alignment */
	mov	r0, lr
	bl	forchkhandler
	pop	{r4, pc}


.end
