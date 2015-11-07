/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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
#include "zroutines.h"
#include "cmd_qlf.h"
#include "parse_file.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "op.h"
#include "incr_link.h"
#include "compiler.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

#define SRC			1
#define OBJ			2
#define NOTYPE			3

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

#define CLOSE_OBJECT_FD(FD, STATUS)	CLOSEFILE_RESET(FD, STATUS)       /* resets "object_file_des" to FD_INVALID */

GBLREF spdesc			stringpool;
GBLREF command_qualifier	glb_cmd_qlf, cmd_qlf;
GBLREF mval			dollar_zsource;
GBLREF int			object_file_des;

error_def(ERR_WILDCARD);
error_def(ERR_VERSION);
error_def(ERR_FILENOTFND);
error_def(ERR_ZLINKFILE);
error_def(ERR_ZLNOOBJECT);
error_def(ERR_FILEPARSE);
error_def(ERR_TEXT);
ZOS_ONLY(error_def(ERR_BADTAG);)

/* Routine to locate object files, or source and compile to an object file if necessary, and drive the linker to link the file
 * into this process appropriately. Three types of linking are currently supported on UNIX platforms (excepting Linux i386 which
 * has its own less fully-featured linker):
 *
 * 1. Link into process private - Executable code becomes part of the process private space.
 * 2. Link from a shared library - M routines linked into a shared library can be linked into a process allowing much of the
 *    object file to be shared.
 * 3. Link from a shared object - Current mechanism is to mmap() the object file and link it similar to how shared library links
 *    are done.
 *
 * Parameters:
 *   - v     - mval containing the name/path of the object file.
 *   - quals - mval containing the ZLINK command options (see GT.M User's Guide).
 */
