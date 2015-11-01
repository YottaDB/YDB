/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

#define SRC			1
#define OBJ			2
#define NOTYPE			3

GBLREF spdesc			stringpool;
GBLREF command_qualifier	glb_cmd_qlf, cmd_qlf;
GBLREF mstr			dollar_zsource, dollar_zcompile;
GBLREF char			object_file_name[MAX_FBUFF + 1];
GBLREF short			object_name_len;

void op_zlink (mval *v, mval *quals)
{
	int			obj_desc, status, qlf, tslash, cntr;
	unsigned short		type;
	char			srcnamebuf[MAX_FBUFF + 1], objnamebuf[MAX_FBUFF + 1], *fname;
	uint4			objnamelen, srcnamelen;
	char			inputf[MAX_FBUFF + 1], obj_file[MAX_FBUFF + 1], list_file[MAX_FBUFF + 1],
				ceprep_file[MAX_FBUFF + 1];
	zro_ent			*srcdir, *objdir;
	mstr			srcstr, objstr, file;
	bool			expdir;
	parse_blk		pblk;
	mval			qualifier;
	struct stat		stat_buf;
	error_def		(ERR_WILDCARD);
	error_def		(ERR_VERSION);
	error_def		(ERR_FILENOTFND);
	error_def		(ERR_ZLINKFILE);
	error_def		(ERR_ZLNOOBJECT);
	error_def		(ERR_FILEPARSE);

	MV_FORCE_STR(v);
	if (!v->str.len || v->str.len > MAX_FBUFF)
		rts_error(VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);

	srcdir = objdir = (zro_ent *) 0;
	expdir = FALSE;
	if (quals)
	{ /* explicit ZLINK */
		memset(&pblk, 0, sizeof(pblk));
		pblk.buff_size = MAX_FBUFF;
		pblk.buffer = inputf;
		pblk.fop = F_SYNTAXO;
		status = parse_file(&v->str, &pblk);
		if (!(status & 1))
			rts_error(VARLSTCNT(5) ERR_FILEPARSE, 2, v->str.len, v->str.addr, status);

		if (pblk.fnb & F_WILD)
			rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
				ERR_WILDCARD, 2, v->str.len, v->str.addr);

		file.addr = pblk.buffer;
		file.len = pblk.b_esl;
		type = NOTYPE;
		expdir = (pblk.fnb & F_HAS_DIR) != 0;
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

		if (stringpool.free + file.len > stringpool.top)
			stp_gcol(file.len);
		memcpy(stringpool.free, file.addr, file.len);
		dollar_zsource.addr = (char *) stringpool.free;
		dollar_zsource.len = file.len;
		stringpool.free += file.len;

		if (type == OBJ)
		{
			memmove(&objnamebuf[0],file.addr, file.len + pblk.b_ext);
			objnamelen = file.len + pblk.b_ext;
			assert (objnamelen <= MAX_FBUFF + 1);
			objnamebuf[objnamelen] = 0;
		} else  if (type == SRC)
		{
			memmove(&srcnamebuf[0],file.addr,file.len + pblk.b_ext);
			srcnamelen = file.len + pblk.b_ext;
			assert (srcnamelen <= MAX_FBUFF + 1);
			srcnamebuf[srcnamelen] = 0;
			objnamelen = file.len;
			if (pblk.b_name > sizeof(mident))
				objnamelen = pblk.b_dir + sizeof(mident);
			memcpy(&objnamebuf[0], file.addr, objnamelen);
			memcpy(&objnamebuf[objnamelen], DOTOBJ, sizeof(DOTOBJ));
			objnamelen += sizeof(DOTOBJ) - 1;
			assert (objnamelen + sizeof(DOTOBJ) <= MAX_FBUFF + 1);
		} else
		{
			if (file.len + sizeof(DOTM) > sizeof(srcnamebuf) ||
				file.len + sizeof(DOTOBJ) > sizeof(objnamebuf))
				rts_error(VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);

			memmove(&srcnamebuf[0], file.addr, file.len);
			memcpy(&srcnamebuf[file.len], DOTM, sizeof(DOTM));
			srcnamelen = file.len + sizeof(DOTM) - 1;
			assert (srcnamelen + sizeof(DOTM) <= MAX_FBUFF + 1);
			memcpy(&objnamebuf[0],file.addr,file.len);
			memcpy(&objnamebuf[file.len], DOTOBJ, sizeof(DOTOBJ));
			objnamelen = file.len + sizeof(DOTOBJ) - 1;
			assert (objnamelen + sizeof(DOTOBJ) <= MAX_FBUFF + 1);
		}
		if (!expdir)
		{
			srcstr.addr = &srcnamebuf[0];
			srcstr.len = srcnamelen;
			objstr.addr = &objnamebuf[0];
			objstr.len = objnamelen;
			if (type == OBJ)
			{
				zro_search(&objstr, &objdir, 0, 0, TRUE);
				if (!objdir)
					rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, dollar_zsource.len, dollar_zsource.addr,
						ERR_FILENOTFND, 2, dollar_zsource.len, dollar_zsource.addr);
			} else  if (type == SRC)
			{
				zro_search(&objstr, &objdir, &srcstr, &srcdir, TRUE);
				if (!srcdir)
					rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, srcnamelen, &srcnamebuf[0],
						ERR_FILENOTFND, 2, srcnamelen, &srcnamebuf[0]);
			} else
			{
				zro_search(&objstr, &objdir, &srcstr, &srcdir, TRUE);
				if (!objdir && !srcdir)
					rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, dollar_zsource.len, dollar_zsource.addr,
						ERR_FILENOTFND, 2, dollar_zsource.len, dollar_zsource.addr);
			}
		}
	} else
	{ /* auto-ZLINK */
		type = NOTYPE;
		memcpy(&srcnamebuf[0],v->str.addr,v->str.len);
		memcpy(&srcnamebuf[v->str.len], DOTM, sizeof(DOTM));
		srcnamelen = v->str.len + sizeof(DOTM) - 1;
		if (srcnamebuf[0] == '%')
			srcnamebuf[0] = '_';

		memcpy(&objnamebuf[0],&srcnamebuf[0],v->str.len);
		memcpy(&objnamebuf[v->str.len], DOTOBJ, sizeof(DOTOBJ));
		objnamelen = v->str.len + sizeof(DOTOBJ) - 1;

		srcstr.addr = &srcnamebuf[0];
		srcstr.len = srcnamelen;
		objstr.addr = &objnamebuf[0];
		objstr.len = objnamelen;
		/* On shared platforms, skip parameter should be FALSE to indicate an auto-ZLINK so that
		 * zro_search looks into shared libraries. On non-shared platforms, it should be
		 * TRUE to instruct zro_search to always skip shared libraries */
		zro_search(&objstr, &objdir, &srcstr, &srcdir, NON_USHBIN_ONLY(TRUE) USHBIN_ONLY(FALSE));
		if (!objdir && !srcdir)
			rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
				ERR_FILENOTFND, 2, v->str.len, v->str.addr);
		qualifier.mvtype = MV_STR;
		qualifier.str = dollar_zcompile;
		quals = &qualifier;
	}

	if (type == OBJ)
	{
		if (objdir)
		{
			assert(ZRO_TYPE_OBJLIB != objdir->type);
			if (objdir->str.len + objnamelen + 2 > sizeof(objnamebuf))
				rts_error(VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);
			if (objdir->str.len)
			{
				tslash = (objdir->str.addr[objdir->str.len - 1] == '/') ? 0 : 1;
				memmove(&objnamebuf[ objdir->str.len + tslash], &objnamebuf[0], objnamelen);
				if (tslash)
					objnamebuf[ objdir->str.len ] = '/';
				memcpy(&objnamebuf[0], objdir->str.addr, objdir->str.len);
				objnamelen += objdir->str.len + tslash;
				objnamebuf[ objnamelen ] = 0;
			}
		}
		POLL_OBJECT_FILE(objnamebuf, obj_desc);
		if (obj_desc == -1)
			rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, dollar_zsource.len, dollar_zsource.addr, errno);
		if (USHBIN_ONLY(!incr_link(obj_desc, NULL)) NON_USHBIN_ONLY(!incr_link(obj_desc)))
			rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, dollar_zsource.len, dollar_zsource.addr, ERR_VERSION);
		status = close (obj_desc);
		if (status == -1)
			rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, dollar_zsource.len, dollar_zsource.addr, errno);
	} else	/* either NO type or SOURCE type */
	{
		struct stat obj_stat, src_stat;
		bool src_found, obj_found, compile;

		cmd_qlf.object_file.str.addr = obj_file;
		cmd_qlf.object_file.str.len = MAX_FBUFF;
		cmd_qlf.list_file.str.addr = list_file;
		cmd_qlf.list_file.str.len = MAX_FBUFF;
		cmd_qlf.ceprep_file.str.addr = ceprep_file;
		cmd_qlf.ceprep_file.str.len = MAX_FBUFF;

		if (srcdir)
		{
			assert(ZRO_TYPE_OBJLIB != objdir->type);
			if (srcdir->str.len + srcnamelen > sizeof(srcnamebuf) - 1)
				rts_error(VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);
			if (srcdir->str.len)
			{
				tslash = (srcdir->str.addr[srcdir->str.len - 1] == '/') ? 0 : 1;
				memmove(&srcnamebuf[ srcdir->str.len + tslash ], &srcnamebuf[0], srcnamelen);
				if (tslash)
					srcnamebuf[ srcdir->str.len ] = '/';
				memcpy(&srcnamebuf[0], srcdir->str.addr, srcdir->str.len);
				srcnamelen += srcdir->str.len + tslash;
				srcnamebuf[ srcnamelen ] = 0;
			}
		}
		if (objdir)
		{
			if (ZRO_TYPE_OBJLIB == objdir->type)
			{
				NON_USHBIN_ONLY(GTMASSERT;)
				assert(objdir->shrlib);
				assert(objdir->shrsym);
				USHBIN_ONLY(
					if (!incr_link(0, objdir))
						GTMASSERT;
				)
				return;
			}
			if (objdir->str.len + objnamelen > sizeof(objnamebuf) - 1)
				rts_error(VARLSTCNT(4) ERR_ZLINKFILE, 2, v->str.len, v->str.addr);
			if (objdir->str.len)
			{
				tslash = (objdir->str.addr[objdir->str.len - 1] == '/') ? 0 : 1;
				memmove(&objnamebuf[ objdir->str.len + tslash ], &objnamebuf[0], objnamelen);
				if (tslash)
					objnamebuf[ objdir->str.len ] = '/';
				memcpy(&objnamebuf[0], objdir->str.addr, objdir->str.len);
				objnamelen += objdir->str.len + tslash;
				objnamebuf[ objnamelen ] = 0;
			}
		}
		src_found = obj_found = compile = FALSE;

		if (type != SRC)
		{
			STAT_FILE(objnamebuf, &obj_stat, status);
			if (status == -1)
			{
				if (errno == ENOENT)
					obj_found = FALSE;
				else
					rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, errno);
			} else
				obj_found = TRUE;
		} else
			compile = TRUE;

		STAT_FILE(srcnamebuf, &src_stat, status);
		if (status == -1)
		{
			if (errno == ENOENT && type != SRC)
				src_found = FALSE;
			else
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE,2,srcnamelen,srcnamebuf,errno);
		} else
			src_found = TRUE;

		if (type != SRC)
		{
			if (src_found)
			{
				if (obj_found)
				{
					if (src_stat.st_mtime > obj_stat.st_mtime)
						compile = TRUE;
				} else
					compile = TRUE;
			} else  if (!obj_found)
				rts_error (VARLSTCNT(8) ERR_ZLINKFILE, 2, objnamelen, objnamebuf,
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
			if (!(cmd_qlf.qlf & CQ_OBJECT) && type != SRC)
			{
				cmd_qlf.qlf = glb_cmd_qlf.qlf;
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, ERR_ZLNOOBJECT);
			}
			zlcompile (srcnamelen, (uchar_ptr_t)srcnamebuf);
			if (type == SRC && !(qlf & CQ_OBJECT))
				return;
		}
		POLL_OBJECT_FILE(objnamebuf, obj_desc);
		if (obj_desc == -1)
			rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, errno);
		status = USHBIN_ONLY(incr_link(obj_desc, NULL)) NON_USHBIN_ONLY(incr_link(obj_desc));
		if (!status)	/* due only to version mismatch, so recompile */
		{
			status = close (obj_desc);
			if (status == -1)
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, errno);
			if (compile)
				GTMASSERT;
			if (!src_found)
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, ERR_VERSION);

			zl_cmd_qlf(&quals->str, &cmd_qlf);
			if (!MV_DEFINED(&cmd_qlf.object_file))
			{
				cmd_qlf.object_file.mvtype = MV_STR;
				cmd_qlf.object_file.str.addr = objnamebuf;
				cmd_qlf.object_file.str.len = objnamelen;
			}
			zlcompile (srcnamelen, (uchar_ptr_t)srcnamebuf);
			if (!(cmd_qlf.qlf & CQ_OBJECT) && type != SRC)
			{
				cmd_qlf.qlf = glb_cmd_qlf.qlf;
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, ERR_ZLNOOBJECT);
			}
			POLL_OBJECT_FILE(object_file_name, obj_desc);
			if (obj_desc == -1)
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, errno);
			if (USHBIN_ONLY(!incr_link(obj_desc, NULL)) NON_USHBIN_ONLY(!incr_link(obj_desc)))
				GTMASSERT;
		}
		status = close (obj_desc);
		if (status == -1)
			rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, errno);
	}
}

