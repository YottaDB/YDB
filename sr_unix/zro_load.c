/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"

#include "io.h"
#include "iosp.h"
#include "zroutines.h"
#include "parse_file.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "zro_shlibs.h"
#include "gtm_limits.h"

#define GETTOK		zro_gettok(&lp, top, &tok)

error_def(ERR_DIRONLY);
error_def(ERR_FILEPARSE);
error_def(ERR_FSEXP);
error_def(ERR_INVZROENT);
error_def(ERR_MAXARGCNT);
error_def(ERR_NOLBRSRC);
error_def(ERR_QUALEXP);
error_def(ERR_ZROSYNTAX);

/* Routine to parse the value of $ZROUTINES and create the list of structures that define the (new) routine
 * search list order and define which (if any) directories can use auto-relink.
 *
 * Parameter:
 *   str   - string to parse (usually dollar_zroutines)
 *
 * Return code:
 *   none
 */
void zro_load(mstr *str)
{
	unsigned		toktyp, status;
	boolean_t		arlink_thisdir_enable, arlink_enabled;
	mstr			tok, transtr;
	char			*lp, *top;
	zro_ent			array[ZRO_MAX_ENTS], *op;
	int			oi, si, total_ents;
	struct  stat		outbuf;
	int			stat_res;
	char			tranbuf[MAX_FBUFF + 1];
	parse_blk		pblk;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	arlink_enabled = FALSE;
	memset(array, 0, SIZEOF(array));
	lp = str->addr;
	top = lp + str->len;
	while ((lp < top) && (ZRO_DEL == *lp))	/* Bypass leading blanks */
		lp++;
	array[0].type = ZRO_TYPE_COUNT;
	array[0].count = 0;
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = tranbuf;
	toktyp = GETTOK;
	if (ZRO_EOL == toktyp)
	{	/* Null string - set default - implies current working directory only */
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
			if (ZRO_IDN != toktyp)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_FSEXP);
			if (ZRO_MAX_ENTS <= (oi + 1))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_ZROSYNTAX, 2, str->len, str->addr, ERR_MAXARGCNT, 1,
					      ZRO_MAX_ENTS);
			/* We have type ZRO_IDN (an identifier/name of some sort). See if token has a "*" (ZRO_ALF) at the end
			 * of it indicating that it is supposed to (1) be a directory and not a shared library and (2) that the
			 * user desires this directory to have auto-relink capability.
			 */
			arlink_thisdir_enable = FALSE;
			/* All platforms allow the auto-relink indicator on object directories but only autorelink able platforms
			 * (#ifdef AUTORELINK_SUPPORTED is set) do anything with it. Other platforms just ignore it. Specifying
			 * "*" at end of non-object directories causes an error further downstream (FILEPARSE) when the "*" is
			 * not stripped off the file name - unless someone has managed to create a directory with a "*" suffix.
			 */
			if (ZRO_ALF == *(tok.addr + tok.len - 1))
			{	/* Auto-relink is indicated */
				arlink_thisdir_enable = TRUE;
				--tok.len;		/* Remove indicator from name so we can use it */
				assert(0 <= tok.len);
			}
			if (SIZEOF(tranbuf) <= tok.len)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
					      ERR_FILEPARSE, 2, tok.len, tok.addr);
			/* Run specified directory through parse_file to fill in any missing pieces and get some info on it */
			pblk.buff_size = MAX_FBUFF;	/* Don't count null terminator here */
			pblk.fnb = 0;
			status = parse_file(&tok, &pblk);
			if (!(status & 1))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
					      ERR_FILEPARSE, 2, tok.len, tok.addr, status);
			tranbuf[pblk.b_esl] = 0;		/* Needed for some subsequent STAT_FILE */
			STAT_FILE(tranbuf, &outbuf, stat_res);
			if (-1 == stat_res)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
					      ERR_FILEPARSE, 2, tok.len, tok.addr, errno);
			if (S_ISREG(outbuf.st_mode))
			{	/* Regular file - a shared library file */
				if (arlink_thisdir_enable)
					/* Auto-relink indicator on shared library not permitted */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
						      ERR_FILEPARSE, 2, tok.len, tok.addr);
				array[oi].shrlib = zro_shlibs_find(tranbuf);
				array[oi].type = ZRO_TYPE_OBJLIB;
				si = oi + 1;
			} else
			{
				if (!S_ISDIR(outbuf.st_mode))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
						      ERR_INVZROENT, 2, tok.len, tok.addr);
				array[oi].type = ZRO_TYPE_OBJECT;
				array[oi + 1].type = ZRO_TYPE_COUNT;
				si = oi + 2;
