/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
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

/*	aarch64_gtm_registers.h - GT.M arm register usage
 *
 *	Requires "aarch64_registers.h".
 *
 */


#define GTM_REG_FRAME_POINTER	AARCH64_REG_X19
#define GTM_REG_FRAME_VAR_PTR	AARCH64_REG_X20
#define GTM_REG_FRAME_TMP_PTR	AARCH64_REG_X21
#define GTM_REG_LITERAL_BASE	AARCH64_REG_X22
#define GTM_REG_XFER_TABLE	AARCH64_REG_X23
#define	GTM_REG_PV		AARCH64_REG_X24
#define GTM_REG_DOLLAR_TRUTH    AARCH64_REG_X25

#define	GTM_REG_R0		AARCH64_REG_X0
#define	GTM_REG_R1		AARCH64_REG_X1

#define GTM_REG_ACCUM		AARCH64_REG_X0
#define GTM_REG_COND_CODE	AARCH64_REG_X0
#define GTM_REG_CODEGEN_TEMP	AARCH64_REG_X15
#define GTM_REG_CODEGEN_TEMP_1	AARCH64_REG_X14
#define GTM_REG_ZERO		AARCH64_REG_ZERO

#define CLRL_REG		AARCH64_REG_ZERO
#define CALLS_TINT_TEMP_REG	AARCH64_REG_X9
#define MOVC3_SRC_REG		AARCH64_REG_X9
#define MOVC3_TRG_REG		AARCH64_REG_X10
#define MOVL_RETVAL_REG		AARCH64_REG_X0
#define MOVL_REG_R1		AARCH64_REG_X1
#define CMPL_TEMP_REG		AARCH64_REG_X11