void op_zlink (mval *v, mval *quals)
{
	int			status, qlf, tslash;
	unsigned short		type;
	char			srcnamebuf[MAX_FBUFF + 1], objnamebuf[MAX_FBUFF + 1], *fname;
	uint4			objnamelen, srcnamelen;
	char			inputf[MAX_FBUFF + 1], obj_file[MAX_FBUFF + 1], list_file[MAX_FBUFF + 1],
				ceprep_file[MAX_FBUFF + 1];
	zro_ent			*srcdir, *objdir;
	mstr			srcstr, objstr, file;
	boolean_t		compile, expdir, obj_found, src_found;
	parse_blk		pblk;
	mval			qualifier;
	struct stat		obj_stat, src_stat;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	if (!v->str.len || (MAX_FBUFF < v->str.len))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);
	object_file_des = FD_INVALID;
	srcdir = objdir = NULL;
	expdir = FALSE;
	if (quals)
	{	/* Explicit ZLINK */
		memset(&pblk, 0, SIZEOF(pblk));
		pblk.buff_size = MAX_FBUFF;
		pblk.buffer = inputf;
		pblk.fop = F_SYNTAXO;
		status = parse_file(&v->str, &pblk);
		if (!(status & 1))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, v->str.len, v->str.addr, status);
		if (pblk.fnb & F_WILD)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
				ERR_WILDCARD, 2, v->str.len, v->str.addr);
		file.addr = pblk.buffer;
		file.len = pblk.b_esl;
		type = NOTYPE;
		expdir = (0 != (pblk.fnb & F_HAS_DIR));
		if (pblk.b_ext)
		{
			file.len -= pblk.b_ext;
			if (('o' == pblk.l_ext[1]) && (2 == pblk.b_ext))
				type = OBJ;
			else
				type = SRC;
		}
		if (!expdir)
		{
			file.addr = pblk.l_name;
			file.len = pblk.b_name;
		}
		ENSURE_STP_FREE_SPACE(file.len);
		memcpy(stringpool.free, file.addr, file.len);
		dollar_zsource.str.addr = (char *) stringpool.free;
		dollar_zsource.str.len = file.len;
		stringpool.free += file.len;
		if (OBJ == type)
		{
			memmove(objnamebuf,file.addr, file.len + pblk.b_ext);
			objnamelen = file.len + pblk.b_ext;
			assert (objnamelen <= MAX_FBUFF + 1);
			objnamebuf[objnamelen] = 0;
		} else if (SRC == type)
		{
			memmove(srcnamebuf, file.addr,file.len + pblk.b_ext);
			srcnamelen = file.len + pblk.b_ext;
			assert (srcnamelen <= MAX_FBUFF + 1);
			srcnamebuf[srcnamelen] = 0;
			objnamelen = file.len;
			if (pblk.b_name > MAX_MIDENT_LEN)
				objnamelen = expdir ? (pblk.b_dir + MAX_MIDENT_LEN) : MAX_MIDENT_LEN;
			memcpy(objnamebuf, file.addr, objnamelen);
			memcpy(&objnamebuf[objnamelen], DOTOBJ, SIZEOF(DOTOBJ));
			objnamelen += STR_LIT_LEN(DOTOBJ);
			assert (objnamelen + SIZEOF(DOTOBJ) <= MAX_FBUFF + 1);
		} else
		{
			if ((file.len + SIZEOF(DOTM) > SIZEOF(srcnamebuf)) ||
			    (file.len + SIZEOF(DOTOBJ) > SIZEOF(objnamebuf)))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);
			memmove(srcnamebuf, file.addr, file.len);
			memcpy(&srcnamebuf[file.len], DOTM, SIZEOF(DOTM));
			srcnamelen = file.len + SIZEOF(DOTM) - 1;
			assert (srcnamelen + SIZEOF(DOTM) <= MAX_FBUFF + 1);
			memcpy(objnamebuf,file.addr,file.len);
			memcpy(&objnamebuf[file.len], DOTOBJ, SIZEOF(DOTOBJ));
			objnamelen = file.len + SIZEOF(DOTOBJ) - 1;
			assert (objnamelen + SIZEOF(DOTOBJ) <= MAX_FBUFF + 1);
		}
		if (!expdir)
		{
			srcstr.addr = srcnamebuf;
			srcstr.len = srcnamelen;
			objstr.addr = objnamebuf;
			objstr.len = objnamelen;
			if (OBJ == type)
			{
				zro_search(&objstr, &objdir, NULL, NULL, SKIP_SHLIBS);
				if (!objdir)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, dollar_zsource.str.len,
						dollar_zsource.str.addr, ERR_FILENOTFND,2, dollar_zsource.str.len,
						dollar_zsource.str.addr);
			} else if (SRC == type)
			{
				zro_search(&objstr, &objdir, &srcstr, &srcdir, SKIP_SHLIBS);
				if (!srcdir)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf,
						ERR_FILENOTFND, 2, srcnamelen, srcnamebuf);
			} else
			{
				zro_search(&objstr, &objdir, &srcstr, &srcdir, PROBE_SHLIBS);
				if (!objdir && !srcdir)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, dollar_zsource.str.len,
						dollar_zsource.str.addr, ERR_FILENOTFND,2, dollar_zsource.str.len,
						dollar_zsource.str.addr);
			}
		}
	} else
	{	/* auto-ZLINK */
		type = NOTYPE;
		memcpy(srcnamebuf, v->str.addr, v->str.len);
		memcpy(&srcnamebuf[v->str.len], DOTM, SIZEOF(DOTM));
		srcnamelen = v->str.len + SIZEOF(DOTM) - 1;
		if ('%' == srcnamebuf[0])
			srcnamebuf[0] = '_';
		memcpy(objnamebuf, srcnamebuf, v->str.len);
		memcpy(&objnamebuf[v->str.len], DOTOBJ, SIZEOF(DOTOBJ));
		objnamelen = v->str.len + SIZEOF(DOTOBJ) - 1;
		srcstr.addr = srcnamebuf;
		srcstr.len = srcnamelen;
		objstr.addr = objnamebuf;
		objstr.len = objnamelen;
		zro_search(&objstr, &objdir, &srcstr, &srcdir, PROBE_SHLIBS);
		if (!objdir && !srcdir)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
				ERR_FILENOTFND, 2, v->str.len, v->str.addr);
		qualifier.mvtype = MV_STR;
		qualifier.str = TREF(dollar_zcompile);
		quals = &qualifier;
	}
	if (OBJ == type)
	{
		if (objdir)
		{
			assert(ZRO_TYPE_OBJLIB != objdir->type);
			if (objdir->str.len + objnamelen + 2 > SIZEOF(objnamebuf))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);
			if (objdir->str.len)
			{
				tslash = ('/' == objdir->str.addr[objdir->str.len - 1]) ? 0 : 1;
				memmove(&objnamebuf[ objdir->str.len + tslash], objnamebuf, objnamelen);
				if (tslash)
					objnamebuf[ objdir->str.len ] = '/';
				memcpy(objnamebuf, objdir->str.addr, objdir->str.len);
				objnamelen += objdir->str.len + tslash;
				objnamebuf[ objnamelen ] = 0;
			}
		}
		OPEN_OBJECT_FILE(objnamebuf, O_RDONLY, object_file_des);
		if (FD_INVALID == object_file_des)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, dollar_zsource.str.len, dollar_zsource.str.addr,
			errno);
		if (IL_RECOMPILE == incr_link(object_file_des, NULL, objnamelen, objnamebuf))
		{
			CLOSE_OBJECT_FD(object_file_des, status);	/* No checking for error here as the priority error
									 * right now is the version issue.
									 */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, dollar_zsource.str.len, dollar_zsource.str.addr,
				ERR_VERSION);
		}
		CLOSE_OBJECT_FD(object_file_des, status);
		if (-1 == status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, dollar_zsource.str.len, dollar_zsource.str.addr,
			errno);
	} else
	{	/* either NO type or SOURCE type */
		cmd_qlf.object_file.str.addr = obj_file;
		cmd_qlf.object_file.str.len = MAX_FBUFF;
		cmd_qlf.list_file.str.addr = list_file;
		cmd_qlf.list_file.str.len = MAX_FBUFF;
		cmd_qlf.ceprep_file.str.addr = ceprep_file;
		cmd_qlf.ceprep_file.str.len = MAX_FBUFF;
		if (srcdir)
		{
			assert(ZRO_TYPE_OBJLIB != objdir->type);
			if (srcdir->str.len + srcnamelen > SIZEOF(srcnamebuf) - 1)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);
			if (srcdir->str.len)
			{
				tslash = ('/' == srcdir->str.addr[srcdir->str.len - 1]) ? 0 : 1;
				memmove(&srcnamebuf[ srcdir->str.len + tslash ], srcnamebuf, srcnamelen);
				if (tslash)
					srcnamebuf[ srcdir->str.len ] = '/';
				memcpy(srcnamebuf, srcdir->str.addr, srcdir->str.len);
				srcnamelen += srcdir->str.len + tslash;
				srcnamebuf[ srcnamelen ] = 0;
			}
		}
		if (objdir)
		{
			if (ZRO_TYPE_OBJLIB == objdir->type)
			{
				assert(objdir->shrlib);
				assert(objdir->shrsym);
				/* The incr_link() routine should drive errors for any issue found with linking from a shared
				 * library so IL_DONE is the only valid return code we *ever* expect back.
				 */
				assertpro(IL_DONE == incr_link(0, objdir, objnamelen, objnamebuf));
				return;
			}
			if (objdir->str.len + objnamelen > SIZEOF(objnamebuf) - 1)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);
			if (objdir->str.len)
			{
				tslash = ('/' == objdir->str.addr[objdir->str.len - 1]) ? 0 : 1;
				memmove(&objnamebuf[ objdir->str.len + tslash ], objnamebuf, objnamelen);
				if (tslash)
					objnamebuf[ objdir->str.len ] = '/';
				memcpy(objnamebuf, objdir->str.addr, objdir->str.len);
				objnamelen += objdir->str.len + tslash;
				objnamebuf[ objnamelen ] = 0;
			}
		}
		src_found = obj_found = compile = FALSE;
		if (SRC != type)
		{
			OPEN_OBJECT_FILE(objnamebuf, O_RDONLY, object_file_des);
			if (FD_INVALID == object_file_des)
			{
				if (ENOENT == errno)
					obj_found = FALSE;
				else
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, errno);
			} else
				obj_found = TRUE;
		} else
			compile = TRUE;
		STAT_FILE(srcnamebuf, &src_stat, status);
		if (-1 == status)
		{
			if ((ENOENT == errno) && (SRC != type))
				src_found = FALSE;
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE,2,srcnamelen,srcnamebuf,errno);
		} else
			src_found = TRUE;
		if (SRC != type)
		{
			if (src_found)
			{
				if (obj_found)
				{
					FSTAT_FILE(object_file_des, &obj_stat, status);
					if (-1 == status)
						rts_error_csa(CSA_ARG(NULL)VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf,
							      errno);
					if ((src_stat.st_mtime > obj_stat.st_mtime)
						|| ((src_stat.st_mtime == obj_stat.st_mtime)
							&& (src_stat.st_nmtime > obj_stat.st_nmtime)))
					{
						CLOSE_OBJECT_FD(object_file_des, status);
						if (-1 == status)
							rts_error_csa(CSA_ARG(NULL)VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen,
								      objnamebuf, errno);
						compile = TRUE;
					}
				} else
					compile = TRUE;
			} else if (!obj_found)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, objnamelen, objnamebuf,
					ERR_FILENOTFND, 2, objnamelen, objnamebuf);
		}
		if (compile)
		{
			zl_cmd_qlf(&quals->str, &cmd_qlf);
			if (!MV_DEFINED(&cmd_qlf.object_file))
			{
				cmd_qlf.object_file.mvtype = MV_STR;
				cmd_qlf.object_file.str.addr = objnamebuf;
				cmd_qlf.object_file.str.len = objnamelen;
			}
			qlf = cmd_qlf.qlf;
			if (!(cmd_qlf.qlf & CQ_OBJECT) && (SRC != type))
			{
				cmd_qlf.qlf = glb_cmd_qlf.qlf;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, ERR_ZLNOOBJECT);
			}
			zlcompile(srcnamelen, (uchar_ptr_t)srcnamebuf);
			if ((SRC == type) && !(qlf & CQ_OBJECT))
				return;
		}
		status = incr_link(object_file_des, objdir, objnamelen, objnamebuf);
		if (IL_RECOMPILE == status)
		{	/* due only to version mismatch, so recompile */
			CLOSE_OBJECT_FD(object_file_des, status);
			if (-1 == status)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, errno);
			assertpro(!compile);
			if (!src_found)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, ERR_VERSION);
			zl_cmd_qlf(&quals->str, &cmd_qlf);
			if (!MV_DEFINED(&cmd_qlf.object_file))
			{
				cmd_qlf.object_file.mvtype = MV_STR;
				cmd_qlf.object_file.str.addr = objnamebuf;
				cmd_qlf.object_file.str.len = objnamelen;
			}
			zlcompile(srcnamelen, (uchar_ptr_t)srcnamebuf);
			if (!(cmd_qlf.qlf & CQ_OBJECT) && (SRC != type))
			{
				cmd_qlf.qlf = glb_cmd_qlf.qlf;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, ERR_ZLNOOBJECT);
			}
			/* We just did a fresh re-compile a few lines above so IL_DONE is the only return code we ever
			 * expect to see back. Only a race-condition created by a different version overlaying the newly
			 * created object file could conceivably cause an IL_RECOMPILE code here (incr_link handles all
			 * the other errors itself). Not at this time considered worthy of special coding.
			 */
			assertpro(IL_DONE == incr_link(object_file_des, objdir, objnamelen, objnamebuf));
		}
		CLOSE_OBJECT_FD(object_file_des, status);
		if (-1 == status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, errno);
	}
}
