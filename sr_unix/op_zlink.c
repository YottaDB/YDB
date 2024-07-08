/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include <errno.h>

#include "stringpool.h"
#ifdef AUTORELINK_SUPPORTED
# include "rtnhdr.h"
#endif
#include "zroutines.h"
#include "cmd_qlf.h"
#include "parse_file.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "op.h"
#include "incr_link.h"
#include "compiler.h"
#ifdef __MVS__
# include "gtm_zos_io.h"
#endif
#include "arlinkdbg.h"
#include "relinkctl.h"
#include "toktyp.h"		/* Needed for "valid_mname.h" */
#include "valid_mname.h"
#include "restrict.h"

typedef enum
{
	SRC = 1,
	OBJ,
	NOTYPE
} linktyp;

/* On shared platforms, skip parameter should be FALSE to indicate an auto-ZLINK so that
 * zro_search looks into shared libraries. On non-shared platforms, it should be
 * TRUE to instruct zro_search to always skip shared libraries
 */
#define SKIP_SHLIBS	TRUE
#ifdef USHBIN_SUPPORTED
#define PROBE_SHLIBS	(!SKIP_SHLIBS) /* i.e., don't skip (skip = FALSE) */
#else
#define PROBE_SHLIBS	SKIP_SHLIBS
#endif

/* On certain platforms the st_mtime field of the stat structure got replaced by a timespec st_mtim field, which in turn has tv_sec
 * and tv_nsec fields. For compatibility reasons, those platforms define an st_mtime macro which points to st_mtim.tv_sec. Whenever
 * we detect such a situation, we define a nanosecond flavor of that macro to point to st_mtim.tv_nsec. On HPUX Itanium and older
 * AIX boxes the stat structure simply has additional fields with the nanoseconds value, yet the names of those field are different
 * on those two architectures, so we choose our mapping accordingly.
 */
#if defined st_mtime
#  define st_nmtime		st_mtim.tv_nsec
#elif defined(_AIX)
#  define st_nmtime		st_mtime_n
#elif defined(__hpux) && defined(__ia64)
#  define st_nmtime		st_nmtime
#endif
/* Macro to close object file and give appropriate error */
#define CLOSE_OBJECT_FD(FD, STATUS)												\
{																\
	CLOSE_OBJECT_FILE(FD, STATUS);	/* Resets "object_file_des" to FD_INVALID */						\
	if (-1 == (STATUS))													\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("close()"), CALLFROM, errno);	\
}

#ifdef AUTORELINK_SUPPORTED
#  define CHECK_OBJECT_HISTORY(OBJPATH, OBJDIR, RECENT_ZHIST_PARM)								\
{																\
	if (TREF(arlink_enabled) && !TREF(trigger_compile_and_link))								\
	{	/* Autorelink is enabled, this is not a trigger and we need a search history for the object file to pass	\
		 * to incr_link(). We had to wait till this point to discover the search history since at the time of the	\
		 * call to zro_search(), there may not have been an object file to find or, given where we found the source,	\
		 * the object file we are linking now may be different from one found before so create the history array given  \
		 * the supplied object file path and our $ZROUTINES directory array. See GTM-8311 for potential improvements	\
		 * in this area.												\
		 *														\
		 * Note we cannot assert RECENT_ZHIST_PARM is null here as we may be about to link a recompiled module after	\
		 * a failed ZLINK but incr_link() took care of releasing the old history in its error path.			\
		 */														\
		RECENT_ZHIST_PARM = zro_search_hist(OBJPATH, &OBJDIR);								\
	}															\
}
#else
#  define CHECK_OBJECT_HISTORY(OBJPATH, OBJDIR, RECENT_ZHIST_PARM)
#endif
# define COMBINE_OBJ_DIR_W_NAME()												\
MBSTART{															\
	if (objdir && objdir->str.len)												\
	{															\
		assert((objdir->str.len + tslash + module_name.len) < MAX_FN_LEN);						\
		assert('/' == objnamebuf[objdir->str.len + tslash - 1]);							\
		memcpy(&objnamebuf[objdir->str.len + tslash], module_name.addr, module_name.len);				\
		objnamelen = objdir->str.len + tslash + module_name.len;							\
		memcpy(&objnamebuf[objnamelen], DOTOBJ, SIZEOF(DOTOBJ));	/* Copies null terminator */			\
		objnamelen += (SIZEOF(DOTOBJ) - 1);										\
	}															\
	assert(objnamelen && (objnamelen <= MAX_FN_LEN));									\
	cmd_qlf.object_file.mvtype = MV_STR;											\
	memcpy(cmd_qlf.object_file.str.addr, objnamebuf, objnamelen);								\
	cmd_qlf.object_file.str.len = objnamelen;										\
	cmd_qlf.object_file.str.addr[cmd_qlf.object_file.str.len] = 0;								\
} MBEND


