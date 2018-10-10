/****************************************************************
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

#define CODEBUF_TYPE	char
#define CALL_SIZE	20
#define EXTRA_INST	0
#define EXTRA_INST_SIZE	0

/* func has values like:
 * func_ptr1 = 0xffffb793e934 <dm_setup>, func_ptr2 = 0xffffb75d04c8 <mum_tstart>, func_ptr3 = 0xffffb75cd3b8 <opp_ret>
 */
#define GEN_CALL(func)  {											\
    assert(SIZEOF(*code) == SIZEOF(char));									\
    /* mov x0,#func[15:16] */											\
    *((intptr_t *)code) = (AARCH64_INS_MOV_IMM									\
				| AARCH64_64_BIT_OP								\
				| ((((intptr_t)(unsigned char *)func & 0x000000000000ffff)) << 5));		\
    code += INST_SIZE;												\
    /* movk x0, #func[31:16] LSL 16 */										\
    *((intptr_t *)code) = (AARCH64_INS_MOVK									\
				| AARCH64_64_BIT_OP								\
				| AARCH64_HW_SHIFT_16								\
				| ((((intptr_t)(unsigned char *)func & 0x00000000ffff0000) >> 16) << 5));	\
    code += INST_SIZE;												\
    /* movk x0, #func[47:16]  LSL 32 */	       									\
    *((intptr_t *)code) = (AARCH64_INS_MOVK									\
				| AARCH64_64_BIT_OP								\
				| AARCH64_HW_SHIFT_32								\
				| ((((intptr_t)(unsigned char *)func & 0x0000ffff00000000) >> 32) << 5));	\
     code += INST_SIZE;												\
    /* movk x0, #func[63:16]  LSL 48 */										\
    *((intptr_t *)code) = (AARCH64_INS_MOVK									\
				| AARCH64_64_BIT_OP								\
				| AARCH64_HW_SHIFT_48								\
				| ((((intptr_t)(unsigned char *)func & 0xffff000000000000) >> 48) << 5));	\
     code += INST_SIZE;												\
    /* blr x0 */												\
    *((intptr_t *)code) = (intptr_t)(unsigned char *)AARCH64_INS_BLR;						\
    code += INST_SIZE;												\
}
