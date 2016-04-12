/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_limits.h"
#include <errno.h>

#include "eintr_wrappers.h"
#include "error.h"
#include "lv_val.h"		/* Needed for "fgncal.h" */
#include "fgncal.h"
#include "min_max.h"
#ifdef AUTORELINK_SUPPORTED
# include "rtnhdr.h"		/* Defines zro_hist * type for return */
# include "parse_file.h"	/* Needed for zro_search_hist() */
#endif
#ifdef DEBUG
#include "toktyp.h"		/* Needed for "valid_mname.h" */
#include "valid_mname.h"
#endif
#include "zroutines.h"
#include "arlinkdbg.h"

error_def(ERR_FILEPARSE);
error_def(ERR_SYSCALL);
error_def(ERR_WILDCARD);
error_def(ERR_ZFILENMTOOLONG);
error_def(ERR_ZLINKFILE);

/* Routine to perform a search of the $ZROUTINES structures (zro_ent) for a given routine source and/or object.
 *
 * Run through the zro_ent structures (source and/or object depending on which args have values). If looking for
 * both, find a related pair (i.e. we don't find objects for unrelated sources or vice versa). The zro_ent
 * structure list is in the following format:
 *   a. An object directory (which itself can be the source directory too if no source directories explicitly
 *      specified - example a zro_ent such as "obj(src)" has one object and one source dir where a spec such as
 *      "dir" is both an object and source directory).
 *   b. 0 or more related source directories
 * This then repeats until the end of the list (the ZRO_TYPE_COUNT records tell how many records exist of the given
 * type).
 *
 * Parameters:
 *   objstr	- If NULL, do not search for object, else pointer to object file text string.
 *   objdir	- NULL if objstr is NULL, otherwise is return pointer to associated object directory
 *		  (Note objdir is set to NULL if object directory is not found).
 *   srcstr	- Like objstr, except for associated source program.
 *   srcdir	- Like objdir, except for associated source program directory.
 *   skip_shlib	- If TRUE, skip over shared libraries. If FALSE, probe shared libraries.
 *
 * No return value.
 */