#				ifdef AUTORELINK_SUPPORTED
#					ifdef DEBUG
					/* If env var gtm_test_autorelink_always is set in dbg version, treat every
					 * object directory specified in $zroutines as if * has been additionally specified.
					 */
					if (TREF(gtm_test_autorelink_always))
						arlink_thisdir_enable = TRUE;
#					endif
				if (arlink_thisdir_enable)
				{	/* Only setup autorelink struct if it is enabled */
					if (!TREF(is_mu_rndwn_rlnkctl))
					{
						transtr.addr = tranbuf;
						transtr.len = pblk.b_esl;
						array[oi].relinkctl_sgmaddr = (void_ptr_t)relinkctl_attach(&transtr, NULL, 0);
					} else
					{	/* If zro_load() is called as a part of MUPIP RUNDOWN -RELINKCTL, then we do not
						 * want to do relinkctl_attach() on all relinkctl files at once because we leave
						 * the function holding the linkctl lock, which might potentially cause a deadlock
						 * if multiple processes are run concurrently with different $gtmroutines. However,
						 * we need a way to tell mu_rndwn_rlnkctl() which object directories are autorelink-
						 * enabled. For that we set a negative number to the presently unused count field of
						 * object directory entries in the zro_ent linked list. If we ever decide to make
						 * that value meaningful, then, perhaps, ensuring that this count remains negative
						 * in case of MUPIP RUNDOWN -RELINKCTL but has the correct absolute value would do
						 * the trick.
						 */
						array[oi].count = ZRO_DIR_ENABLE_AR;
					}
				}
