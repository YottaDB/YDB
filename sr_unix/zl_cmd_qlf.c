/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cmd_qlf.h"
#include "cli.h"
#include "cli_parse.h"
#include "parse_file.h"
#include "min_max.h"
#include "toktyp.h"		/* Needed for "valid_mname.h" */
#include "valid_mname.h"
#include "gtmmsg.h"

#define SET_OBJ(NAME, LEN)					\
MBSTART {							\
	memcpy(cmd_qlf.object_file.str.addr, NAME, LEN);	\
	cmd_qlf.object_file.str.addr[LEN] = 0;			\
	cmd_qlf.object_file.str.len = LEN;			\
	cmd_qlf.object_file.mvtype = MV_STR;			\
} MBEND

#define	COMMAND			"MUMPS "

GBLREF	char			cli_err_str[];
GBLREF	CLI_ENTRY		*cmd_ary, mumps_cmd_ary[];
GBLREF	command_qualifier	cmd_qlf, glb_cmd_qlf;
GBLREF	mident			routine_name, module_name, int_module_name;
GBLREF	unsigned char		object_file_name[], source_file_name[];
GBLREF	unsigned short		object_name_len, source_name_len;

STATICDEF uint4		save_qlf;

error_def(ERR_COMPILEQUALS);
error_def(ERR_FILEPARSE);
error_def(ERR_NOTMNAME);
error_def(ERR_WILDCARD);
error_def(ERR_ZLINKFILE);
error_def(ERR_ZLNOOBJECT);