GBLREF command_qualifier	cmd_qlf;
GBLREF int			object_file_des;
GBLREF mident			module_name, routine_name;
GBLREF mval			dollar_zsource;
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF unsigned char		object_file_name[];
GBLREF unsigned short		object_name_len;

error_def(ERR_FILENOTFND);
error_def(ERR_FILEPARSE);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_VERSION);
error_def(ERR_WILDCARD);
error_def(ERR_ZLINKFILE);
error_def(ERR_ZLNOOBJECT);
ZOS_ONLY(error_def(ERR_BADTAG);)

/* Routine to locate object files, or source and compile to an object file if necessary, and drive the linker to link the file
 * into this process appropriately. Three types of linking are currently supported on UNIX platforms (excepting Linux i386 which
 * has its own less fully-featured linker):
 *
 * 1. Link into process private - Executable code becomes part of the process private space.
 * 2. Link from a shared library - M routines linked into a shared library can be linked into a process allowing much of the
 *    object file to be shared.
 * 3. Link from a shared object - Shared objects are loaded into shared memory by GT.M (rtnobj.c is the manager) and linked
 *    much like objects linked from a shared library.
 *
 * Parameters:
 *   - v     - mval containing the name/path of the object file.
 *   - quals - mval containing the ZLINK command options (see GT.M User's Guide).
 */
