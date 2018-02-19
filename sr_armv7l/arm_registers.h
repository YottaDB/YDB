/****************************************************************
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2017 Stephen L Johnson. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	Arm register names  */

#define ARM_REG_R0	0	/* Argument 1/scratch register/result */
#define ARM_REG_R1	1	/* Argument 2/scratch register/result */
#define	ARM_REG_R2	2	/* Argument 3/scratch register/result */
#define	ARM_REG_R3	3	/* Argument 4/scratch register/result */
#define	ARM_REG_R4	4	/* Register variable */
#define	ARM_REG_R5	5	/* Register variable */
#define	ARM_REG_R6	6	/* Register variable */
#define	ARM_REG_R7	7	/* Register variable */
#define	ARM_REG_R8	8	/* Register variable */
#define	ARM_REG_R9	9	/* Static base/register variable */
#define	ARM_REG_R10	10	/* Stack limit/stack chunk handle/register variable */
#define	ARM_REG_R11	11	/* Frame pointer/register variable */
#define	ARM_REG_R12	12	/* Scratch register/new -sb in inter-link-unit call */
#define	ARM_REG_R13	13	/* Lower end of current stack frame */
#define	ARM_REG_R14	14	/* Link register/scratch register */
#define	ARM_REG_R15	15	/* Program counter */

#define	ARM_REG_A1	0	/* Argument 1/scratch register/result */
#define	ARM_REG_A2	1	/* Argument 2/scratch register/result */
#define	ARM_REG_A3	2	/* Argument 3/scratch register/result */
#define	ARM_REG_A4	3	/* Argument 4/scratch register/result */
#define	ARM_REG_V1	4	/* Register variable */
#define	ARM_REG_V2	5	/* Register variable */
#define	ARM_REG_V3	6	/* Register variable */
#define	ARM_REG_V4	7	/* Register variable - */
#define	ARM_REG_V5	8	/* Register variable */
#define	ARM_REG_V6	9	/* Static base/register variable */
#define	ARM_REG_V7	10	/* Stack limit/stack chunk handle/register variable */
#define	ARM_REG_V8	11	/* Frame pointer/register variable */

#define	ARM_REG_SB	9	/* Static base/register variable */
#define	ARM_REG_SL	10	/* Stack limit/stack chunk handle/register variable */
#define	ARM_REG_FP	11	/* Frame pointer/register variable */

#define	ARM_REG_IP	12	/* Scratch register/new -sb in inter-link-unit call */
#define	ARM_REG_SP	13	/* Lower end of current stack frame */
#define	ARM_REG_LR	14	/* Link register/scratch register */
#define	ARM_REG_PC	15	/* Program counter */


/*	Number of arguments passed in registers.  */
#define MACHINE_REG_ARGS	4

/*	Number of NEON "d" registers used for vector copy */
#define D_REG_COUNT		8
