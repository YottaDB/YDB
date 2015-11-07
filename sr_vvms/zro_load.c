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
#include <rms.h>

#include "toktyp.h"
#include "zroutines.h"
#include "gtm_caseconv.h"
#include "longcpy.h"
#include "error.h"

#define GETTOK		zro_gettok (&lp, top, &toktyp, &tok)

static	char		*zro_str;

error_def		(ERR_BADQUAL);
error_def		(ERR_COMMAORRPAREXP);
error_def		(ERR_DIRONLY);
error_def		(ERR_FILEPARSE);
error_def		(ERR_FSEXP);
error_def		(ERR_MAXARGCNT);
error_def		(ERR_QUALEXP);
error_def		(ERR_QUALVAL);
error_def		(ERR_WILDCARD);
error_def		(ERR_ZROSYNTAX);
error_def		(ERR_NOLBRSRC);

CONDITION_HANDLER(zro_load_ch)
{
	START_CH;
	if (zro_str)
		free(zro_str);
	zro_str = NULL;
	NEXTCH;
}

void  zro_load (mstr *str)
{
	unsigned		toktyp, status;
	mstr			tok;
	struct FAB		fab;
	struct NAM		nam;
	unsigned char		*lp, *top, qual[8];
	boolean_t		file;
	zro_ent			array[ZRO_MAX_ENTS];
	int			oi, si;
	unsigned char		buff[255];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* D9D01-002286 - copy str->addr into the malloc'd area (zro_str) so that zro_root components
	 * always point to the persistent area (rather than to the string pool which may get garbage
	 * collected later). */
	if (zro_str)
		free(zro_str);
	zro_str = (char *)malloc(str->len);
	memcpy(zro_str, str->addr, str->len);

	ESTABLISH(zro_load_ch);	/* Condition handler to release zro_str if $ZRO fails to load */
	fab = cc$rms_fab;
	nam = cc$rms_nam;
	fab.fab$l_nam = &nam;
	nam.nam$l_esa = buff;
	nam.nam$b_ess = SIZEOF(buff);

	lp = zro_str;
	top = lp + str->len;
	array[0].type = ZRO_TYPE_COUNT;
	array[0].count = 0;

	GETTOK;
	if (toktyp == TK_EOL)
	{
		status = sys$parse (&fab);
		array[0].count = 1;
		array[1].type = ZRO_TYPE_OBJECT;
		array[1].node_present = FALSE;
		array[1].str.len = 0;
		memcpy (array[1].dvi, nam.nam$t_dvi, NAM$C_DVI + 12);
		array[2].type = ZRO_TYPE_COUNT;
		array[2].count = 1;
		array[3] = array[1];
		array[3].type = ZRO_TYPE_SOURCE;
		si = 4;
	} else
	{
		for (oi = 1;;)
		{
			file = FALSE;
			if (toktyp != TK_IDENT)
				rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FSEXP);
			if (oi + 1 >= ZRO_MAX_ENTS)
				rts_error(VARLSTCNT(7) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
			fab.fab$b_fns = tok.len;
			fab.fab$l_fna = tok.addr;
			status = sys$parse (&fab);
			if (!(status & 1))
				rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FILEPARSE, 2, tok.len, tok.addr,
					  status);
			if (nam.nam$l_fnb & (NAM$M_WILDCARD))
				rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_WILDCARD, 2, tok.len, tok.addr);
			if (nam.nam$l_fnb & (NAM$M_EXP_NAME | NAM$M_EXP_TYPE | NAM$M_EXP_VER))
			{
				file = TRUE;
				status = sys$search(&fab);
				if (!(status & 1))
					rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
						  ERR_FILEPARSE, 2, tok.len, tok.addr, status);
			}
			array[0].count++;
			if (file)
				array[oi].type = ZRO_TYPE_OBJLIB;
			else
				array[oi].type = ZRO_TYPE_OBJECT;
			array[oi].node_present = (nam.nam$l_fnb & (NAM$M_NODE));
			array[oi].str = tok;
			memcpy (array[oi].dvi, nam.nam$t_dvi, NAM$C_DVI + 12);
			array[oi + 1].type = ZRO_TYPE_COUNT;
			si = oi + 2;
			GETTOK;
			if (toktyp == TK_SLASH)
			{
				GETTOK;
				if (toktyp != TK_IDENT)
					rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_QUALEXP);
				if (tok.len == 3)
				{
					lower_to_upper (qual, tok.addr, 3);
					if (!memcmp (qual, "SRC", 3))
					{
						if (file)
							rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_NOLBRSRC);

						GETTOK;
						if (si >= ZRO_MAX_ENTS)
							rts_error(VARLSTCNT(7) ERR_ZROSYNTAX, 2, str->len, str->addr,
								  ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
						if (toktyp == TK_COMMA || toktyp == TK_EOL)
						{
							array[oi + 1].count = 1;
							array[si] = array[oi];
							array[si].type = ZRO_TYPE_SOURCE;
							si++;
						} else if (toktyp == TK_EQUAL)
						{
							GETTOK;
							if (toktyp != TK_LPAREN)
							{
								if (toktyp != TK_IDENT)
									rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len,
										  str->addr, ERR_FSEXP);
								fab.fab$b_fns = tok.len;
								fab.fab$l_fna = tok.addr;
								status = sys$parse (&fab);
								if (!(status & 1))
									rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len,
										  str->addr, ERR_FILEPARSE, 2, tok.len, tok.addr,
										  status);
								if (nam.nam$l_fnb & (NAM$M_EXP_NAME | NAM$M_EXP_TYPE
										     | NAM$M_EXP_VER))
									rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len,
										  str->addr, ERR_DIRONLY, 2, tok.len,
										  tok.addr);
								if (nam.nam$l_fnb & (NAM$M_WILDCARD))
									rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len,
										  str->addr, ERR_WILDCARD, 2, tok.len,
										  tok.addr);
								array[oi + 1].count = 1;
								array[si].type = ZRO_TYPE_SOURCE;
								array[si].node_present = (nam.nam$l_fnb & (NAM$M_NODE));
								array[si].str = tok;
								memcpy (array[si].dvi, nam.nam$t_dvi, NAM$C_DVI + 12);
								si++;
								GETTOK;
							} else
							{
								array[oi + 1].count = 0;
								for (;;)
								{
									GETTOK;
									if (toktyp != TK_IDENT)
										rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len,
											  str->addr, ERR_FSEXP);
									if (si >= ZRO_MAX_ENTS)
										rts_error(VARLSTCNT(7) ERR_ZROSYNTAX, 2, str->len,
											  str->addr,ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
									fab.fab$b_fns = tok.len;
									fab.fab$l_fna = tok.addr;
									status = sys$parse (&fab);
									if (!(status & 1))
										rts_error(VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len,
											  str->addr,ERR_FILEPARSE, 2, tok.len,
											  tok.addr, status);
									if (nam.nam$l_fnb & (NAM$M_EXP_NAME | NAM$M_EXP_TYPE
											     | NAM$M_EXP_VER))
										rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len,
											  str->addr, ERR_DIRONLY, 2, tok.len,
											  tok.addr);
									if (nam.nam$l_fnb & (NAM$M_WILDCARD))
										rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len,
											  str->addr, ERR_WILDCARD, 2, tok.len,
											  tok.addr);
									array[oi + 1].count++;
									array[si].type = ZRO_TYPE_SOURCE;
									array[si].node_present = (nam.nam$l_fnb & (NAM$M_NODE));
									array[si].str = tok;
									memcpy (array[si].dvi, nam.nam$t_dvi, NAM$C_DVI + 12);
									si++;
									GETTOK;
									if (toktyp == TK_RPAREN)
										break;
									if (toktyp != TK_COMMA)
										rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len,
											  str->addr, ERR_COMMAORRPAREXP);
								}
								GETTOK;
							}
						} else
							rts_error(VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_QUALVAL);
					} else
						rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
							  ERR_BADQUAL, 2, tok.len, tok.addr);
				} else if (tok.len == 5)
				{
					lower_to_upper (qual, tok.addr, 5);
					if (!memcmp (qual, "NOSRC", 5))
					{
						array[oi + 1].count = 0;
						GETTOK;
					} else
						rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
							  ERR_BADQUAL, 2, tok.len, tok.addr);
				} else
					rts_error(VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
						  ERR_BADQUAL, 2, tok.len, tok.addr);
			} else if (toktyp == TK_COMMA || toktyp == TK_EOL)
			{
				if (!file)
				{
					array[oi + 1].count = 1;
					array[si] = array[oi];
					array[si].type = ZRO_TYPE_SOURCE;
					si++;
				} else
					array[oi + 1].count = 0;
			}
			if (toktyp == TK_COMMA)
				GETTOK;
			else if (toktyp == TK_EOL)
				break;
			else
				rts_error(VARLSTCNT(4) ERR_ZROSYNTAX, 2, str->len, str->addr);
			oi = si;
		}
	}
	if (TREF(zro_root))
		free (TREF(zro_root));
	TREF(zro_root) = malloc(si * SIZEOF(zro_ent));
	memcpy((uchar_ptr_t)TREF(zro_root), (uchar_ptr_t)array, si * SIZEOF(zro_ent));
	REVERT;
}
