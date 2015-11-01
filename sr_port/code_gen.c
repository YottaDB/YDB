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

#include "mdef.h"
#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "cgp.h"
#include "cmd_qlf.h"
#include "rtnhdr.h"
#include "obj_file.h"
#include "list_file.h"
#include "emit_code.h"
#include "dumptable.h"

LITREF octabstruct	oc_tab[];	/* op-code table */
GBLREF triple		t_orig;		/* head of triples */
GBLREF char		cg_phase;	/* code generation phase */
GBLREF int4		curr_addr;	/* current address */
GBLREF src_line_struct	src_head;
GBLREF short		source_column,source_line;

void	code_gen(void)
{
	int4		old_line;
	triple		*ct;	/* current triple */
	src_line_struct	*sl;

	if (cg_phase == CGP_ASSEMBLY)
	{
		curr_addr = t_orig.exorder.fl->rtaddr;
		old_line = -1;
	}
	dqloop(&t_orig, exorder, ct)
	{
		if (cg_phase == CGP_APPROX_ADDR)
		{
			ct->rtaddr = curr_addr;
		}
		else if (cg_phase == CGP_ASSEMBLY)
		{
			if (ct->src.line != old_line)
			{
				list_line("");
				for (sl = src_head.que.bl; sl->line <= ct->src.line && sl != &src_head; )
				{
					list_line_number();
					dqdel(sl,que);
					list_line(sl->addr);
					sl = src_head.que.bl;
				}
				old_line = ct->src.line;
			}
		}
		source_line = ct->src.line;
		source_column = ct->src.column;

		if (!(oc_tab[ct->opcode].octype & OCT_CGSKIP))
		{
			trip_gen(ct);
		}
	}/* dqloop */

#ifdef	_AIX
	emit_epilog();
#endif
	if (cg_phase == CGP_ASSEMBLY)
		dumptable();
}/* code_gen */
