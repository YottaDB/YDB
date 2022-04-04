/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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

LITREF unsigned char lower_to_upper_table[];

error_def(ERR_ZSHOWBADFUNC);

void op_zshow(mval *func, int type, lv_val *lvn)
{
	const char	*ptr, *ptr_top;
  	zshow_out	output;
<<<<<<< HEAD
=======
	char		zshow_code;
	char		reqorder[sizeof(ZSHOW_ALL_ITEMS)];	/* codes in requested order, just once */
	int		reqcnt;
	int		done_chars = 0;	/* Bit field of upper case characters found */

>>>>>>> eb3ea98c (GT.M V7.0-002)
	MAXSTR_BUFF_DECL(buff);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(func);
	assert(0 <= func->str.len);
	for (reqcnt = 0, ptr = func->str.addr, ptr_top = ptr + func->str.len; ptr < ptr_top; ptr++)
	{
		zshow_code = lower_to_upper_table[(unsigned char)*ptr];
		switch (zshow_code)
		{
			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'G':
			case 'I':
			case 'L':
			case 'R':
			case 'S':
			case 'T':
			case 'V':
				if (!(done_chars & (1 << (zshow_code - 'A'))))
				{	/* New code, put it in the request list and mark it as done */
					done_chars |= (1 << (zshow_code - 'A'));
					reqorder[reqcnt++] = zshow_code;
				}
				break;
			case '*':
				/* '*' requested. Set all character codes as done, including impossible,
				 * which doubles as the '*' requested flag, because there are only 26
				 * characters in the alphabet */
				done_chars = -1;
				break;
			default:
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZSHOWBADFUNC);
		}
	}
	if (-1 == done_chars)
	{	/* '*' requested, use the default list */
		ptr = ZSHOW_ALL;
		reqcnt = STR_LIT_LEN(ZSHOW_ALL);
	} else	/* Use the generated list */
		ptr = reqorder;
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
	/* The order of output is in the requested order of zshow information codes
	 * NOTE: "*", aka all, has an order of output: "IVBDLGRC" without "A" or "T"
	 * NOTE: "R" is for stack output with checksums (default for '*')
	 * 	 "S" is for stack output without checksums
	 * NOTE: "T" is "G" and "L", reversed order from the above, and with totals only
	 * NOTE: "A" added with GTM-8160 for AutoRelink to provide the same information
	 * 	     as MUPIP RCTLDUMP. Due to the volume of information, this is not
	 * 	     included in "*"
	 */
	assert((0 <= reqcnt) && (reqcnt < SIZEOF(reqorder)));
	for (ptr_top = ptr + reqcnt; ptr < ptr_top; ptr++)
	{
		output.line_num = 1;	/* Reset to column 1 */
		output.code = *ptr;
		switch (*ptr)
		{
			case 'A':
				ARLINK_ONLY(zshow_rctldump(&output));
				break;
			case 'B':
				zshow_zbreaks(&output);
				break;
			case 'C':
				zshow_zcalls(&output);
				break;
			case 'D':
				zshow_devices(&output);
				break;
			case 'G':
				output.line_num = 0;	/* G statistics start at 0 for <*,*> output and not 1 like the others */
				zshow_gvstats(&output, FALSE);
				break;
			case 'I':
				zshow_svn(&output, SV_ALL);
				break;
			case 'L':
				output.line_num = 0;	/* L statistics start at 0 for <LUS,LUF> output and not 1 like the others */
				zshow_locks(&output, FALSE);
				break;
			case 'R':
				zshow_stack(&output, TRUE);	/* show_checksum = TRUE */
				break;
			case 'S':
				zshow_stack(&output, FALSE);	/* show_checksum = FALSE */
				break;
			case 'T':
				output.code = 'G';
				output.line_num = 0;	/* G statistics start at 0 for <*,*> output and not 1 like the others */
				zshow_gvstats(&output, TRUE);
				output.code = 'L';
				output.line_num = 0;	/* L statistics start at 0 for <LUS,LUF> output and not 1 like the others */
				zshow_locks(&output, TRUE);
				break;
			case 'V':
				zshow_zwrite(&output);
				break;
			default:
				assert(FALSE);
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
