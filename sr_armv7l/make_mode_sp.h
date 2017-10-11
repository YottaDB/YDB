/****************************************************************
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define CODEBUF_TYPE	char
#define CALL_SIZE	12
#define EXTRA_INST	0
#define EXTRA_INST_SIZE	0

#define GEN_CALL(func)  { \
    assert(SIZEOF(*code) == SIZEOF(char));			\
    /* movw r0,#func[15:16] */					\
    *code++ = (int)func & 0x000000ff;				\
    *code++ = ((int)func & 0x00000f00) >> 8;			\
    *code++ = ((int)func & 0x0000f000) >> 12;			\
    *code++ = 0xe3;						\
    /* movt r0, #func[31:16] */					\
    *code++ = (((int)func & 0x00ff0000) >> 16);			\
    *code++ = (((int)func & 0x0f000000) >> 24);			\
    *code++ = 0x40 | (((int)func & 0xf0000000) >> 28);		\
    *code++ = 0xe3;						\
    /* blx r0 */						\
    *code++ = 0x30;						\
    *code++ = 0xff;						\
    *code++ = 0x2f;						\
    *code++ = 0xe1;						\
    }