void zro_search(mstr *objstr, zro_ent **objdir, mstr *srcstr, zro_ent **srcdir, boolean_t skip_shlib)
{
	uint4			status;
	zro_ent			*op, *sp, *op_result, *sp_result;
	char			objfn[PATH_MAX], srcfn[PATH_MAX], *obp, *sbp, save_char;
	int			objcnt, srccnt;
	struct stat		outbuf;
	int			stat_res;
	mstr			rtnname;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!TREF(zro_root))
		zro_init();
	assert((NULL != objstr) || (NULL != srcstr));	/* Must search for object or source or both */
	assert((NULL == objstr) || (NULL != objdir));	/* If object text, then must have pointer for result */
	assert((NULL == srcstr) || (NULL != srcdir));	/* If source text, then must have pointer for result */
	assert(ZRO_TYPE_COUNT == (TREF(zro_root))->type);
	op_result = sp_result = NULL;
	objcnt = (TREF(zro_root))->count;
	assert(0 < objcnt);
	for (op = TREF(zro_root) + 1; !op_result && !sp_result && (0 < objcnt--); )
	{
		assert((ZRO_TYPE_OBJECT == op->type) || (ZRO_TYPE_OBJLIB == op->type));
		if (NULL != objstr)
		{
			if (ZRO_TYPE_OBJLIB == op->type)
			{
				if (!skip_shlib)
				{
					assert(op->shrlib);
					rtnname.len = objstr->len - (int)STR_LIT_LEN(DOTOBJ);
					memcpy(objfn, objstr->addr, rtnname.len);
					objfn[rtnname.len] = 0;
					rtnname.addr = objfn;
#					ifdef DEBUG
					save_char = rtnname.addr[0];
					if ('_' == save_char)
						rtnname.addr[0] = '%';
							/* Temporary adjustment for "valid_mname" to not assert fail */
					assert(valid_mname(&rtnname));
					if ('_' == save_char)
						rtnname.addr[0] = save_char;	/* restore */
#					endif
					if (NULL != (op->shrsym = (void *)fgn_getrtn(op->shrlib, &rtnname, SUCCESS)))
						/* Note assignment above */
						op_result = op;
				}
				op++;
				continue;
			}
			if ((op->str.len + objstr->len + 2) > SIZEOF(objfn))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZFILENMTOOLONG, 2, op->str.len, op->str.addr);
			obp = &objfn[0];
			if (op->str.len)
			{
				memcpy(obp, op->str.addr, op->str.len);
				obp += op->str.len;
				*obp++ = '/';
			}
			memcpy(obp, objstr->addr, objstr->len);
			obp += objstr->len;
			*obp++ = 0;
			STAT_FILE(objfn, &outbuf, stat_res);
			if (-1 == stat_res)
			{
				if (errno != ENOENT)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("stat"), CALLFROM,
						      errno);
			} else
				op_result = op;
		}
		if (srcstr)
		{
			sp = op + 1;
			if (ZRO_TYPE_OBJLIB == op->type)
			{
				op = sp;
				continue;
			}
			assert(ZRO_TYPE_COUNT == sp->type);
			srccnt = (sp++)->count;
			for ( ; !sp_result && srccnt-- > 0; sp++)
			{
				assert(sp->type == ZRO_TYPE_SOURCE);
				if (sp->str.len + srcstr->len + 2 > SIZEOF(srcfn)) /* Extra 2 for '/' & null */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZFILENMTOOLONG, 2, sp->str.len, sp->str.addr);
				sbp = &srcfn[0];
				if (sp->str.len)
				{
					memcpy (sbp, sp->str.addr, sp->str.len);
					sbp += sp->str.len;
					*sbp++ = '/';
				}
				memcpy(sbp, srcstr->addr, srcstr->len);
				sbp += srcstr->len;
				*sbp++ = 0;
				STAT_FILE(srcfn, &outbuf, stat_res);
				if (-1 == stat_res)
				{
					if (ENOENT != errno)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("stat"),
							      CALLFROM, errno);
				} else
				{
					sp_result = sp;
					op_result = op;
				}
			}
			op = sp;
		} else
		{
			op++;
			assert(ZRO_TYPE_COUNT == op->type);
			op += op->count;
			op++;
		}
	}
	if (NULL != objdir)
		*objdir = op_result;
	if (NULL != srcdir)
		*srcdir = sp_result;
	return;
}

#ifdef AUTORELINK_SUPPORTED
/* Routine to take a given object file path and create the autorelink search history for that object and return the zro_ent
 * structure associated with the object file with the following caveats:
 *   1. If the file is in a $ZROUTINES entry that is not autorelink-enabled, no history is returned.
 *   2. Directories that are not autorelink-enabled do not show up in the history.
 *   3. If the directory is not one that is currently in $ZROUTINES, no history is returned (special case of #2).
 *
 * Parameters:
 *
 *   objstr - Address of mstr containing full path of the object file
 *   objdir - Store *addr of the zroutines entry addr (if non-NULL). Stores NULL if not found.
 *
 * Return value:
 *
 *   Address of search history block (if NULL, *objdir is also NULL)
 */
