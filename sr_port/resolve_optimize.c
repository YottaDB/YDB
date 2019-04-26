/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "compiler.h"
#include "mdq.h"
#include "op.h"
#include "opcode.h"
#include "cmd_qlf.h"
#include "mmemory.h"
#include "resolve_lab.h"
#include "cdbg_dump.h"
#include "gtmdbglvl.h"
#include "stringpool.h"
#include "mvalconv.h"
#include "show_source_line.h"

GBLREF mident			routine_name;
GBLREF short int 		source_line;
GBLREF spdesc			stringpool;
GBLREF src_line_struct		src_head;

error_def(ERR_DONOBLOCK);

boolean_t resolve_optimize(triple *curtrip)
{	/* a when all lines have been parsed optimization, might someday do more */
	boolean_t negative, optimized = FALSE;
	int	i, sav_col;
	mstr	src_line;
	mval	tmp_mval, accumulator, *mval_x, *mval_y;
	triple	*ref, *y, *x, *triple_temp;
	triple	*line_offset, *label, *routine;
	src_line_struct	*cur_line;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If we are resolving indirect's or something of the sort, die sadly */
	assert(NULL != src_head.que.fl);
	switch (curtrip->opcode)
	{
	case OC_FNTEXT:
		/* If OC_FNTEXT for current routine, we can optimize it by simply inserting text from src_line_struct.que */
		assert(OC_LITC != curtrip->operand[0].oprval.tref->opcode);
		routine = curtrip->operand[1].oprval.tref->operand[1].oprval.tref;
		line_offset = curtrip->operand[1].oprval.tref->operand[0].oprval.tref;
		label = curtrip->operand[0].oprval.tref;
		/* TODO: there should be a routine to verify literals for a given function */
		if (MLIT_REF != routine->operand[0].oprclass)
			break;
		if (!WANT_CURRENT_RTN(&routine->operand[0].oprval.mlit->v))
			break;
		if (MLIT_REF != label->operand[0].oprclass)
			break;
		if (ILIT_REF != line_offset->operand[0].oprclass)
			break;
		/* If we're here, we have a $TEXT with all literals for the current routine */
		src_line.len = 0;
		src_line.addr = 0;
		if ((0 == label->operand[0].oprval.mlit->v.str.len) && (0 == line_offset->operand[0].oprval.ilit))
		{	/* Special case: label == "" && +0 means file name - drop in file name */
			src_line.len = routine_name.len;
			src_line.addr = malloc(src_line.len);
			memcpy(src_line.addr, routine_name.addr, src_line.len);
		} else
		{	/* Search through strings for label; if label == "" skip */
			cur_line = src_head.que.fl;
			negative = (0 > line_offset->operand[0].oprval.ilit);
			if (0 != label->operand[0].oprval.mlit->v.str.len && cur_line != cur_line->que.fl)
			{
				for (i = 0; &src_head != cur_line; cur_line = cur_line->que.fl)
				{
					if (label->operand[0].oprval.mlit->v.str.len > cur_line->str.len)
						continue;
					if (label->operand[0].oprval.mlit->v.str.len != cur_line->str.len)
					{
						switch (cur_line->str.addr[label->operand[0].oprval.mlit->v.str.len])
						{
							case ' ':
							case ';':
							case '(':
							case ':':
								break;
							default:
								/* If we get here, it means we have a superstring of the label;
								 * i.e. searching for "a" found "abc"
								 */
								continue;
						}
					}
					if (!strncmp(label->operand[0].oprval.mlit->v.str.addr, cur_line->str.addr,
						label->operand[0].oprval.mlit->v.str.len))
						break;
				}
				if (&src_head == cur_line)
					break;		/* line not found; runtime produces an error */
			} else
			{	/* We need a special case to handle +0; if no label, it means start at top of file
				 * and we begin counting on 1, otherwise, it means the line that the label is on
				 */
				i = 1;
			}
			/* We could mod the offset by the size of the file, but hopefully no one is dumb enough to say +100000 */
			/* Counting the number of lines in the file will be O(n), not worth it */
			for (; i < (negative ? -1 : 1) * line_offset->operand[0].oprval.ilit && cur_line != &src_head; i++)
			{
				cur_line = (negative ? cur_line->que.bl : cur_line->que.fl);
			}
			/* If we went through all nodes and i is less than the line we are looking for, use an empty source line */
			if (&src_head == cur_line)
			{	/* Special case; we were counting backward, hit the end of the file, but we are done counting */
				/* This means we should output the name of the routine */
				if (i == (negative ? -1 : 1) * line_offset->operand[0].oprval.ilit
					&& negative)
				{
					src_line.len = routine_name.len;
					src_line.addr = malloc(src_line.len);
					memcpy(src_line.addr, routine_name.addr, src_line.len);
				} else
				{
					src_line.len = 0;
					src_line.addr = 0;
				}
			} else
			{
				src_line.len = cur_line->str.len;
				src_line.addr = malloc(src_line.len);
				memcpy(src_line.addr, cur_line->str.addr, cur_line->str.len);
			}
		}
		/* Insert literal into triple tree */
		tmp_mval.mvtype = MV_STR;
		/* Minus one so we don't copy newline character */
		tmp_mval.str.len = (src_line.len == 0 ? 0 :
			src_line.len - (src_line.addr[src_line.len-1] == '\n' ? 1 : 0));
		ENSURE_STP_FREE_SPACE(tmp_mval.str.len);
		tmp_mval.str.addr = (char *)stringpool.free;
		memcpy(tmp_mval.str.addr, src_line.addr, tmp_mval.str.len);
		/* Replace tab characters with spaces */
		for (i = 0; i < tmp_mval.str.len && tmp_mval.str.addr[i] != ';'; i++)
		{
			if ('\t' == tmp_mval.str.addr[i])
				tmp_mval.str.addr[i] = ' ';
		}
		stringpool.free += tmp_mval.str.len;
		if ((0 != src_line.len) && (NULL != src_line.addr))
			free(src_line.addr);
		/* Update all things that referenced this value */
		curtrip->opcode = OC_LIT;
		put_lit_s(&tmp_mval, curtrip);
		label->opcode = OC_NOOP;
		routine->opcode = OC_NOOP;
		optimized = TRUE;
		break;
	case OC_CALLSP:
		assert((src_head.que.bl->line + 1) == source_line);
		sav_col = TREF(last_source_column);
		source_line = curtrip->src.line;
		for (cur_line = src_head.que.fl; &src_head != cur_line; cur_line = cur_line->que.fl)
		{
			if (source_line == cur_line->line)
			{	/* this just spits out a warning */
				src_line = TREF(source_buffer);
				TREF(source_buffer) = cur_line->str;
				TREF(last_source_column) = curtrip->src.column;
				show_source_line(TRUE);
				dec_err(VARLSTCNT(1) ERR_DONOBLOCK);
				/* not aware of any reason to put the the following 3 back, but should be rare and safe */
				TREF(source_buffer) = src_line;
				TREF(last_source_column) = sav_col;
				source_line = (src_head.que.bl->line + 1);
				break;
			}
		}
		assert(&src_head != cur_line);						/* code line should have been there */
		curtrip->opcode = OC_NOOP;						/* this is real opimization */
		break;
	default:
		break;									/* No optimization (yet) for other cases */
	}
	return optimized;
}
