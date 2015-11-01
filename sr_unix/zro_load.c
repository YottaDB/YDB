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
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "io.h"
#include "iosp.h"
#include "zroutines.h"
#include "parse_file.h"
#include "eintr_wrappers.h"
#include "longcpy.h"

#define GETTOK		toktyp = zro_gettok (&lp, top, &tok)

GBLDEF	zro_ent		*zro_root;
GBLREF  int		errno;

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
	error_def		(ERR_DIRONLY);
	error_def		(ERR_FILEPARSE);
	error_def		(ERR_FSEXP);
	error_def		(ERR_MAXARGCNT);
	error_def		(ERR_QUALEXP);
	error_def		(ERR_ZROSYNTAX);

	lp = str->addr;
	top = lp + str->len;
	while (lp < top && *lp == ZRO_DEL)
		lp++;

	array[0].type = ZRO_TYPE_COUNT;
	array[0].count = 0;
	memset(&pblk, 0, sizeof(pblk));
	pblk.buffer = tranbuf;

	GETTOK;
	if (toktyp == ZRO_EOL)
	{
		array[0].count = 1;
		array[1].type = ZRO_TYPE_OBJECT;
		array[1].str.len = 0;
		array[2].type = ZRO_TYPE_COUNT;
		array[2].count = 1;
		array[3] = array[1];
		array[3].type = ZRO_TYPE_SOURCE;
		si = 4;
	}
	else
	for (oi = 1;;)
	{
		if (toktyp != ZRO_IDN)
			rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FSEXP);
		if (oi + 1 >= ZRO_MAX_ENTS)
			rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_MAXARGCNT);
		if (tok.len >= sizeof (tranbuf))
			rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FILEPARSE, 2, tok.len, tok.addr);
		pblk.buff_size = MAX_FBUFF;
		pblk.fnb = 0;
		status = parse_file(&tok, &pblk);
		if (!(status & 1))
			rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
				ERR_FILEPARSE, 2, tok.len, tok.addr, status);

		tranbuf[ pblk.b_esl ] = 0;
		STAT_FILE(tranbuf, &outbuf, stat_res);
		if (-1 == stat_res)
			rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FILEPARSE, 2, tok.len, tok.addr, errno);
		if (!(outbuf.st_mode & S_IFDIR))
			rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_DIRONLY, 2, tok.len, tok.addr);
		array[0].count++;
		array[oi].type = ZRO_TYPE_OBJECT;
		array[oi].str = tok;
		array[oi + 1].type = ZRO_TYPE_COUNT;
		si = oi + 2;
		GETTOK;
		if (toktyp == ZRO_LBR)
		{
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
					rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_MAXARGCNT);
				if (tok.len >= sizeof (tranbuf))
					rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
						ERR_FILEPARSE, 2, tok.len, tok.addr);
				pblk.buff_size = MAX_FBUFF;
				pblk.fnb = 0;
				status = parse_file(&tok, &pblk);
				if (!(status & 1))
					rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
						ERR_FILEPARSE, 2, tok.len, tok.addr, status);
				tranbuf[ pblk.b_esl ] = 0;
				STAT_FILE(tranbuf, &outbuf, stat_res);
				if (-1 == stat_res)
					rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
						ERR_FILEPARSE, 2, tok.len, tok.addr, errno);
				if (!(outbuf.st_mode & S_IFDIR))
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
		}
		else
		if (toktyp == ZRO_DEL || toktyp == ZRO_EOL)
		{
			if (si >= ZRO_MAX_ENTS)
				rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_MAXARGCNT);
			array[oi + 1].count = 1;
			array[si] = array[oi];
			array[si].type = ZRO_TYPE_SOURCE;
			si++;
		}
		if (toktyp == ZRO_EOL)
			break;

		if (toktyp == ZRO_DEL)
			GETTOK;
		else
			rts_error(VARLSTCNT(4) ERR_ZROSYNTAX, 2, str->len, str->addr);
		oi = si;
	}
	total_ents = si;
	if (zro_root)
	{
		assert (zro_root->type == ZRO_TYPE_COUNT);
		oi = zro_root->count;
		assert (oi);
		for (op = zro_root + 1; oi-- > 0; )
		{	/* release space held by translated entries */
			assert (op->type == ZRO_TYPE_OBJECT);
			if (op->str.len)
				free(op->str.addr);
			op++;
			assert (op->type == ZRO_TYPE_COUNT);
			si = op++->count;
			for ( ; si-- > 0; op++)
			{
				assert (op->type == ZRO_TYPE_SOURCE);
				if (op->str.len)
					free(op->str.addr);
			}
		}
		free (zro_root);
	}
	zro_root = (zro_ent *) malloc (total_ents * sizeof (zro_ent));
	longcpy ((uchar_ptr_t)zro_root, (uchar_ptr_t)array, total_ents * sizeof (zro_ent));
	assert (zro_root->type == ZRO_TYPE_COUNT);
	oi = zro_root->count;
	assert (oi);
	for (op = zro_root + 1; oi-- > 0; )
	{
		assert (op->type == ZRO_TYPE_OBJECT);
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
		op++;
		assert (op->type == ZRO_TYPE_COUNT);
		si = op++->count;
		for ( ; si-- > 0; op++)
		{
			assert (op->type == ZRO_TYPE_SOURCE);
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