zro_hist *zro_search_hist(char *objnamebuf, zro_ent **objdir)
{
	uint4			status;
	parse_blk		pblk;
	zro_ent			*op, *op_result;
	mstr			objstr, dirpath, rtnname, zroentname;
	int			objcnt;
	zro_search_hist_ent	zhent_base[ZRO_MAX_ENTS];
	zro_search_hist_ent	*zhent;
	zro_hist		*recent_zhist;
	char			obj_file[MAX_FBUFF + 1];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	objstr.addr = objnamebuf;
	assert(NULL != objstr.addr);
	objstr.len = STRLEN(objstr.addr);
	assert(0 < objstr.len);
	DBGARLNK((stderr, "\n\nzro_search_hist: Entered for %s\n", objnamebuf));
	/* First parse our input string to isolate the directory name we will look up */
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buff_size = MAX_FBUFF;
	pblk.buffer = obj_file;
	pblk.fop = F_SYNTAXO;				/* Just a syntax parse */
	status = parse_file(&objstr, &pblk);
	if (!(status & 1))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, objstr.len, objstr.addr, status);
	if (pblk.fnb & F_WILD)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, objstr.len, objstr.addr,
			      ERR_WILDCARD, 2, objstr.len, objstr.addr);
	dirpath.len = pblk.b_dir;			/* Create mstr describing the object file's dirpath */
	dirpath.addr = pblk.l_dir;
	/* Remove any trailing '/' in the directory name since the directories in the zro_ent blocks have none
	 * so we need to match their style. Note this means if for whatever unfathomable reason there is a
	 * zro_ent for the root directory, the length can be zero so allow for that.
	 */
	if ('/' == *(dirpath.addr + dirpath.len - 1))
		dirpath.len--;
	assert((0 <= dirpath.len) && (NULL != dirpath.addr));
	/* Build the routine-name mstr that contains only the routine name */
	rtnname.addr = pblk.l_name;
	rtnname.len = pblk.b_name;
	assert((0 < rtnname.len) && (NULL != rtnname.addr));
	CONVERT_FILENAME_TO_RTNNAME(rtnname);
	assert(valid_mname(&rtnname));
	DBGARLNK((stderr, "zro_search_hist: Looking for dir %.*s for routine %.*s\n", dirpath.len, dirpath.addr,
		  rtnname.len, rtnname.addr));
	/* Now lets locate it in the parsed $ZROUTINES block list while creating some history */
	if (!TREF(zro_root))
		zro_init();
	op_result = NULL;
	objcnt = (TREF(zro_root))->count;
	assert(0 < objcnt);
	zhent = &zhent_base[0];				/* History initialization */
	for (op = TREF(zro_root) + 1; (0 < objcnt--);)
	{	/* Once through each zro_ent block in our array only looking at object directory type entries */
		assert((ZRO_TYPE_OBJECT == op->type) || (ZRO_TYPE_OBJLIB == op->type));
		if (ZRO_TYPE_OBJLIB == op->type)
			continue;			/* We only deal with object directories in this loop */
		/* If this directory is autorelink enabled, add it to the history */
		if (NULL != op->relinkctl_sgmaddr)
			zro_record_zhist(zhent++, op, &rtnname);
		/* In order to properly match the entries, we need to normalize them. The zro_ent name may or may not
		 * have a trailing '/' in the name. The dirpath value won't have a trailing '/'.
		 */
		zroentname = op->str;
		DBGARLNK((stderr, "zro_search_hist: recsleft: %d  current dirblk: %.*s\n", objcnt, zroentname.len,
			  zroentname.addr));
		if ((0 < zroentname.len) && ('/' == *(zroentname.addr + zroentname.len - 1)))
			zroentname.len--;
		if (MSTR_EQ(&dirpath, &zroentname))
		{
			op_result = op;
			break;
		}
		/* Bump past source entries to next object entry */
		op++;
		assert(ZRO_TYPE_COUNT == op->type);
		op += op->count;
		op++;
	}
	if (NULL != objdir)
		*objdir = op_result;
	/* If either of we didn't find the directory in the $ZROUTINES list (so it couldn't be auto-relink enabled or
	 * we found the directory but it wasn't auto-relink enabled, we need return no history or zro_ent value.
	 */
	if ((NULL == op_result) || (NULL == op_result->relinkctl_sgmaddr))
		return NULL;
	/* We found the routine in an auto-relink enabled directory so create/return the history and the zro_ent
	 * structure we found it in.
	 */
	recent_zhist = zro_zhist_saverecent(zhent, &zhent_base[0]);
	return recent_zhist;
}
#endif