#				endif
			}
			arlink_enabled |= arlink_thisdir_enable;	/* Cumulative value of enabled dirs */
			array[0].count++;
			array[oi].str = tok;
			toktyp = GETTOK;
			if (ZRO_LBR == toktyp)
			{
				if (ZRO_TYPE_OBJLIB == array[oi].type)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr,
						      ERR_NOLBRSRC);
				toktyp = GETTOK;
				if (ZRO_DEL == toktyp)
					toktyp = GETTOK;
				if ((ZRO_IDN != toktyp) && (ZRO_RBR != toktyp))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr,
						      ERR_QUALEXP);
				array[oi + 1].count = 0;
				for (;;)
				{
					if (ZRO_RBR == toktyp)
						break;
					if (ZRO_IDN != toktyp)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZROSYNTAX, 2, str->len, str->addr,
							      ERR_FSEXP);
					if (ZRO_MAX_ENTS <= si)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_ZROSYNTAX, 2, str->len, str->addr,
							      ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
					if (SIZEOF(tranbuf) <= tok.len)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
							      ERR_FILEPARSE, 2, tok.len, tok.addr);
					pblk.buff_size = MAX_FBUFF;
					pblk.fnb = 0;
					status = parse_file(&tok, &pblk);
					if (!(status & 1))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
							      ERR_FILEPARSE, 2, tok.len, tok.addr, status);
					tranbuf[pblk.b_esl] = 0;
					STAT_FILE(tranbuf, &outbuf, stat_res);
					if (-1 == stat_res)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
							      ERR_FILEPARSE, 2, tok.len, tok.addr, errno);
					if (!S_ISDIR(outbuf.st_mode))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZROSYNTAX, 2, str->len, str->addr,
							      ERR_DIRONLY, 2, tok.len, tok.addr);
					array[oi + 1].count++;
					array[si].type = ZRO_TYPE_SOURCE;
					array[si].str = tok;
					si++;
					toktyp = GETTOK;
					if (ZRO_DEL == toktyp)
						toktyp = GETTOK;
				}
				toktyp = GETTOK;
			} else
			{
				if ((ZRO_TYPE_OBJLIB != array[oi].type) && ((ZRO_DEL == toktyp) || (ZRO_EOL == toktyp)))
				{
					if (ZRO_MAX_ENTS <= si)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_ZROSYNTAX, 2, str->len, str->addr,
							      ERR_MAXARGCNT, 1, ZRO_MAX_ENTS);
					array[oi + 1].count = 1;
					array[si] = array[oi];
					array[si].type = ZRO_TYPE_SOURCE;
					si++;
				}
			}
			if (ZRO_EOL == toktyp)
				break;
			if (ZRO_DEL == toktyp)
				toktyp = GETTOK;
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZROSYNTAX, 2, str->len, str->addr);
			oi = si;
		}
	}
	total_ents = si;
	if (TREF(zro_root))
	{
		assert((TREF(zro_root))->type == ZRO_TYPE_COUNT);
		oi = (TREF(zro_root))->count;
		assert(oi);
		for (op = TREF(zro_root) + 1; 0 < oi--;)
		{	/* Release space held by translated entries */
			assert((ZRO_TYPE_OBJECT == op->type) || (ZRO_TYPE_OBJLIB == op->type));
			if (op->str.len)
				free(op->str.addr);
			if (ZRO_TYPE_OBJLIB == (op++)->type)
				continue;	/* i.e. no sources for shared library */
			assert(ZRO_TYPE_COUNT == op->type);
			si = (op++)->count;
			for (; si-- > 0; op++)
			{
				assert(ZRO_TYPE_SOURCE == op->type);
				if (op->str.len)
					free(op->str.addr);
			}
		}
		free(TREF(zro_root));
	}
	TREF(zro_root) = (zro_ent *)malloc(total_ents * SIZEOF(zro_ent));
	memcpy((uchar_ptr_t)TREF(zro_root), (uchar_ptr_t)array, total_ents * SIZEOF(zro_ent));
	assert(ZRO_TYPE_COUNT == (TREF(zro_root))->type);
	oi = (TREF(zro_root))->count;
	assert(oi);
	for (op = TREF(zro_root) + 1; 0 < oi--;)
	{
		assert((ZRO_TYPE_OBJECT == op->type) || (ZRO_TYPE_OBJLIB == op->type));
		if (op->str.len)
		{
			pblk.buff_size = MAX_FBUFF;
			pblk.fnb = 0;
			status = parse_file(&op->str, &pblk);
			if (!(status & 1))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
					      ERR_FILEPARSE, 2, op->str.len, op->str.addr, status);
			op->str.addr = (char *)malloc(pblk.b_esl);
			op->str.len = pblk.b_esl;
			memcpy(op->str.addr, pblk.buffer, pblk.b_esl);
		}
		if (ZRO_TYPE_OBJLIB == (op++)->type)
			continue;
		assert(ZRO_TYPE_COUNT == op->type);
		si = (op++)->count;
		for (; 0 < si--; op++)
		{
			assert(ZRO_TYPE_SOURCE == op->type);
			if (op->str.len)
			{
				pblk.buff_size = MAX_FBUFF;
				pblk.fnb = 0;
				status = parse_file(&op->str, &pblk);
				if (!(status & 1))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZROSYNTAX, 2, str->len, str->addr,
						      ERR_FILEPARSE, 2, op->str.len, op->str.addr, status);
				op->str.addr = (char *)malloc(pblk.b_esl);
				op->str.len = pblk.b_esl;
				memcpy(op->str.addr, pblk.buffer, pblk.b_esl);
			}
		}
	}
	ARLINK_ONLY(TREF(arlink_enabled) = arlink_enabled);	/* Set if any zro entry is enabled for autorelink */
	(TREF(set_zroutines_cycle))++;			/* Signal need to recompute zroutines histories for each linked routine */
}
