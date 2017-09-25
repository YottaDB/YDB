/* mval2num.s */
/*	Convert mval to numeric */
/*		on entry: r1 - pointer to mval to convert	*/
	
	.title	mval2num.s

.include "linkage.si"
.include "mval_def.si"
.include "debug.si"

	.sbttl	mval2num
	
	.text
.extern	n2s
.extern	s2n
.extern underr

ENTRY mval2num
	push	{r6, lr}
	CHKSTKALIGN					/* Verify stack alignment */
	mv_force_defined r1
	mov	r6, r1
	mv_force_num r1
	mov	r1, r6
	mv_force_str_if_num_approx r1
	pop	{r6, pc}

.end
