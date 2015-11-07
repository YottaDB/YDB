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

/*	Alpha OpenVMS procedure descriptor values.  */

/*	FLAGS field.  */
#define	PDSC$K_KIND_BOUND		0
#define	PDSC$K_KIND_NULL		8
#define	PDSC$K_KIND_FP_STACK		9
#define	PDSC$K_KIND_FP_REGISTER		10
#define	PDSC$M_HANDLER_VALID		0x10
#define	PDSC$M_HANDLER_REINVOKABLE	0x20
#define	PDSC$M_HANDLER_DATA_VALID	0x40
#define	PDSC$M_BASE_REG_IS_FP		0x80
#define	PDSC$M_REI_RETURN		0x100
#define	PDSC$M_STACK_RETURN_VALUE	0x200
#define	PDSC$M_BASE_FRAME		0x400
#define	PDSC$M_NATIVE			0x1000
#define	PDSC$M_NO_JACKET		0x2000
#define	PDSC$M_TIE_FRAME		0x4000

#define	PDSC$M_FUNC_RETURN		0xF
#define	PDSC$K_NULL_SIZE		16
#define	PDSC$K_BOUND_SIZE		24
#define	PDSC$K_MIN_BOUND_SIZE		24
#define	PDSC$K_MIN_LENGTH_SF		32
#define	PDSC$K_MIN_STACK_SIZE		32
#define	PDSC$K_MAX_STACK_SIZE		48
#define	PDSC$K_MIN_LENGTH_RF		24
#define	PDSC$K_MIN_REGISTER_SIZE	24
#define	PDSC$K_MAX_REGISTER_SIZE	40
#define	PDSC$K_BOUND_ENVIRONMENT_SIZE	32
#define	PDSC$W_FLAGS			0
#define	PDSC$S_KIND			4

/*	FLAGS field.  */
#define	PDSC$V_KIND			0
#define	PDSC$V_HANDLER_VALID		4
#define	PDSC$V_HANDLER_REINVOKABLE	5
#define	PDSC$V_HANDLER_DATA_VALID	6
#define	PDSC$V_BASE_REG_IS_FP		7
#define	PDSC$V_REI_RETURN		8
#define	PDSC$V_STACK_RETURN_VALUE	9
#define	PDSC$V_BASE_FRAME		10
#define	PDSC$V_NATIVE			12
#define	PDSC$V_NO_JACKET		13
#define	PDSC$V_TIE_FRAME		14

#define	PDSC$W_RSA_OFFSET		2
#define	PDSC$B_SAVE_FP			2
#define	PDSC$B_SAVE_RA			3
#define	PDSC$B_ENTRY_RA			4
#define	PDSC$S_FUNC_RETURN		4
#define	PDSC$V_FUNC_RETURN		0
#define	PDSC$W_SIGNATURE_OFFSET		6
#define	PDSC$S_ENTRY			8
#define	PDSC$Q_ENTRY			8
#define	PDSC$L_ENTRY			8
#define	PDSC$L_SIZE			16
#define	PDSC$S_PROC_VALUE		8
#define	PDSC$Q_PROC_VALUE		16
#define	PDSC$L_PROC_VALUE		16
#define	PDSC$S_KIND_SPECIFIC		24
#define	PDSC$R_KIND_SPECIFIC		24
#define	PDSC$L_IREG_MASK		24
#define	PDSC$L_FREG_MASK		28
#define	PDSC$S_STACK_HANDLER		8
#define	PDSC$Q_STACK_HANDLER		32
#define	PDSC$S_STACK_HANDLER_DATA	8
#define	PDSC$Q_STACK_HANDLER_DATA	40
#define	PDSC$S_REG_HANDLER		8
#define	PDSC$Q_REG_HANDLER		24
#define	PDSC$S_REG_HANDLER_DATA		8
#define	PDSC$Q_REG_HANDLER_DATA		32
#define	PDSC$L_ENVIRONMENT		24
#define	PDSC$S_ENVIRONMENT		8
#define	PDSC$Q_ENVIRONMENT		24
#define	PDSC$K_LKP_LENGTH		16
#define	PDSC$S_LKP_ENTRY		8
#define	PDSC$Q_LKP_ENTRY		0
#define	PDSC$PS_LKP_ENTRY		0
#define	PDSC$S_LKP_PROC_VALUE		8
#define	PDSC$Q_LKP_PROC_VALUE		8
#define	PDSC$PS_LKP_PROC_VALUE		8
#define	LKP$K_SIZE			16
#define	LKP$S_ENTRY			8
#define	LKP$Q_ENTRY			0
#define	LKP$PS_ENTRY			0
#define	LKP$S_PROC_VALUE		8
#define	LKP$Q_PROC_VALUE		8
#define	LKP$PS_PROC_VALUE		8
