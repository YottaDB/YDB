/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"

#include <errno.h>
#include "io.h"
#include "iosp.h"
#include "zroutines.h"
#include "parse_file.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "zro_shlibs.h"

#define GETTOK		toktyp = zro_gettok(&lp, top, &tok)

error_def(ERR_DIRONLY);
error_def(ERR_FILEPARSE);
error_def(ERR_FSEXP);
error_def(ERR_INVZROENT);
error_def(ERR_MAXARGCNT);
error_def(ERR_NOLBRSRC);
error_def(ERR_QUALEXP);
error_def(ERR_ZROSYNTAX);

void zro_load (mstr *str)
{
	unsigned		toktyp, status;
	mstr			tok;
	char			*lp, *top;
	zro_ent			array[ZRO_MAX_ENTS], *op;
	int			oi, si, total_ents;
	struct  stat		outbuf;
	int			stat_res;
	char			tranbuf[MAX_FBUFF + 1];
	parse_blk		pblk;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	memset(array, 0, SIZEOF(array));
	lp = str->addr;
	top = lp + str->len;
	while (lp < top && *lp == ZRO_DEL)	/* Bypass leading blanks */
		lp++;

	array[0].type = ZRO_TYPE_COUNT;
	array[0].count = 0;
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = tranbuf;

	GETTOK;
	if (toktyp == ZRO_EOL)
	{	/* Null string - set default */
		array[0].count = 1;
		array[1].type = ZRO_TYPE_OBJECT;
		array[1].str.len = 0;
		array[2].type = ZRO_TYPE_COUNT;
		array[2].count = 1;
		array[3].type = ZRO_TYPE_SOURCE;
		array[3].str.len = 0;
		si = 4;
	} else
	{	/* String supplied - parse it */
		for (oi = 1;;)
		{
			if (toktyp != ZRO_IDN)
				rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FSEXP);
			if (oi + 1 >= ZRO_MAX_ENTS)
				rts_error(VARLSTCNT(7) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
			if (tok.len >= SIZEOF(tranbuf))
				rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FILEPARSE, 2, tok.len, tok.addr);
			pblk.buff_size = MAX_FBUFF;
			pblk.fnb = 0;
			status = parse_file(&tok, &pblk);
			if (!(status & 1))
				rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
					ERR_FILEPARSE, 2, tok.len, tok.addr, status);

			tranbuf[pblk.b_esl] = 0;
			STAT_FILE(tranbuf, &outbuf, stat_res);
			if (-1 == stat_res)
				rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FILEPARSE, 2, tok.len, tok.addr,
					  errno);
			if (S_ISREG(outbuf.st_mode))
			{	/* regular file - a shared library file */
				array[oi].shrlib = zro_shlibs_find(tranbuf);
				array[oi].type = ZRO_TYPE_OBJLIB;
				si = oi + 1;
			} else
			{
				if (!S_ISDIR(outbuf.st_mode))
					rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_INVZROENT, 2,
							tok.len, tok.addr);
				array[oi].type = ZRO_TYPE_OBJECT;
				array[oi + 1].type = ZRO_TYPE_COUNT;
				si = oi + 2;
			}
			array[0].count++;
			array[oi].str = tok;
			GETTOK;
			if (toktyp == ZRO_LBR)
			{
				if (array[oi].type == ZRO_TYPE_OBJLIB)
					rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_NOLBRSRC);

				GETTOK;
				if (toktyp == ZRO_DEL)
					GETTOK;
				if (toktyp != ZRO_IDN && toktyp != ZRO_RBR)
					rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_QUALEXP);

				array[oi + 1].count = 0;
				for (;;)
				{
					if (toktyp == ZRO_RBR)
						break;
					if (toktyp != ZRO_IDN)
						rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FSEXP);
					if (si >= ZRO_MAX_ENTS)
						rts_error(VARLSTCNT(7) ERR_ZROSYNTAX, 2, str->len, str->addr,
								ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
					if (tok.len >= SIZEOF(tranbuf))
						rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
							ERR_FILEPARSE, 2, tok.len, tok.addr);
					pblk.buff_size = MAX_FBUFF;
					pblk.fnb = 0;
					status = parse_file(&tok, &pblk);
					if (!(status & 1))
						rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
							ERR_FILEPARSE, 2, tok.len, tok.addr, status);
					tranbuf[pblk.b_esl] = 0;
					STAT_FILE(tranbuf, &outbuf, stat_res);
					if (-1 == stat_res)
						rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
							ERR_FILEPARSE, 2, tok.len, tok.addr, errno);
					if (!S_ISDIR(outbuf.st_mode))
						rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
							ERR_DIRONLY, 2, tok.len, tok.addr);
					array[oi + 1].count++;
					array[si].type = ZRO_TYPE_SOURCE;
					array[si].str = tok;
					si++;
					GETTOK;
					if (toktyp == ZRO_DEL)
						GETTOK;
				}
				GETTOK;
			} else
			{
				if ((array[oi].type != ZRO_TYPE_OBJLIB) && (toktyp == ZRO_DEL || toktyp == ZRO_EOL))
				{
					if (si >= ZRO_MAX_ENTS)
						rts_error(VARLSTCNT(7) ERR_ZROSYNTAX, 2, str->len, str->addr,
							  ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
					array[oi + 1].count = 1;
					array[si] = array[oi];
					array[si].type = ZRO_TYPE_SOURCE;
					si++;
				}
			}
			if (toktyp == ZRO_EOL)
				break;

			if (toktyp == ZRO_DEL)
				GETTOK;
			else
				rts_error(VARLSTCNT(4) ERR_ZROSYNTAX, 2, str->len, str->addr);
			oi = si;
		}
	}
	total_ents = si;
	if (TREF(zro_root))
	{
		assert((TREF(zro_root))->type == ZRO_TYPE_COUNT);
		oi = (TREF(zro_root))->count;
		assert(oi);
		for (op = TREF(zro_root) + 1; oi-- > 0; )
		{	/* release space held by translated entries */
			assert(op->type == ZRO_TYPE_OBJECT || op->type == ZRO_TYPE_OBJLIB);
			if (op->str.len)
				free(op->str.addr);
			if ((op++)->type == ZRO_TYPE_OBJLIB)
				continue;	/* i.e. no sources for shared library */
			assert(op->type == ZRO_TYPE_COUNT);
			si = (op++)->count;
			for ( ; si-- > 0; op++)
			{
				assert(op->type == ZRO_TYPE_SOURCE);
				if (op->str.len)
					free(op->str.addr);
			}
		}
		free(TREF(zro_root));
	}
	TREF(zro_root) = (zro_ent *)malloc(total_ents * SIZEOF(zro_ent));
	memcpy((uchar_ptr_t)TREF(zro_root), (uchar_ptr_t)array, total_ents * SIZEOF(zro_ent));
	assert((TREF(zro_root))->type == ZRO_TYPE_COUNT);
	oi = (TREF(zro_root))->count;
	assert(oi);
	for (op = TREF(zro_root) + 1; oi-- > 0; )
	{
		assert(op->type == ZRO_TYPE_OBJECT || op->type == ZRO_TYPE_OBJLIB);
		if (op->str.len)
		{
			pblk.buff_size = MAX_FBUFF;
			pblk.fnb = 0;
			status = parse_file(&op->str, &pblk);
			if (!(status & 1))
				rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
					ERR_FILEPARSE, 2, op->str.len, op->str.addr, status);

			op->str.addr = (char *)malloc(pblk.b_esl);
			op->str.len = pblk.b_esl;
			memcpy(op->str.addr, pblk.buffer, pblk.b_esl);
		}
		if ((op++)->type == ZRO_TYPE_OBJLIB)
			continue;
		assert(op->type == ZRO_TYPE_COUNT);
		si = (op++)->count;
		for ( ; si-- > 0; op++)
		{
			assert(op->type == ZRO_TYPE_SOURCE);
			if (op->str.len)
			{
				pblk.buff_size = MAX_FBUFF;
				pblk.fnb = 0;
				status = parse_file(&op->str, &pblk);
				if (!(status & 1))
					rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
						ERR_FILEPARSE, 2, op->str.len, op->str.addr, status);

				op->str.addr = (char *)malloc(pblk.b_esl);
				op->str.len = pblk.b_esl;
				memcpy(op->str.addr, pblk.buffer, pblk.b_esl);
			}
		}
	}
}
