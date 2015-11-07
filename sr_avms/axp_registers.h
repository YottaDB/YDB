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

/*	axp_registers.h - OpenVMS AXP register usage.  */


/*	Register names according to OpenVMS usage.  */

#define ALPHA_REG_R0	0	/* corresponds to VAX register r0 */
#define ALPHA_REG_R1	1	/* corresponds to VAX register r1 */
#define	ALPHA_REG_S0	2	/* saved */
#define	ALPHA_REG_S1	3
#define	ALPHA_REG_S2	4
#define	ALPHA_REG_S3	5
#define	ALPHA_REG_S4	6
#define	ALPHA_REG_S5	7
#define	ALPHA_REG_S6	8
#define	ALPHA_REG_S7	9
#define	ALPHA_REG_S8	10
#define	ALPHA_REG_S9	11
#define	ALPHA_REG_S10	12
#define	ALPHA_REG_S11	13
#define	ALPHA_REG_S12	14
#define	ALPHA_REG_S13	15
#define	ALPHA_REG_A0	16	/* argument */
#define	ALPHA_REG_A1	17
#define	ALPHA_REG_A2	18
#define	ALPHA_REG_A3	19
#define	ALPHA_REG_A4	20
#define	ALPHA_REG_A5	21
#define	ALPHA_REG_T0	22	/* temp */
#define	ALPHA_REG_T1	23
#define	ALPHA_REG_T2	24
#define	ALPHA_REG_AI	25	/* arg info */
#define	ALPHA_REG_RA	26	/* return address */
#define	ALPHA_REG_PV	27	/* procedure value */
#define	ALPHA_REG_VS	28	/* volatile scratch */
#define	ALPHA_REG_FP	29	/* frame pointer */
#define	ALPHA_REG_SP	30	/* stack pointer */
#define ALPHA_REG_ZERO	31	/* hard-wired zero */

#define	ALPHA_REG_V0	ALPHA_REG_R0	/* function return value */


/*	Number of arguments passed in registers.  */

#define MACHINE_REG_ARGS	6