void op_zlink (mval *v, mval *quals)
{
	boolean_t		compile, expdir, obj_found, src_found;
	char			*err_code, *fname,
				ceprep_file[MAX_FN_LEN + 1],
				inputf[MAX_FN_LEN + 1],
				list_file[MAX_FN_LEN + 1],
				obj_file[MAX_FN_LEN + 1],
				objnamebuf[MAX_FN_LEN + 1],
				srcnamebuf[MAX_FN_LEN + 1];
	int			initial_object_file_des, qlf, save_errno, status, tslash = 0;
	linktyp			type;
	mstr			srcstr, objstr, file;
	parse_blk		pblk;
	struct stat		obj_stat, src_stat;
	unsigned short		objnamelen, srcnamelen;
	size_t			cpy_len;
	zro_ent			*srcdir, *objdir;
	ARLINK_ONLY(zro_hist	*recent_zhist;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((NULL != quals) && RESTRICTED(zlink_op))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "ZLINK");
	ARLINK_ONLY(recent_zhist = NULL);
	MV_FORCE_STR(v);
	if (!v->str.len)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Filename/path is missing"));
	assert(0 <= v->str.len);
	if ((0 > v->str.len) || (MAX_FN_LEN < v->str.len))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, (0 < v->str.len) ? MAX_FN_LEN : 0, v->str.addr,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("Filename/path exceeds max length"));
	DBGARLNK((stderr, "op_zlink: Call to (re)link routine %.*s\n", v->str.len, v->str.addr));
	assert((SIZEOF(DOTM) == SIZEOF(DOTOBJ)) && (SIZEOF(srcnamebuf) == SIZEOF(objnamebuf)));
	object_file_des = FD_INVALID;
	srcdir = objdir = NULL;
	expdir = FALSE;
	module_name.len = object_name_len = 0;
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buff_size = MAX_FN_LEN;
	pblk.buffer = inputf;
	pblk.fop = F_SYNTAXO;
	status = parse_file(&v->str, &pblk);
	if (!(status & 1))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_FILEPARSE, 2, v->str.len, v->str.addr, status);
	if (pblk.fnb & F_WILD)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
			ERR_WILDCARD, 2, v->str.len, v->str.addr);
	/* 4SCA: file.len is bounded by MAX_FN_LEN in parse_file() */
	file.addr = pblk.buffer;
	file.len = pblk.b_esl;
	type = NOTYPE;
	if (pblk.b_ext)
	{	/* 4SCA: file.len is decremented by the extension length pblk.b_ext */
		file.len -= pblk.b_ext;
		type = (('o' == pblk.l_ext[1]) && (2 == pblk.b_ext)) ? OBJ : SRC;
	}
	srcnamelen = MIN(v->str.len, MAX_FN_LEN);
	memcpy(srcnamebuf, v->str.addr, srcnamelen);
	if ('%' == srcnamebuf[0])
		srcnamebuf[0] = '_';
	INIT_CMD_QLF_STRINGS(cmd_qlf, obj_file, list_file, ceprep_file, MAX_FN_LEN);
	if (!TREF(trigger_compile_and_link))						/* triggers don't rely on $ZCOMPILE */
		zl_cmd_qlf(&(TREF(dollar_zcompile)), &cmd_qlf, srcnamebuf, &srcnamelen, !quals);
	if (NULL != quals)	/* after initization w default quals, override with any actual quals */
		zl_cmd_qlf(&quals->str, &cmd_qlf, srcnamebuf, &srcnamelen, TRUE);
	objnamelen = MIN(pblk.b_name, MAX_MIDENT_LEN);
	memcpy(objnamebuf, pblk.l_name, objnamelen);
	if ((!TREF(trigger_compile_and_link) && (objnamelen - object_name_len))
			|| memcmp(objnamebuf, object_file_name, objnamelen))
	{	/* appear to have an object name that differs from the file name */
		assert(MAX_MIDENT_LEN >= object_name_len);
		objnamelen = object_name_len;
		memcpy(objnamebuf, object_file_name, objnamelen);
	}
	if (NULL != quals)
	{	/* Explicit ZLINK from generated code or from gtm_trigger() */
		expdir = (0 != (pblk.fnb & F_HAS_DIR));
		if (!expdir)
		{	/* if the directory was not explicit, skip past it */
			file.addr = pblk.l_name;
			file.len = pblk.b_name;
		} else if (SRC != type)
		{	/* if we have an explicit, directory shift the object name to make room and fill in the directory */
			assert((OBJ == type) || (NOTYPE == type));
			cpy_len = objnamelen;
			assert(sizeof(objnamebuf) >= cpy_len);
			assert(sizeof(objnamebuf) >= (cpy_len + pblk.b_dir));
			if (sizeof(objnamebuf) < (cpy_len + pblk.b_dir)) /* 4SCA BYPASSOK */
				cpy_len = sizeof(objnamebuf) - pblk.b_dir;
			assert(256 >= cpy_len); /* For Veracode grins */
			memmove(&objnamebuf[pblk.b_dir], objnamebuf, cpy_len);
			/* assert(sizeof(objnamebuf) >= pblk.b_dir); */		/* Compiler complains that it is always true */
			memcpy(objnamebuf, file.addr, pblk.b_dir);
			objnamelen += pblk.b_dir;
		}
		assert(MAX_FN_LEN > (objnamelen + SIZEOF(DOTOBJ)));
		memcpy(&objnamebuf[objnamelen], DOTOBJ, SIZEOF(DOTOBJ));		/* Copies null terminator */
		objnamelen += STR_LIT_LEN(DOTOBJ);
		if (TREF(trigger_compile_and_link))
		{	/* trigger sources don't have a .m extension */
			assert(OBJ == type);
			srcnamelen -= (SIZEOF(DOTOBJ) - 1);
			srcnamebuf[srcnamelen] = 0;
		} else
		{	/* maintain $ZSOURCE */
			ENSURE_STP_FREE_SPACE(file.len);
			memcpy(stringpool.free, file.addr, file.len);
			dollar_zsource.str.addr = (char *)stringpool.free;
			dollar_zsource.str.len = file.len;
			stringpool.free += file.len;
			if (OBJ == type)
				memcpy(&srcnamebuf[srcnamelen], DOTM, SIZEOF(DOTM));	/* Copies null terminator */
		}
		if (SRC == type)
		{	/* source file extension specified */
			srcnamelen = file.len + pblk.b_ext;
			assert(MAX_FN_LEN >= srcnamelen);
			memcpy(srcnamebuf, file.addr, srcnamelen);
			srcnamebuf[srcnamelen] = 0;
		}
		if (NOTYPE == type)
		{	/* No file extension specified - object done above, so do source */
			if ((file.len + SIZEOF(DOTM)) > SIZEOF(srcnamebuf))
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Filename/path exceeds max length"));
			memcpy(srcnamebuf, file.addr, file.len);
			srcnamelen = file.len + SIZEOF(DOTM) - 1;
			assert(MAX_FN_LEN >= srcnamelen);
			memcpy(&srcnamebuf[file.len], DOTM, SIZEOF(DOTM));		/* Copies null terminator */
		}
		if (!expdir)
		{	/* No full or relative directory specified on file to link - fill it in */
			srcstr.addr = srcnamebuf;
			srcstr.len = srcnamelen;
			objstr.addr = objnamebuf;
			objstr.len = objnamelen;
			if (OBJ == type)
			{	/* Explicit ZLINK of object - don't locate source */
				zro_search(&objstr, &objdir, NULL, NULL, SKIP_SHLIBS);
				if (NULL == objdir)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, dollar_zsource.str.len,
						dollar_zsource.str.addr, ERR_FILENOTFND, 2, objnamelen, objnamebuf);
			} else if (SRC == type)
			{	/* Explicit ZLINK of source - locate both source and object*/
				zro_search(&objstr, &objdir, &srcstr, &srcdir, SKIP_SHLIBS);
				if (NULL == srcdir)
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, dollar_zsource.str.len,
						dollar_zsource.str.addr, ERR_FILENOTFND,2, srcnamelen, srcnamebuf);
			} else
			{	/* Explicit ZLINK no file type specified - locate both source and object */
				zro_search(&objstr, &objdir, &srcstr, &srcdir, PROBE_SHLIBS);
				if ((NULL == objdir) && (NULL == srcdir))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(12) ERR_ZLINKFILE, 2, dollar_zsource.str.len,
						dollar_zsource.str.addr, ERR_FILENOTFND,2, srcnamelen, srcnamebuf,
						ERR_FILENOTFND,2, objnamelen, objnamebuf);
			}
		}
	} else
	{	/* auto-ZLINK for execution, ZBREAK, $TEXT(), or ZEDIT (i.e. non-specific call to op_zlink()) */
		assert(NOTYPE == type);
		memcpy(srcnamebuf, pblk.l_name, pblk.b_name);
		memcpy(&srcnamebuf[pblk.b_name], DOTM, SIZEOF(DOTM));
		srcnamelen = pblk.b_name + SIZEOF(DOTM) - 1;
		if (!objnamelen)
		{
			objnamelen = pblk.b_name;
			memcpy(srcnamebuf, pblk.l_name, objnamelen);
		}
		assert(MAX_FN_LEN > (objnamelen + SIZEOF(DOTOBJ)));
		memcpy(&objnamebuf[objnamelen], DOTOBJ, SIZEOF(DOTOBJ));		/* Copies null terminator */
		objnamelen += (SIZEOF(DOTOBJ) - 1);
		if ('%' == srcnamebuf[0])
			srcnamebuf[0] = '_';
		srcstr.addr = srcnamebuf;
		srcstr.len = srcnamelen;
		objstr.addr = objnamebuf;
		objstr.len = objnamelen;
		zro_search(&objstr, &objdir, &srcstr, &srcdir, PROBE_SHLIBS);
		if (NULL == srcdir)
		{
			if (NULL == objdir)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(12) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
					ERR_FILENOTFND, 2, srcnamelen, srcnamebuf, ERR_FILENOTFND, 2, objnamelen, objnamebuf);
		} else if (NULL == objdir)
			type = SRC;
	}
	if (OBJ == type)
	{	/* Object file extension specified */
		if (objdir)
		{	/* Object file found via zro_search() */
			assert(ZRO_TYPE_OBJLIB != objdir->type);
			if ((objdir->str.len + objnamelen + 2) > (SIZEOF(objnamebuf) - 1))
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Filename/path exceeds max length"));
			if (objdir->str.len)
			{
				tslash = ('/' == objdir->str.addr[objdir->str.len - 1]) ? 0 : 1;
				memmove(&objnamebuf[objdir->str.len + tslash], objnamebuf, objnamelen);
				if (tslash)
				{
					assert(objnamelen + tslash <= MAX_FN_LEN + 1);
					objnamebuf[objdir->str.len] = '/';
				}
				/* 4SCA: Protected by rts_error above */
				memcpy(objnamebuf, objdir->str.addr, objdir->str.len);
				objnamelen += objdir->str.len + tslash;
				objnamebuf[objnamelen] = 0;
			}
		}
		OPEN_OBJECT_FILE(objnamebuf, O_RDONLY, object_file_des);
		if (FD_INVALID == object_file_des)
		{	/* Could not find object file */
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, errno);
		}
		/* Note - if explicit ZLINK, objdir can be NULL if link is from a directory not mentioned in $ZROUTINES */
		CHECK_OBJECT_HISTORY(objnamebuf, objdir, RECENT_ZHIST);
		if (IL_RECOMPILE == INCR_LINK(&object_file_des, objdir, RECENT_ZHIST, objnamelen, objnamebuf))
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_ZLINKFILE, 2, v->str.len, v->str.addr, ERR_VERSION);
		}
		CLOSE_OBJECT_FD(object_file_des, status);
	} else
	{	/* Either NO file extension specified or is SOURCE file extension type */
		if (srcdir)
		{	/* A source directory containing routine was found by zro_search() */
			assert(ZRO_TYPE_OBJLIB != objdir->type);
			if ((srcdir->str.len + srcnamelen) > (SIZEOF(srcnamebuf) - 1))
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Filename/path exceeds max length"));
			if (srcdir->str.len)
			{
				tslash = ('/' == srcdir->str.addr[srcdir->str.len - 1]) ? 0 : 1;
				/* 4SCA: Protected by rts_error above */
				memmove(&srcnamebuf[srcdir->str.len + tslash], srcnamebuf, srcnamelen);
				if (tslash)
					srcnamebuf[srcdir->str.len] = '/';
				memcpy(srcnamebuf, srcdir->str.addr, srcdir->str.len);
				srcnamelen += srcdir->str.len + tslash;
				srcnamebuf[srcnamelen] = 0;
			}
		}
		if (objdir)
		{	/* An object directory or shared library containing the routine was found by zro_search() */
			if (ZRO_TYPE_OBJLIB == objdir->type)
			{	/* For object libraries, there are no auto-relink complications so no need to hunt down
				 * any history for it or to pass in any.
				 */
				assert(objdir->shrlib);
				assert(objdir->shrsym);
				/* The incr_link() routine should drive errors for any issue found with linking from a shared
				 * library so IL_DONE is the only valid return code we *ever* expect back.
				 */
				assertpro(IL_DONE == INCR_LINK(NULL, objdir, NULL, objnamelen, objnamebuf));
				return;
			}
			if ((objdir->str.len + objnamelen) > (SIZEOF(objnamebuf) - 1))
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Filename/path exceeds max length"));
			if (objdir->str.len)
			{
				tslash = ('/' == objdir->str.addr[objdir->str.len - 1]) ? 0 : 1;
				assert((objnamelen + objdir->str.len + tslash) <= (MAX_FN_LEN + 1));
				/* 4SCA: Protected by rts_error above */
				memmove(&objnamebuf[objdir->str.len + tslash], objnamebuf, objnamelen);
				if (tslash)
					objnamebuf[objdir->str.len] = '/';
				memcpy(objnamebuf, objdir->str.addr, objdir->str.len);
				objnamelen += objdir->str.len + tslash;
				objnamebuf[objnamelen] = 0;
			}
		}
		/* Check which source/object we ended up with and whether they are for the same file or not so we can determine if
		 * recompilation might be required.
		 */
		src_found = obj_found = compile = FALSE;
		if (SRC != type)
		{	/* Object or NO file extension specified - check object file exists */
			initial_object_file_des = object_file_des;
			OPEN_OBJECT_FILE(objnamebuf, O_RDONLY, object_file_des);
			if (FD_INVALID == object_file_des)
			{
				save_errno = errno;
				if (ENOENT != save_errno)
				{
					err_code = STRERROR(save_errno);
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, objnamelen, objnamebuf,
						ERR_TEXT, 2, LEN_AND_STR(err_code));
				}
			} else
				obj_found = TRUE;
		} else	/* If source file extension specified, force re-compile */
		{
			compile = TRUE;
			assert(FD_INVALID == object_file_des);	/* Shouldn't be an object file open yet */
		}
		STAT_FILE(srcnamebuf, &src_stat, status);	/* Check if source file exists */
		if (-1 == status)
		{
			save_errno = errno;
			if ((ENOENT == errno) && (SRC != type))
				src_found = FALSE;
			else
			{
				if (FD_INVALID != object_file_des)	/* Chose object file if open, ignore error */
					CLOSE_OBJECT_FILE(object_file_des, status);
				err_code = STRERROR(save_errno);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
					ERR_TEXT, 2, LEN_AND_STR(err_code));
			}
		} else
			src_found = TRUE;
		if (SRC != type)
		{	/* If object or no file extension type specified, check if have both source and object and if so, decide
			 * if source needs to be recompiled.
			 */
			if (src_found)
			{
				if (obj_found)
				{
					FSTAT_FILE(object_file_des, &obj_stat, status);
					if (-1 == status)
					{
						save_errno = errno;
						if (FD_INVALID != object_file_des)	/* Close object file if open */
							CLOSE_OBJECT_FILE(object_file_des, status);
						err_code = STRERROR(save_errno);
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
							ERR_TEXT, 2, LEN_AND_STR(err_code));
					}
					if ((src_stat.st_mtime > obj_stat.st_mtime) || ((src_stat.st_mtime == obj_stat.st_mtime)
						&& (src_stat.st_nmtime > obj_stat.st_nmtime)))
					{
						CLOSE_OBJECT_FD(object_file_des, status);
						compile = TRUE;
					}
				} else
				{
					compile = TRUE;
					assert(FD_INVALID == object_file_des);	/* Make sure no object file open */
				}
			} else if (!obj_found)
			{
				assert(FD_INVALID == object_file_des);	/* Make sure closed */
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
					ERR_FILENOTFND, 2, objnamelen, objnamebuf);
			}
		}
		if (compile)
		{	/* (Re)Compile source file */
			qlf = cmd_qlf.qlf;
			if (!(qlf & CQ_OBJECT) && (SRC != type))
			{
				assert(FD_INVALID == object_file_des);	/* Make sure no object file open */
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_ZLINKFILE, 2, v->str.len, v->str.addr, ERR_ZLNOOBJECT);
			}
			COMBINE_OBJ_DIR_W_NAME();
			zlcompile(srcnamelen, (uchar_ptr_t)srcnamebuf);
			assert(object_name_len <= MAX_FN_LEN);
			objnamelen = object_name_len;
			memcpy(objnamebuf, object_file_name, object_name_len);
			objnamebuf[object_name_len] = 0;
			assert(FD_INVALID == object_file_des);	/* zlcompile() should have driven obj_code() which closes object */
			assertpro(FALSE == TREF(compile_time) && (stringpool.base == rts_stringpool.base));	/* sb back in rts */
			if (!(cmd_qlf.qlf & CQ_OBJECT))
				return;
			OPEN_OBJECT_FILE(objnamebuf, O_RDONLY, object_file_des);
			if (FD_INVALID == object_file_des)
			{
				save_errno = errno;
				err_code = STRERROR(save_errno);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(10) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
					ERR_ZLNOOBJECT, 0, ERR_TEXT, 2, LEN_AND_STR(err_code));
			}
		} else
		{
			module_name.len = routine_name.len = MIN(v->str.len, MAX_MIDENT_LEN);
			memcpy(module_name.addr, v->str.addr, module_name.len);
			memcpy(routine_name.addr, v->str.addr, routine_name.len);
			CONVERT_FILENAME_TO_RTNNAME(routine_name);
		}
		assert(FD_INVALID != object_file_des);		/* Object file should be open at this point */
		CHECK_OBJECT_HISTORY(objnamebuf, objdir, RECENT_ZHIST);
		status = INCR_LINK(&object_file_des, objdir, RECENT_ZHIST, objnamelen, objnamebuf);
		if (IL_RECOMPILE == status)
		{	/* Failure due only to version mismatch, so recompile */
			assertpro(!compile);
			if (!src_found)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(9) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, ERR_FILENOTFND,
					2, srcnamelen, srcnamebuf, ERR_VERSION);
			if (!compile)
			{
				if (!TREF(trigger_compile_and_link))
					COMBINE_OBJ_DIR_W_NAME();
				else if (!MV_DEFINED(&cmd_qlf.object_file))
				{
					cmd_qlf.object_file.mvtype = MV_STR;
					cmd_qlf.object_file.str.addr = objnamebuf;
					cmd_qlf.object_file.str.len = objnamelen;
				}
				assert((MV_STR & cmd_qlf.object_file.mvtype) && cmd_qlf.object_file.str.len
					&& strlen(cmd_qlf.object_file.str.addr) == cmd_qlf.object_file.str.len);
			}
			zlcompile(srcnamelen, (uchar_ptr_t)srcnamebuf);
			assertpro(FALSE == TREF(compile_time) && (stringpool.base == rts_stringpool.base));	/* sb back in rts */
			if (!(cmd_qlf.qlf & CQ_OBJECT) && (SRC != type))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, v->str.len, v->str.addr, ERR_ZLNOOBJECT);
			assert(FD_INVALID == object_file_des);	/* zlcompile() should have driven obj_code() which closes it */
			OPEN_OBJECT_FILE(objnamebuf, O_RDONLY, object_file_des);
			CHECK_OBJECT_HISTORY(objnamebuf, objdir, RECENT_ZHIST);
			/* We just did a fresh re-compile a few lines above so IL_DONE is the only return code we ever
			 * expect to see back. Only a race-condition created by a different version overlaying the newly
			 * created object file could conceivably cause an IL_RECOMPILE code here (incr_link handles all
			 * the other errors itself). Not at this time considered worthy of special coding.
			 */
			status = INCR_LINK(&object_file_des, objdir, RECENT_ZHIST, objnamelen, objnamebuf);
			assertpro(IL_DONE == status);
		}
		CLOSE_OBJECT_FD(object_file_des, status);
	}
}
