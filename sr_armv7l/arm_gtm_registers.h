/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
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

/*	arm_gtm_registers.h - GT.M arm register usage
 *
 *	Requires "arm_registers.h".
 *
 */


#define GTM_REG_FRAME_POINTER	ARM_REG_R5
#define GTM_REG_FRAME_VAR_PTR	ARM_REG_R10
#define GTM_REG_FRAME_TMP_PTR	ARM_REG_R9
#define GTM_REG_LITERAL_BASE	ARM_REG_R8
#define GTM_REG_XFER_TABLE	ARM_REG_R7

#define	GTM_REG_R0		ARM_REG_R0
#define	GTM_REG_R1		ARM_REG_R1

#define GTM_REG_ACCUM		ARM_REG_R0
#define GTM_REG_COND_CODE	ARM_REG_R0
#define GTM_REG_CODEGEN_TEMP	ARM_REG_R12
#define GTM_REG_CODEGEN_TEMP_1	ARM_REG_R4

#define	GTM_REG_PV		ARM_REG_R6
