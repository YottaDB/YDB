/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CDBG_DUMP_H
#define CDBG_DUMP_H

/* Values for 2nd arg of cdbg_dump_operand */
#define OP_0 0		/* operand[0] is passed */
#define OP_1 1		/* operand[1] is passed */
#define OP_DEST 2	/* destination is passed */

void cdbg_dump_triple(triple *dtrip, int indent);
void cdbg_dump_shrunk_triple(triple *dtrip, int old_size, int new_size);
void cdbg_dump_operand(int indent, oprtype *opr, int opnum);
void cdbg_dump_mval(int indent, mval *mv);
void cdbg_dump_mstr(int indent, mstr *ms);
char *cdbg_indent(int indent);
char *cdbg_makstr(char *str, char **buf, mstr_len_t len);
#endif
