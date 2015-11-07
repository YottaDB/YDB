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
#include "gtm_limits.h"

#include <descrip.h>
#include <rms.h>
#include <ssdef.h>

#include "io.h"
#include "zroutines.h"
#include "stringpool.h"
#include "op.h"
#include "setterm.h"
#include "get_tpu_addr.h"
#include "min_max.h"

GBLREF io_pair		io_std_device;
GBLREF mval		dollar_zsource;
GBLREF int4		dollar_zeditor;

error_def	(ERR_ZEDFILSPEC);
error_def	(ERR_FILENAMETOOLONG);

void op_zedit(mval *v, mval *p)
{
	char		combuf[259] = {'T', 'P', 'U', ' '};
	uint4		status;
	static uint4	(*tpu_entry)() = 0;
	int		comlen, objcnt, typ, ver;
	struct FAB	fab;
	struct NAM	nam;
	unsigned char	es[MAX_FN_LEN];
	zro_ent		*sp, *srcdir;
	mstr		src;
	$DESCRIPTOR	(com,combuf);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	MV_FORCE_STR(p);
	if (MAX_FN_LEN < v->str.len)
		rts_error(VARLSTCNT(5) ERR_ZEDFILSPEC, 2, MIN(SHRT_MAX, v->str.len), v->str.addr,
			ERR_FILENAMETOOLONG);
	fab = cc$rms_fab;
	fab.fab$l_fna = v->str.addr;
	fab.fab$b_fns = v->str.len;
	fab.fab$l_dna = DOTM;
	fab.fab$b_dns = SIZEOF(DOTM) - 1;
	fab.fab$l_nam = &nam;
	nam = cc$rms_nam;
	nam.nam$l_esa = es;
	nam.nam$b_ess = SIZEOF(es);
	status = sys$parse (&fab);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	if ((SIZEOF(DOTOBJ) - 1 == nam.nam$b_type) && !memcmp (nam.nam$l_type, LIT_AND_LEN(DOTOBJ)))
		rts_error(VARLSTCNT(4) ERR_ZEDFILSPEC, 2, v->str.len, v->str.addr);
	ver = nam.nam$b_ver;
	if (!memcmp(nam.nam$l_type, LIT_AND_LEN(DOTM)))
		typ = 0;
	else
		typ = nam.nam$b_type;
	if (!(nam.nam$l_fnb & (NAM$M_NODE | NAM$M_EXP_DEV | NAM$M_EXP_DIR)))
	{
		src.addr = nam.nam$l_name;
		src.len = nam.nam$b_name + nam.nam$b_type + nam.nam$b_ver;
		zro_search (0, 0, &src, &srcdir);
		if (NULL == srcdir)
		{	/* find the first source directory */
			objcnt = (TREF(zro_root))->count;
			for (sp = TREF(zro_root) + 1;  (NULL == srcdir) && (0 < objcnt--);)
			{
				sp++;
				if (0 != sp++->count)
					srcdir = sp;
			}
		}
		if (NULL != srcdir)
		{
			fab.fab$l_dna = srcdir->str.addr;
			fab.fab$b_dns = srcdir->str.len;
			nam.nam$b_nop = NAM$M_SYNCHK;
			fab.fab$l_fna = src.addr;
			fab.fab$b_fns = src.len;
			status = sys$parse (&fab);
			if (!(status & 1))
				rts_error(VARLSTCNT(1) status);
		}
		dollar_zsource.str.addr = nam.nam$l_name;
		dollar_zsource.str.len = nam.nam$b_name + typ;
	} else
	{
		dollar_zsource.str.addr = es;
		dollar_zsource.str.len = nam.nam$b_esl - nam.nam$b_type + typ - ver;
	}
	s2pool(&dollar_zsource.str);
	comlen = 4;
	memcpy (&combuf[comlen], es, nam.nam$b_esl);
	comlen += nam.nam$b_esl;
	if (0 != p->str.len && (comlen + p->str.len <= SIZEOF(combuf)))
	{
		memcpy (&combuf[comlen], p->str.addr, p->str.len);
		comlen += p->str.len;
	}
	com.dsc$w_length = comlen;

	if (tt == io_std_device.in->type)
		resetterm(io_std_device.in);
	if (0 == tpu_entry)
		/* get_tpu_addr really should return a pointer to a uint4 function */
		tpu_entry = get_tpu_addr();
	status = (*tpu_entry)(&com);
	if (tt == io_std_device.in->type)
		setterm(io_std_device.in);
	dollar_zeditor = status;
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
}
