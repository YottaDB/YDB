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

#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "gtm_stdlib.h"
#include "gtm_unistd.h"

#include "io.h"
#include "parse_file.h"
#include "zroutines.h"
#include "eintr_wrappers.h"
#include "stringpool.h"
#include "setterm.h"
#include "op.h"

#define	DOTM		".m"
#define	DOTOBJ		".o"

GBLREF	io_pair		io_std_device;
GBLREF	mstr 		dollar_zsource;
GBLREF	mstr		editor;
GBLREF	int4		dollar_zeditor;
GBLREF	zro_ent		*zro_root;

void op_zedit(mval *v, mval *p)
{
	char		*edt;
	char		es[MAX_FBUFF + 1], typ, *ptr;
	short		path_len, tslash;
	int		objcnt;
	int		waitid;
	unsigned int	childid, status;
#ifdef _BSD
	union wait	wait_status;
#endif
	bool		has_ext, exp_dir;
	parse_blk	pblk;
	mstr		src;
	zro_ent		*sp, *srcdir;
	struct		sigaction act, intr;

	error_def	(ERR_ZEDFILSPEC);
	error_def	(ERR_FILENOTFND);

	if (!editor.len)
	{
		edt = GETENV("EDITOR");
		if (!edt)
			edt = "editor";
		rts_error(VARLSTCNT(4) ERR_FILENOTFND, 2, LEN_AND_STR(edt));
	}
	MV_FORCE_STR(v);
	MV_FORCE_STR(p);
	src.len = v->str.len;
	src.addr = v->str.addr;
	if (0 == src.len)
		rts_error(VARLSTCNT(4) ERR_ZEDFILSPEC, 2, src.len, src.addr);
	memset(&pblk, 0, sizeof(pblk));
	pblk.buffer = es;
	pblk.buff_size = MAX_FBUFF;
	status = parse_file(&src, &pblk);
	if (!(status & 1))
		rts_error(VARLSTCNT(5) ERR_ZEDFILSPEC, 2, src.len, src.addr, status);
	has_ext = 0 != (pblk.fnb & F_HAS_EXT);
	exp_dir = 0 != (pblk.fnb & F_HAS_DIR);
	if (!(pblk.fnb & F_HAS_NAME))
	{
		assert(!has_ext);
		rts_error(VARLSTCNT(4) ERR_ZEDFILSPEC, 2, pblk.b_esl, pblk.buffer);
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
			typ = sizeof(DOTM) - 1;
			if (path_len + typ > MAX_FBUFF)
				rts_error(VARLSTCNT(4) ERR_ZEDFILSPEC, 2, path_len, es);
			memcpy(&es[path_len], DOTM, sizeof(DOTM) - 1);
			path_len += typ;
		}
	} else
	{
		if ((sizeof(DOTOBJ) - 1 == pblk.b_ext) && !memcmp (ptr + pblk.b_name, DOTOBJ, sizeof(DOTOBJ) - 1))
			rts_error(VARLSTCNT(4) ERR_ZEDFILSPEC, 2, path_len, es);
		else if ((sizeof(DOTM) - 1 == pblk.b_ext) && !memcmp (ptr + pblk.b_name, DOTM, sizeof(DOTM) - 1))
			typ = sizeof(DOTM) - 1;
	}
	dollar_zsource.addr = es;
	dollar_zsource.len = path_len - typ;
	s2pool(&dollar_zsource);
	es[path_len] = 0;
	if (!exp_dir)
	{
		src.addr = es;
		src.len = path_len;
		srcdir = (zro_ent *)0;
		zro_search(0, 0, &src, &srcdir);
		if (NULL == srcdir)
		{	/* find the first source directory */
			objcnt = zro_root->count;
			for (sp = zro_root + 1;  (NULL == srcdir) && (0 < objcnt--);)
			{
				sp++;
				if (0 != sp++->count)
					srcdir  = sp;
			}
		}
		if (srcdir && srcdir->str.len)
		{
			tslash = ('/' == srcdir->str.addr[srcdir->str.len - 1]) ? 0 : 1;
			if (path_len + srcdir->str.len + tslash >= sizeof(es))
				rts_error(VARLSTCNT(4) ERR_ZEDFILSPEC, 2, src.len, src.addr);
			memmove(&es[ srcdir->str.len + tslash], &es[0], path_len);
			if (tslash)
				es[ srcdir->str.len ] = '/';
			memcpy(&es[0], srcdir->str.addr, srcdir->str.len);
			path_len += srcdir->str.len + tslash;
			es[ path_len ] = 0;
		}
	}

	flush_pio();
	if (tt == io_std_device.in->type)
		resetterm(io_std_device.in);
	/* ignore interrupts */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	sigaction(SIGINT, &act, &intr);
	childid = fork();
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
			setterm(io_std_device.in);
	} else
	{
		EXECL(editor.addr, editor.addr, es, 0);
		exit(-1);
	}
}