void zl_cmd_qlf(mstr *quals, command_qualifier *qualif, char *srcstr, unsigned short *srclen, boolean_t last)
{
	char		cbuf[MAX_LINE], inputf[MAX_FN_LEN + 1];
	CLI_ENTRY	*save_cmd_ary;
	int		ci, parse_ret, status;
	mident		file;
	mstr		fstr;
	parse_blk	pblk;
	short		object_name_mvtype;
	unsigned short	clen = 0;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (quals->len + SIZEOF(COMMAND) > MAX_LINE)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_COMPILEQUALS, 2, quals->len, quals->addr);
	MEMCPY_LIT(cbuf, COMMAND);
	memcpy(cbuf + SIZEOF(COMMAND) - 1, quals->addr, quals->len);
	cbuf[SIZEOF(COMMAND) - 1 + quals->len] = 0;
	/* The caller of this function could have their own command parsing tables. Nevertheless, we need to parse the string as if
	 * it was a MUMPS compilation command. So we switch temporarily to the MUMPS parsing table "mumps_cmd_ary". Note that the
	 * only rts_errors possible between save and restore of the cmd_ary are in compile_source_file and those are internally
	 * handled by source_ch which will transfer control back to us (right after the the call to compile_source_file below)
	 * and hence proper restoring of cmd_ary is guaranteed even in case of errors.
	 */
	save_cmd_ary = cmd_ary;
	cmd_ary = &mumps_cmd_ary[0];
	cli_str_setup((SIZEOF(COMMAND) + quals->len), cbuf);
	parse_ret = parse_cmd();
	if (parse_ret)
	{
		if (last & save_qlf)
			glb_cmd_qlf.qlf = save_qlf;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) parse_ret, 2, LEN_AND_STR(cli_err_str));
	}
	object_name_mvtype = qualif->object_file.mvtype;
	get_cmd_qlf(qualif);
	qualif->object_file.mvtype |= object_name_mvtype;
	cmd_ary = save_cmd_ary;							/* restore cmd_ary */
	if (last && (!module_name.len || !*srclen))
	{	/* if we're calling this twice the first time would be without a real "INFILE" */
		if (!*srclen)
		{
			*srclen = MAX_FN_LEN;
			status = cli_get_str("INFILE", srcstr, srclen);
			assert(status);
			if (!*srclen)
				return;	/* No M program to process. Return. */
		}
		assert(*srclen);
		memset(&pblk, 0, SIZEOF(pblk));
		pblk.buff_size = MAX_FN_LEN;
		pblk.buffer = inputf;
		pblk.fop = F_SYNTAXO;
		fstr.addr = srcstr;
		fstr.len = *srclen;
		status = parse_file(&fstr, &pblk);
		if (!(status & 1))
		{
			if (save_qlf)
				glb_cmd_qlf.qlf = save_qlf;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, *srclen, srcstr, status);
		}
		assert(pblk.b_name);
		file.addr = pblk.l_name;
		if ((pblk.b_ext != (SIZEOF(DOTM) - 1)) || memcmp(&pblk.l_name[pblk.b_name], DOTM, SIZEOF(DOTM) - 1))
		{	/* Move any non-".m" extension over to be part of the file name */
			pblk.b_name += pblk.b_ext;
			pblk.b_ext = 0;
			if (MAX_FN_LEN >= (*srclen + SIZEOF(DOTM)))
			{
				memcpy(&pblk.l_name[pblk.b_name], DOTM, SIZEOF(DOTM));
				pblk.b_ext = (SIZEOF(DOTM) - 1);
			}
		}
		file.len = pblk.b_name;
		source_name_len = pblk.b_dir + pblk.b_name + pblk.b_ext;
		source_name_len = MIN(source_name_len, MAX_FN_LEN);
		memcpy(source_file_name, pblk.buffer, source_name_len);
		source_file_name[source_name_len] = 0;
		memcpy(srcstr, source_file_name, source_name_len);
		srcstr[source_name_len] = 0;
		*srclen = source_name_len;
		if (!module_name.len && !(CQ_NAMEOFRTN & cmd_qlf.qlf) && !(CLI_PRESENT == cli_present("OBJECT")))
		{
			memcpy(routine_name.addr, file.addr, file.len);
			clen = routine_name.len = MIN(file.len, MAX_MIDENT_LEN);
			object_name_len = clen;
			memcpy(object_file_name, pblk.l_name, object_name_len);
			SET_OBJ(object_file_name, object_name_len);
		}

	}
	assert(!last || *srclen);
	/* routine_name is the internal name of the routine (with any leading '%' translated to '_') which by default is the
	 * unpathed name of the source file. An -OBJECT qualif (without a NAMEOFOFRTN) makes routine_name from the name of the
	 * object (which trigger compilation uses). A NAMEOFOFRTN qualifier (which trigger compilation uses) overrides the others.
	 * int_module_name is the external symbol (without the % translation) that gets exposed (in the GTM context)
	 */
	if (CLI_PRESENT == cli_present("OBJECT"))
	{
		assert((cmd_qlf.object_file.str.len) && (MAX_FN_LEN >= cmd_qlf.object_file.str.len)
			&& (cmd_qlf.object_file.str.addr));
		object_name_len = MAX_FN_LEN;
		cli_get_str("OBJECT", (char *)object_file_name, &object_name_len);
		cmd_qlf.object_file.mvtype = MV_STR;
		if (!(CQ_NAMEOFRTN & cmd_qlf.qlf))
		{
			for (ci = object_name_len - 1; ci && ('/' != object_file_name[ci]); ci--)
				;       /* scan back from end for rtn name & triggerness */
			ci += ci ? 1 : 0;
			clen = object_name_len - ci;
			if (2 <= clen)
			{
				if ('o' != object_file_name[ci + --clen])
					clen++;
				else if ('.' != object_file_name[ci + --clen])
					clen++;
			}
			if (2 <= clen)
			{
				if ('m' != object_file_name[ci + --clen])
					clen++;
				else if ('.' != object_file_name[ci + --clen])
					clen++;
			}
			clen = MIN(clen, MAX_MIDENT_LEN);
			memcpy(routine_name.addr, &object_file_name[ci], clen);
			SET_OBJ(object_file_name, object_name_len);
		}
	}
	assert(!last || *srclen);
	if (CQ_NAMEOFRTN & cmd_qlf.qlf)
	{       /* Routine name specified - name wins over object */
		clen = MAX_MIDENT_LEN;
		cli_get_str("NAMEOFRTN", routine_name.addr, &clen);
		assert(MAX_MIDENT_LEN >= clen);
		cmd_qlf.qlf &= ~CQ_NAMEOFRTN;   /* Can only be used for first module in list */
		if (CLI_PRESENT != cli_present("OBJECT"))
		{
			memcpy(object_file_name, routine_name.addr, clen);
			SET_OBJ(object_file_name, clen);
		}
	}
	assert(!last || *srclen);
	if (clen)
	{
		memcpy(module_name.addr, routine_name.addr, clen);
		if ('_' == *routine_name.addr)
			routine_name.addr[0] = '%';
		routine_name.len = clen;
		if (last && !(pblk.fnb & F_WILD) && !TREF(trigger_compile_and_link) && !valid_mname(&routine_name))
		{
			if (save_qlf)
				glb_cmd_qlf.qlf = save_qlf;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_NOTMNAME, 2, RTS_ERROR_MSTR(&routine_name), ERR_ZLNOOBJECT);
		}
		module_name.len = int_module_name.len = clen;
		memcpy(int_module_name.addr, routine_name.addr, clen);
	} else
		assert(!last || int_module_name.len);
	assert(!last || *srclen);
	if (!last)
	{
		save_qlf = glb_cmd_qlf.qlf;
		glb_cmd_qlf.qlf = cmd_qlf.qlf;
	} else if (save_qlf)
		glb_cmd_qlf.qlf = save_qlf;
	return;
}
