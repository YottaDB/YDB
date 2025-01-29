/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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

#include "gtm_signal.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"

#include <errno.h>

#include "io.h"
#include "iott_setterm.h"
#include "parse_file.h"
#include "zroutines.h"
#include "eintr_wrappers.h"
#include "stringpool.h"
#include "op.h"
#include "fork_init.h"
#include "geteditor.h"
#include "restrict.h"
#include "iottdef.h"
#include "ydb_getenv.h"

GBLREF	io_pair		io_std_device;
GBLREF	mval 		dollar_zsource;
GBLREF	mstr		editor;
GBLREF	int4		dollar_zeditor;

error_def(ERR_NOEDITOR);
error_def(ERR_ZEDFILSPEC);
error_def(ERR_RESTRICTEDOP);

void op_zedit(mval *v, mval *p)
{
	char		*edt;
	char		es[MAX_FN_LEN + 1], typ, *ptr;
	short		path_len, tslash;
	int		objcnt, ret;
	unsigned int	status;
	bool		has_ext, exp_dir;
	parse_blk	pblk;
	mstr		src;
	zro_ent		*sp, *srcdir;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	geteditor();
	if (!editor.len)
	{
		edt = ydb_getenv(YDBENVINDX_GENERIC_EDITOR, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH);
		if (!edt)
			edt = "editor";
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_NOEDITOR, 2, LEN_AND_STR(edt));
	}
	MV_FORCE_STR(v);
	MV_FORCE_STR(p);
	src.len = v->str.len;
	src.addr = v->str.addr;
	if (0 == src.len)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZEDFILSPEC, 2, src.len, src.addr);
		return;
	}
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = es;
	pblk.buff_size = MAX_FN_LEN;
	status = parse_file(&src, &pblk);
	if (!(status & 1))
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZEDFILSPEC, 2, src.len, src.addr, status);
		return;
	}
	has_ext = 0 != (pblk.fnb & F_HAS_EXT);
	exp_dir = 0 != (pblk.fnb & F_HAS_DIR);
	if (!(pblk.fnb & F_HAS_NAME))
	{
		assert(!has_ext);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZEDFILSPEC, 2, pblk.b_esl, pblk.buffer);
		return;
	}
	if (!exp_dir)
	{
		memmove(&es[0], pblk.l_name, pblk.b_name + pblk.b_ext);
		path_len = pblk.b_name + pblk.b_ext;
		ptr = es;
	} else
	{
		path_len = pblk.b_esl;
		ptr = pblk.l_name;
	}
	typ = 0;
	if (!has_ext)
	{
		if ('.' != *ptr)
		{
			typ = STR_LIT_LEN(DOTM);
			if (path_len + typ > MAX_FN_LEN)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZEDFILSPEC, 2, path_len, es);
				return;
			}
			memcpy(&es[path_len], DOTM, STR_LIT_LEN(DOTM));
			path_len += typ;
		}
	} else
	{
		if ((STR_LIT_LEN(DOTOBJ) == pblk.b_ext) && !MEMCMP_LIT(ptr + pblk.b_name, DOTOBJ))
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZEDFILSPEC, 2, path_len, es);
			return;
		} else if ((STR_LIT_LEN(DOTM) == pblk.b_ext) && !MEMCMP_LIT(ptr + pblk.b_name, DOTM))
			typ = STR_LIT_LEN(DOTM);
	}
	dollar_zsource.str.addr = es;
	dollar_zsource.str.len = path_len - typ;
	s2pool(&dollar_zsource.str);
	es[path_len] = 0;
	if (!exp_dir)
	{
		src.addr = es;
		src.len = path_len;
		srcdir = (zro_ent *)0;
		zro_search(NULL, NULL, &src, &srcdir, TRUE);
		if (NULL == srcdir)
		{	/* find the first source directory */
			objcnt = (TREF(zro_root))->count;
			for (sp = TREF(zro_root) + 1;  (NULL == srcdir) && (0 < objcnt--); ++sp)
			{
				if (ZRO_TYPE_OBJECT == sp->type)
				{
					sp++;
					assert(ZRO_TYPE_COUNT == sp->type);
					if (0 != sp->count)
						srcdir  = sp + 1;
				} else
				{  /* shared library entries (ZRO_TYPE_OBJLIB) do not have source directories */
					assert(ZRO_TYPE_OBJLIB == sp->type);
				}
			}
		}
		if (srcdir && srcdir->str.len)
		{
			assert(ZRO_TYPE_SOURCE == srcdir->type);
			tslash = ('/' == srcdir->str.addr[srcdir->str.len - 1]) ? 0 : 1;
			if (path_len + srcdir->str.len + tslash >= SIZEOF(es))
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZEDFILSPEC, 2, src.len, src.addr);
				return;
			}
			memmove(&es[ srcdir->str.len + tslash], &es[0], path_len);
			if (tslash)
				es[ srcdir->str.len ] = '/';
			memcpy(&es[0], srcdir->str.addr, srcdir->str.len);
			path_len += srcdir->str.len + tslash;
			es[ path_len ] = 0;
		}
	}

	if (RESTRICTED(zedit_op))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "ZEDIT");

	flush_pio();
<<<<<<< HEAD
	ret = gtm_system_internal(editor.addr, es, NULL, NULL);
	if (-1 == ret)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("gtm_system_internal()"), CALLFROM, errno);
	assert(WIFEXITED(ret));	/* Since "gtm_system_internal" does a WAITPID with options parameter set to 0,
				 * we are guaranteed that a return that is not -1 implies the child has terminated.
				 */
	if (WIFEXITED(ret))
	{
		ret = WEXITSTATUS(ret);
		dollar_zeditor = ret;
=======
	if (tt == io_std_device.in->type)
		iott_resetterm(io_std_device.in);
	/* ignore interrupts */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	sigaction(SIGINT, &act, &intr);
	FORK(childid);
	if (childid)
	{
		waitid = (int)childid;
		for (;;)
		{
#ifdef _BSD
			WAIT(&wait_status, waitid);
#else
			WAIT((int *)&status, waitid);
#endif
			if (waitid == (int)childid)
				break;
			if (-1 == waitid)
				break;
		}
		if (-1 != waitid)
			dollar_zeditor = 0;
		else
			dollar_zeditor = errno;
		/* restore interrupt handler */
		sigaction(SIGINT, &intr, 0);
		if (tt == io_std_device.in->type)
			iott_setterm(io_std_device.in);
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
	} else
		dollar_zeditor = -1;	/* Not sure how child did not terminate. Record that event with a -1 here. */
}
