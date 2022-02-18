/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "error.h"
#include "lv_val.h"
#include "mlkdef.h"
#include "zshow.h"
#include "compiler.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtm_maxstr.h"
#include "svnames.h"

GBLREF	lv_val	*active_lv;
GBLREF	gv_key	*gv_currkey;

LITREF	mval	literal_null;

error_def(ERR_ZSHOWBADFUNC);

void op_zshow(mval *func, int type, lv_val *lvn)
{
	const char	*ptr;
	boolean_t	do_all = FALSE,
			done_a = FALSE,
			done_b = FALSE,
			done_c = FALSE,
			done_d = FALSE,
			done_g = FALSE,
			done_i = FALSE,
			done_l = FALSE,
			done_r = FALSE,
			done_s = FALSE,
			done_t = FALSE,
			done_v = FALSE;
	int		i;
  	zshow_out	output;
	MAXSTR_BUFF_DECL(buff);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(func);
	for (i = 0, ptr = func->str.addr; i < func->str.len; i++, ptr++)
	{
		switch (*ptr)
		{
			case 'A':
			case 'a':
			case 'B':
			case 'b':
			case 'C':
			case 'c':
			case 'D':
			case 'd':
			case 'G':
			case 'g':
			case 'I':
			case 'i':
			case 'L':
			case 'l':
			case 'R':
			case 'r':
			case 'S':
			case 's':
			case 'T':
			case 't':
			case 'V':
			case 'v':
				continue;
			case '*':
				do_all = TRUE;
				break;
			default:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZSHOWBADFUNC);
		}
	}
	if (do_all)
	{
		ptr = ZSHOW_ALL;
		i = STR_LIT_LEN(ZSHOW_ALL);
	} else
	{
		ptr = func->str.addr;
		i = func->str.len;
	}
	memset(&output, 0, SIZEOF(output));
	assert((ZSHOW_LOCAL == type) || (NULL == lvn));
	if (ZSHOW_LOCAL == type)
	{
		assert(NULL != lvn);
		output.out_var.lv.lvar = lvn;
	} else if (type == ZSHOW_GLOBAL)
	{
		output.out_var.gv.end = gv_currkey->end;
		output.out_var.gv.prev = gv_currkey->prev;
		output.line_cont = 0;	/* currently only used for global output and only initialized here */
	}
	MAXSTR_BUFF_INIT;
	output.type = type;
	output.buff = &buff[0];
	output.size = SIZEOF(buff);
	output.ptr = output.buff;
	for ( ; i ; i--, ptr++)
	{
		output.line_num = 1;
		switch (*ptr)
		{
			case 'A':
			case 'a':
				if (done_a)
					break;
				done_a = TRUE;
				output.code = 'A';
				ARLINK_ONLY(zshow_rctldump(&output));
				break;
			case 'B':
			case 'b':
				if (done_b)
					break;
				done_b = TRUE;
				output.code = 'B';
				zshow_zbreaks(&output);
				break;
			case 'C':
			case 'c':
				if (done_c)
					break;
				done_c = TRUE;
				output.code = 'C';
				zshow_zcalls(&output);
				break;
			case 'D':
			case 'd':
				if (done_d)
					break;
				done_d = TRUE;
				output.code = 'D';
				zshow_devices(&output);
				break;
			case 'G':
			case 'g':
				if (done_g)
					break;
				done_g = TRUE;
				output.code = 'G';
				output.line_num = 0;	/* G statistics start at 0 for <*,*> output and not 1 like the others */
				zshow_gvstats(&output, FALSE);
				break;
			case 'I':
			case 'i':
				if (done_i)
					break;
				done_i = TRUE;
				output.code = 'I';
				zshow_svn(&output, SV_ALL);
				break;
			case 'L':
			case 'l':
				if (done_l)
					break;
				done_l = TRUE;
				output.code = 'L';
				output.line_num = 0;	/* L statistics start at 0 for <LUS,LUF> output and not 1 like the others */
				zshow_locks(&output, FALSE);
				break;
			case 'R':
			case 'r':
				if (done_r)
					break;
				done_r = TRUE;
				output.code = 'R';
				zshow_stack(&output, TRUE);	/* show_checksum = TRUE */
				break;
			case 'S':
			case 's':
				if (done_s)
					break;
				done_s = TRUE;
				output.code = 'S';
				zshow_stack(&output, FALSE);	/* show_checksum = FALSE */
				break;
			case 'T':
			case 't':
				if (done_t)
					break;
				done_t = TRUE;
				output.code = 'G';
				output.line_num = 0;	/* G statistics start at 0 for <*,*> output and not 1 like the others */
				zshow_gvstats(&output, TRUE);
				output.code = 'L';
				output.line_num = 0;	/* L statistics start at 0 for <LUS,LUF> output and not 1 like the others */
				zshow_locks(&output, TRUE);
				break;
			case 'V':
			case 'v':
				if (done_v)
					break;
				done_v = TRUE;
				output.code = 'V';
				zshow_zwrite(&output);
				break;
		}
		/* If ZSHOW output was redirected to "lvn" but no zshow records got dumped in the subtree under "lvn" as part
		 * of the current zshow action code, it is possible an "op_putindx()" call was done to create a subscripted node
		 * under "lvn" in anticipation of actual nodes getting set under the subtree but none happened. In that case,
		 * the currently active lvn ("active_lv" global variable maintained by "op_putindx()") would have $data = 0.
		 * This will create an out-of-design situation for a later $query(lvn) and hence needs to be killed. Hence the
		 * use of the UNDO_ACTIVE_LV macro below. Note though that the "op_kill" call inside UNDO_ACTIVE_LV could
		 * potentially kill not just "active_lv" but also parent lv tree nodes until it finds a node with a non-zero $data.
		 * But we do not want the kill to reach "lvn" as that lv node is relied upon by later iterations in this for loop
		 * to unconditionally exist (the "op_putindx()" for that lvn would have been done by generated code). Therefore,
		 * check if "lvn" does not have any data and if so temporarily set it to point to one ("literal_null") before
		 * the UNDO_ACTIVE_LV and then reset it immediately afterwards.
		 */
		if ((ZSHOW_LOCAL == type) && (NULL != active_lv) && lcl_arg1_is_desc_of_arg2(active_lv, lvn))
		{
			boolean_t	is_defined;

			is_defined = LV_IS_VAL_DEFINED(lvn);
			if (!is_defined)
			{
				assert(0 == lvn->v.mvtype);
				lvn->v = literal_null;
			}
			UNDO_ACTIVE_LV(actlv_op_zshow);
			if (!is_defined)
				LV_VAL_CLEAR_MVTYPE(lvn);
		}
	}
	output.code = 0;
	output.flush = TRUE;
	zshow_output(&output,0);
	MAXSTR_BUFF_FINI;
	/* If ZSHOW output was redirected to "lvn", check if it is not a base var and if it is undefined. In that case,
	 * it is possible this is a node with $data = 0 (see comment block above explaining $query issues otherwise).
	 * If so, we need to kill it and any potential parent nodes until a non-zero $data node is found. Therefore use
	 * the UNDO_ACTIVE_LV macro below. Note though that "active_lv" should never point to an unsubscripted lvn hence
	 * the check for !LV_IS_BASE_VAR below.
	 */
	if ((ZSHOW_LOCAL == type) && !LV_IS_BASE_VAR(lvn) && !LV_IS_VAL_DEFINED(lvn))
	{
		active_lv = lvn;	/* needed by the UNDO_ACTIVE_LV macro call */
		UNDO_ACTIVE_LV(actlv_op_zshow);
	}
}
