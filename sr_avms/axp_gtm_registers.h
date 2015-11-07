/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	axp_gtm_registers.h - GT.M OpenVMS AXP register usage.
 *
 *	Requires "axp_registers.h".
 *
 */


#define GTM_REG_FRAME_POINTER	ALPHA_REG_S10		/* r12 */
#define GTM_REG_FRAME_VAR_PTR	ALPHA_REG_S6		/* r8  */
#define GTM_REG_FRAME_TMP_PTR	ALPHA_REG_S7		/* r9  */
#define GTM_REG_LITERAL_BASE	ALPHA_REG_S12		/* r14 */
#define GTM_REG_XFER_TABLE	ALPHA_REG_S9		/* r11 */
#define GTM_REG_DOLLAR_TRUTH	ALPHA_REG_S8		/* r10 */

#define	GTM_REG_R0		ALPHA_REG_R0
#define	GTM_REG_R1		ALPHA_REG_R1

#define GTM_REG_ACCUM		ALPHA_REG_T1		/* r23 */
#define GTM_REG_COND_CODE	ALPHA_REG_T2		/* r24 */
#define GTM_REG_CODEGEN_TEMP	ALPHA_REG_VS		/* r28 */


/*	The generated code uses GTM_REG_PV for its PV.  Also, the VAX Macro
 *	translator uses S11 (R13) to save the current PV.  So, for consistency,
 *	we make the GTM PV the same (S11).  In addition, when returning from
 *	C, we usually reload the PV of the procedure to which we are returning
 *	into S11 where it is expected (from the Mumps stack frame).  In some
 *	routines (OPP_RET, for example), we intend to "return" to a new procedure
 *	and enter it via its prolog: in that case, we need to set the PV into
 *	R27, the Alpha PV register.
 */

#define	GTM_REG_PV		ALPHA_REG_S11		/* r13 */
