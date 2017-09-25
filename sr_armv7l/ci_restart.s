/* ci_restart.s */
/* (re)start a GT.M frame */
	
/* setup register/stack arguments from 'param_list' and transfer
 *	control to op_extcall/op_extexfun which return only after the M
 *	routine finishes and QUITs.
 */
	
	.title	ci_restart.s
	.sbttl	ci_restart

.include "linkage.si"
.include "stack.si"

.extern	param_list
	
	.text

ci_rtn		=  0
argcnt		=  4
rtnaddr		=  8
labaddr		= 12
retaddr		= 16
mask		= 20
args		= 24

ENTRY ci_restart
	ldr	r4, =param_list
	ldr	r4, [r4]
	mov	fp, sp					/* save sp here - to be restored after op_extexfun or op_extcall */
	ldr	r0, [r4, #argcnt]			/* argcnt */
	ADJ_STACK_ALIGN_EVEN_ARGS r0
	mov	r12, r0
	cmp	r0, #0					/* if (argcnt > 0) { */
	ble	L0
	mov	r1, r0, LSL #2				/* param_list->args[argcnt] */
	sub	r1, #4
	add	r3, r4, #args				/* point at first arg */
	add	r3, r1					/* point at last arg */
L1:
	ldr	r1, [r3], #-4				/* push arguments backwards to stack */
	push	{r1}
	subs	r0, #1
	bne	L1					/* } */
L0:
	push	{r12}					/* push arg count on stack */
	ldr	r3, [r4, #mask]
	ldr	r2, [r4, #retaddr]
	ldr	r1, [r4, #labaddr]
	ldr	r0, [r4, #rtnaddr]
	ldr	r4, [r4, #ci_rtn]
	bx	r4


.end

