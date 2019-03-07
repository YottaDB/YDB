/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2018 Stephen L Johnson. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	Aarch64 register names  */

#define AARCH64_REG_X0		 0	/* Argument 1/scratch register/result */
#define AARCH64_REG_X1		 1	/* Argument 2/scratch register/result */
#define	AARCH64_REG_X2		 2	/* Argument 3/scratch register/result */
#define	AARCH64_REG_X3		 3	/* Argument 4/scratch register/result */
#define	AARCH64_REG_X4		 4	/* Argument 5/scratch register/result */
#define	AARCH64_REG_X5		 5	/* Argument 6/scratch register/result */
#define	AARCH64_REG_X6		 6	/* Argument 7/scratch register/result */
#define	AARCH64_REG_X7		 7	/* Argument 8/scratch register/result */
#define	AARCH64_REG_X8		 8	/* Register variable */
#define	AARCH64_REG_X9		 9	/* Scratch register */
#define	AARCH64_REG_X10		10	/* Scratch register */
#define	AARCH64_REG_X11		11	/* Scratch register */
#define	AARCH64_REG_X12		12	/* Scratch register */
#define	AARCH64_REG_X13		13	/* Scratch register */
#define	AARCH64_REG_X14		14	/* Scratch register */
#define	AARCH64_REG_X15		15	/* Scratch register */
#define	AARCH64_REG_X16		16	/* Intra-procedure-call */
#define	AARCH64_REG_X17		17	/* Intra-procedure-call */
#define	AARCH64_REG_X18		18	/* Intra-procedure-call */
#define	AARCH64_REG_X19		19	/* preserved by called routine */
#define	AARCH64_REG_X20		20	/* preserved by called routine */
#define	AARCH64_REG_X21		21	/* preserved by called routine */
#define	AARCH64_REG_X22		22	/* preserved by called routine */
#define	AARCH64_REG_X23		23	/* preserved by called routine */
#define	AARCH64_REG_X24		24	/* preserved by called routine */
#define	AARCH64_REG_X25		25	/* preserved by called routine */
#define	AARCH64_REG_X26		26	/* preserved by called routine */
#define	AARCH64_REG_X27		27	/* preserved by called routine */
#define	AARCH64_REG_X28		28	/* preserved by called routine */
#define	AARCH64_REG_X29		29	/* Frame pointer/register variable */
#define	AARCH64_REG_X30		30	/* Link register/scratch register */
#define	AARCH64_REG_X31		31	/* Lower end of current stack frame/Zero register */

#define	AARCH64_REG_FP		29	/* Frame pointer/register variable */
#define	AARCH64_REG_LR		30	/* Link register/scratch register */
#define	AARCH64_REG_SP		31	/* Lower end of current stack frame/Zero register */
#define AARCH64_REG_ZERO	31	/* Hard-wired zero */

/*	Number of arguments passed in registers.  */
#define MACHINE_REG_ARGS	 8
