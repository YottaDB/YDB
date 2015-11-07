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

#include <rms.h>
#include "gtm_limits.h"

#include "cmd_qlf.h"
#include "stringpool.h"
#include "zroutines.h"
#include "gt_timer.h"
#include "incr_link.h"
#include "compiler.h"
#include "zl_olb.h"
#include "min_max.h"

#define SRC			1
#define OBJ			2
#define NOTYPE			3

#define QUADCMP(p1, p2)		(*((unsigned *)p1 + 1) > *((unsigned *)p2 + 1) || \
				*((unsigned *)p1 + 1) == *((unsigned *)p2 + 1) && *(unsigned *)p1 > *(unsigned *)p2)

GBLREF spdesc 			stringpool;
GBLREF command_qualifier	cmd_qlf, glb_cmd_qlf;
GBLREF mval			dollar_zsource;
GBLREF char			object_file_name[];
GBLREF short			object_name_len;
GBLREF struct FAB		obj_fab;

error_def (ERR_WILDCARD);
error_def (ERR_VERSION);
error_def (ERR_FILENOTFND);
error_def (ERR_ZLINKFILE);
error_def (ERR_FILEPARSE);
error_def (ERR_ZLNOOBJECT);
error_def (ERR_FILENAMETOOLONG);

void op_zlink(mval *v, mval *quals)
{
	struct FAB		srcfab;
	struct NAM		srcnam, objnam;
	struct XABDAT		srcxab, objxab;
	boolean_t		compile, expdir, libr, obj_found, src_found;
	short			flen;
	unsigned short		type;
	unsigned char		srccom[MAX_FN_LEN], srcnamebuf[MAX_FN_LEN], objnamebuf[MAX_FN_LEN], objnamelen, srcnamelen,ver[6];
	unsigned char		objcom[MAX_FN_LEN], list_file[MAX_FN_LEN], ceprep_file[MAX_FN_LEN], *fname;
	zro_ent			*srcdir, *objdir;
	mstr			srcstr, objstr, version;
	mval			qualifier;
	unsigned		status, srcfnb;
	uint4			lcnt, librindx, qlf;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	if (MAX_FN_LEN < v->str.len)
		rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, MIN(UCHAR_MAX, v->str.len), v->str.addr, ERR_FILENAMETOOLONG);
	version.len = 0;
	srcdir = objdir = 0;
	version.addr = ver;
	libr = FALSE;
	obj_fab = cc$rms_fab;
	if (quals)
	{
		MV_FORCE_STR(quals);
		srcfab = cc$rms_fab;
		srcfab.fab$l_fna = v->str.addr;
		srcfab.fab$b_fns = v->str.len;
		srcfab.fab$l_nam = &srcnam;
		srcnam = cc$rms_nam;
		srcnam.nam$l_esa = srcnamebuf;
		srcnam.nam$b_ess = SIZEOF(srcnamebuf);
		srcnam.nam$b_nop = NAM$M_SYNCHK;
		status = sys$parse(&srcfab);
		if (!(status & 1))
			rts_error(VARLSTCNT(9) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
				ERR_FILEPARSE, 2, v->str.len, v->str.addr, status);
		if (srcnam.nam$l_fnb & NAM$M_WILDCARD)
			rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
				ERR_WILDCARD, 2, v->str.len, v->str.addr);
		srcfnb = srcnam.nam$l_fnb;
		expdir = (srcfnb & (NAM$M_NODE | NAM$M_EXP_DEV | NAM$M_EXP_DIR));
		if (srcfnb & NAM$M_EXP_VER)
		{
			memcpy(version.addr, srcnam.nam$l_ver, srcnam.nam$b_ver);
			version.len = srcnam.nam$b_ver;
		}
		if (expdir)
		{
			if (version.len)
				flen = srcnam.nam$b_esl - srcnam.nam$b_type - version.len;
			else
				flen = srcnam.nam$b_esl - srcnam.nam$b_type - 1; /* semicolon is put in by default */
			fname = srcnam.nam$l_esa;
		} else
		{
			flen = srcnam.nam$b_name;
			fname = srcnam.nam$l_name;
		}
		ENSURE_STP_FREE_SPACE(flen);
		memcpy(stringpool.free, fname, flen);
		dollar_zsource.str.addr = stringpool.free;
		dollar_zsource.str.len = flen;
		stringpool.free += flen;
		if (srcfnb & NAM$M_EXP_TYPE)
		{
			if ((SIZEOF(DOTOBJ) - 1 == srcnam.nam$b_type) &&
				!MEMCMP_LIT(srcnam.nam$l_type, DOTOBJ))
			{
				type = OBJ;
				objstr.addr = srcnam.nam$l_esa;
				objstr.len = srcnam.nam$b_esl;
			} else
			{
				type = SRC;
				memcpy(srcnamebuf, dollar_zsource.str.addr, flen);
				memcpy(&srcnamebuf[flen], srcnam.nam$l_type, srcnam.nam$b_type);
				memcpy(&srcnamebuf[flen + srcnam.nam$b_type],
					version.addr, version.len);
				srcnamelen = flen + srcnam.nam$b_type + version.len;
				srcnamebuf[srcnamelen] = 0;
				srcstr.addr = srcnamebuf;
				srcstr.len = srcnamelen;
				memcpy(objnamebuf, dollar_zsource.str.addr, flen);
				memcpy(&objnamebuf[flen], DOTOBJ, SIZEOF(DOTOBJ));
				objnamelen = flen + SIZEOF(DOTOBJ) - 1;
				objstr.addr = objnamebuf;
				objstr.len = objnamelen;
			}
		} else
		{
			type = NOTYPE;
			memcpy(srcnamebuf, dollar_zsource.str.addr, flen);
			memcpy(&srcnamebuf[flen], DOTM, SIZEOF(DOTM));
			srcnamelen = flen + SIZEOF(DOTM) - 1;
			memcpy(objnamebuf, dollar_zsource.str.addr, flen);
			MEMCPY_LIT(&objnamebuf[flen], DOTOBJ);
			memcpy(&objnamebuf[flen + SIZEOF(DOTOBJ) - 1], version.addr, version.len);
			objnamelen = flen + SIZEOF(DOTOBJ) + version.len - 1;
			objnamebuf[objnamelen] = 0;
			srcstr.addr = srcnamebuf;
			srcstr.len = srcnamelen;
			objstr.addr = objnamebuf;
			objstr.len = objnamelen;
		}
		if (!expdir)
		{
			if (OBJ == type)
			{
				zro_search(&objstr, &objdir, 0, 0);
				if (!objdir)
					rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, dollar_zsource.str.len, dollar_zsource.str.addr,
						ERR_FILENOTFND, 2, dollar_zsource.str.len, dollar_zsource.str.addr);
			} else  if (SRC == type)
			{
				zro_search(&objstr, &objdir, &srcstr, &srcdir);
				if (!srcdir)
					rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf,
						ERR_FILENOTFND, 2, srcnamelen, srcnamebuf);
			} else
			{
				zro_search(&objstr, &objdir, &srcstr, &srcdir);
				if (!objdir && !srcdir)
					rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, dollar_zsource.str.len, dollar_zsource.str.addr,
						ERR_FILENOTFND, 2, dollar_zsource.str.len, dollar_zsource.str.addr);
			}
		}
	} else
	{
		expdir = FALSE;
		type = NOTYPE;
		flen = v->str.len;
		memcpy(srcnamebuf, v->str.addr, flen);
		MEMCPY_LIT(&srcnamebuf[flen], DOTM);
		srcnamelen = flen + SIZEOF(DOTM) - 1;
		if ('%' == srcnamebuf[0])
			srcnamebuf[0] = '_';
		memcpy(objnamebuf, srcnamebuf, flen);
		MEMCPY_LIT(&objnamebuf[flen], DOTOBJ);
		objnamelen = flen + SIZEOF(DOTOBJ) - 1;
		srcstr.addr = srcnamebuf;
		srcstr.len = srcnamelen;
		objstr.addr = objnamebuf;
		objstr.len = objnamelen;
		zro_search(&objstr, &objdir, &srcstr, &srcdir);
		if (!objdir && !srcdir)
			rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, v->str.len, v->str.addr,
				ERR_FILENOTFND, 2, v->str.len, v->str.addr);
		qualifier.mvtype = MV_STR;
		qualifier.str = TREF(dollar_zcompile);
		quals = &qualifier;
	}
	if (OBJ == type)
	{
		obj_fab.fab$b_fac = FAB$M_GET;
		obj_fab.fab$b_shr = FAB$M_SHRGET;
		if (NULL != objdir)
		{
			if (ZRO_TYPE_OBJLIB == objdir->type)
				libr = TRUE;
			else
			{
				srcfab.fab$l_dna = objdir->str.addr;
				srcfab.fab$b_dns = objdir->str.len;
			}
		}
		for (lcnt = 0;  lcnt < MAX_FILE_OPEN_TRIES;  lcnt++)
		{
			status = (FALSE == libr) ? sys$open(&srcfab): zl_olb(&objdir->str, &objstr, &librindx);
			if (RMS$_FLK != status)
				break;
			hiber_start(WAIT_FOR_FILE_TIME);
		}
		if (FALSE == (status & 1))
			rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, dollar_zsource.str.len, dollar_zsource.str.addr, status);
		if (FALSE == ((FALSE == libr) ? incr_link(&srcfab, libr) : incr_link(&librindx, libr)))
			rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, dollar_zsource.str.len, dollar_zsource.str.addr, ERR_VERSION);
		status = (FALSE == libr) ? sys$close(&srcfab) : lbr$close(&librindx);
		if (FALSE == (status & 1))
			rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, dollar_zsource.str.len, dollar_zsource.str.addr, status);
	} else	/* either NO type or SOURCE type */
	{
		src_found = obj_found = compile = FALSE;
		srcfab = obj_fab = cc$rms_fab;
		obj_fab.fab$l_xab = &objxab;
		srcxab = objxab = cc$rms_xabdat;
		obj_fab.fab$l_nam = &objnam;
		srcnam = objnam = cc$rms_nam;
		obj_fab.fab$l_fna = objnamebuf;
		obj_fab.fab$b_fns = objnamelen;
		obj_fab.fab$b_fac = FAB$M_GET;
		obj_fab.fab$b_shr = FAB$M_SHRGET;
		objnam.nam$l_esa = objcom;
		objnam.nam$b_ess = SIZEOF(objcom);
		srcfab.fab$l_nam = &srcnam;
		srcfab.fab$l_xab = &srcxab;
		srcfab.fab$l_fna = srcnamebuf;
		srcfab.fab$b_fns = srcnamelen;
		srcfab.fab$b_fac = FAB$M_GET;
		srcfab.fab$b_shr = FAB$M_SHRGET;
		srcnam.nam$l_esa = srccom;
		srcnam.nam$b_ess = SIZEOF(srccom);
		cmd_qlf.object_file.str.addr = objcom;
		cmd_qlf.object_file.str.len = 255;
		cmd_qlf.list_file.str.addr = list_file;
		cmd_qlf.list_file.str.len = 255;
		cmd_qlf.ceprep_file.str.addr = ceprep_file;
		cmd_qlf.ceprep_file.str.len = 255;
		if (srcdir && srcdir->str.len)
		{
			srcfab.fab$l_dna = srcdir->str.addr;
			srcfab.fab$b_dns = srcdir->str.len;
		}
		if (objdir && objdir->str.len)
		{
			if (ZRO_TYPE_OBJLIB == objdir->type)
				libr = TRUE;
			else
			{
				obj_fab.fab$l_dna = objdir->str.addr;
				obj_fab.fab$b_dns = objdir->str.len;
			}
		}
		if (SRC != type)
		{
			if (!expdir && !objdir)
				obj_found = FALSE;
			else  if (!libr)
			{
				for (lcnt = 0;  lcnt < MAX_FILE_OPEN_TRIES;  lcnt++)
				{
					status = sys$open(&obj_fab);
					if (RMS$_FLK != status)
						break;
					hiber_start(WAIT_FOR_FILE_TIME);
				}
				if (!(status & 1))
				{
					if (RMS$_FNF == status)
						obj_found = FALSE;
					else
						rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, status);
				} else
					obj_found = TRUE;
			} else
			{
				status = zl_olb(&objdir->str, &objstr, &librindx);
				if (status)
					obj_found = TRUE;
			}
		} else
			compile = TRUE;
		if (!expdir && !srcdir)
			src_found = FALSE;
		else
		{
			for (lcnt = 0;  lcnt < MAX_FILE_OPEN_TRIES;  lcnt++)
			{
				status = sys$open(&srcfab);
				if (RMS$_FLK != status)
					break;
				hiber_start(WAIT_FOR_FILE_TIME);
			}
			if (!(status & 1))
			{
				if ((RMS$_FNF == status) && (SRC != type))
					src_found = FALSE;
				else
					rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, status);
			} else
			{
				src_found = TRUE;
				if (SRC == type)
				{
					status = sys$close(&srcfab);
					if (!(status & 1))
						rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, status);
				}
			}
		}
		if (SRC != type)
		{
			if (src_found)
			{
				if (obj_found)
				{
					if (QUADCMP(&srcxab.xab$q_rdt, &objxab.xab$q_rdt))
					{
						status = sys$close(&obj_fab);
						obj_fab = cc$rms_fab;
						if (!(status & 1))
							rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, objnamelen, objnamebuf, status);
						compile = TRUE;
					}
				} else
					compile = TRUE;
				status = sys$close(&srcfab);
				if (!(status & 1))
					rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, status);
			} else  if (!obj_found)
				rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, objnamelen, objnamebuf,
					ERR_FILENOTFND, 2, objnamelen, objnamebuf);
		}
		if (compile)
		{
			zl_cmd_qlf(&quals->str, &cmd_qlf);
			if (!MV_DEFINED(&cmd_qlf.object_file))
			{
				objnam.nam$b_nop = NAM$M_SYNCHK;
				status = sys$parse(&obj_fab);
				if (!(status & 1))
					rts_error(VARLSTCNT(4) ERR_FILEPARSE, 2, obj_fab.fab$b_fns, obj_fab.fab$l_fna);
				cmd_qlf.object_file.mvtype = MV_STR;
				cmd_qlf.object_file.str.len = objnam.nam$b_esl - objnam.nam$b_ver;
			}
			qlf = cmd_qlf.qlf;
			if (!(cmd_qlf.qlf & CQ_OBJECT) && (SRC != type))
			{
				cmd_qlf.qlf = glb_cmd_qlf.qlf;
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, ERR_ZLNOOBJECT);
			}
			zlcompile(srcnam.nam$b_esl, srcnam.nam$l_esa);
			if ((SRC == type) && !(qlf & CQ_OBJECT))
				return;
		}
		status = libr ? incr_link(&librindx, libr) : incr_link(&obj_fab, libr);
		if (!status)	/* due only to version mismatch, so recompile */
		{
			if (!libr)
			{
				status = sys$close(&obj_fab);
				obj_fab = cc$rms_fab;
			} else
				status = lbr$close(&librindx);
			if (!(status & 1))
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, objstr.len, objstr.addr, status);
			if (compile)
				GTMASSERT;
			if (!src_found)
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, ERR_VERSION);
			zl_cmd_qlf(&quals->str, &cmd_qlf);
			if (!MV_DEFINED(&cmd_qlf.object_file))
			{
				objnam.nam$b_nop = NAM$M_SYNCHK;
				status = sys$parse(&obj_fab);
				if (!(status & 1))
					rts_error(VARLSTCNT(4) ERR_FILEPARSE, 2, obj_fab.fab$b_fns, obj_fab.fab$l_fna);
				cmd_qlf.object_file.mvtype = MV_STR;
				cmd_qlf.object_file.str.len = objnam.nam$b_esl - objnam.nam$b_ver;
			}
			if (!(cmd_qlf.qlf & CQ_OBJECT) && (SRC != type))
			{
				cmd_qlf.qlf = glb_cmd_qlf.qlf;
				rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, srcnamelen, srcnamebuf, ERR_ZLNOOBJECT);
			}
			zlcompile(srcnam.nam$b_esl, srcnam.nam$l_esa);
			if (!incr_link(&obj_fab, libr))
				GTMASSERT;
		}
		if (!libr)
		{
			status = sys$close(&obj_fab);
			obj_fab = cc$rms_fab;
		} else
			status = lbr$close(&librindx);
		if (!(status & 1))
			rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, objstr.len, objstr.addr, status);
	}
	return;
}
