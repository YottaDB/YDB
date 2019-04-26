/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
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
#include "min_max.h"

#define INIT_QUALIF_STR(QUALIF, CQCODE, FIELD)										\
{															\
	if ((glb_cmd_qlf.qlf & CQCODE) && (MV_STR == glb_cmd_qlf.FIELD.mvtype) && (0 < glb_cmd_qlf.FIELD.str.len))	\
	{														\
		QUALIF->FIELD.mvtype = MV_STR;										\
		QUALIF->FIELD.str.len = glb_cmd_qlf.FIELD.str.len;							\
		memcpy(QUALIF->FIELD.str.addr, glb_cmd_qlf.FIELD.str.addr, glb_cmd_qlf.FIELD.str.len);			\
	}														\
}

GBLDEF list_params 		lst_param;

GBLREF command_qualifier 	glb_cmd_qlf;
GBLREF boolean_t		gtm_utf8_mode;

void get_cmd_qlf(command_qualifier *qualif)
{
	static readonly char upper[] = "UPPER";
	static readonly char lower[] = "LOWER";
	mstr		*s;
	int4		temp_int;
	unsigned short	len;
	unsigned char	inbuf[255];

	qualif->qlf = glb_cmd_qlf.qlf;
	qualif->object_file.mvtype = qualif->list_file.mvtype = qualif->ceprep_file.mvtype = 0;
	INIT_QUALIF_STR(qualif, CQ_OBJECT, object_file);
	INIT_QUALIF_STR(qualif, CQ_LIST, list_file);
	INIT_QUALIF_STR(qualif, CQ_CE_PREPROCESS, ceprep_file);
	if (gtm_utf8_mode)
		qualif->qlf |= CQ_UTF8;		/* Mark as being compiled in UTF8 mode */
	if (CLI_PRESENT == cli_present("OBJECT"))
	{
		qualif->qlf |= CQ_OBJECT;
		qualif->object_file.mvtype = MV_STR;
		s = &qualif->object_file.str;	/* 4SCA: object_file is allocated MAX_FN_LEN bytes */
		len = s->len;
		if (FALSE == cli_get_str("OBJECT", s->addr, &len))
		{
			s->len = 0;
			if (glb_cmd_qlf.object_file.mvtype == MV_STR  &&  glb_cmd_qlf.object_file.str.len > 0)
			{
				s->len = MIN(glb_cmd_qlf.object_file.str.len, MAX_FN_LEN);
				memcpy(s->addr, glb_cmd_qlf.object_file.str.addr, s->len);
			}
		} else
			s->len = len;
	} else if (cli_negated("OBJECT"))
		qualif->qlf &= ~CQ_OBJECT;
	if (CLI_PRESENT == cli_present("CROSS_REFERENCE"))	/* CROSS_REFERENCE is undocumented and apparently not useful */
		qualif->qlf |= CQ_CROSS_REFERENCE;
	else if (cli_negated("CROSS_REFERENCE"))
		qualif->qlf &= ~CQ_CROSS_REFERENCE;

	if (TRUE == cli_present("IGNORE"))
		qualif->qlf |= CQ_IGNORE;
	else if (cli_negated("IGNORE"))
		qualif->qlf &= ~CQ_IGNORE;
	if (CLI_PRESENT == cli_present("DEBUG"))		/* the only other appearance of CQ_DEBUG is in cmd_qlf.h */
		qualif->qlf |= CQ_DEBUG;
	else if (cli_negated("DEBUG"))
		qualif->qlf &= ~CQ_DEBUG;

	if (cli_negated("LINE_ENTRY"))			/* NOLINE_ENTRY appears implies colon syntax on all labels */
		qualif->qlf &= ~CQ_LINE_ENTRY;

	if (cli_negated("INLINE_LITERALS"))
		qualif->qlf &= ~CQ_INLINE_LITERALS;

	if (cli_negated("ALIGN_STRINGS"))		/* ALIGN_STRINGS is undocument and unimplemented */
		qualif->qlf &= ~CQ_ALIGN_STRINGS;

	#ifdef DEBUG
	if (CLI_PRESENT == cli_present("MACHINE_CODE"))
		qualif->qlf |= CQ_MACHINE_CODE;
	else if (cli_negated("MACHINE_CODE"))
		qualif->qlf &= ~CQ_MACHINE_CODE;
	#else
	qualif->qlf &= ~CQ_MACHINE_CODE;
	#endif

	if (cli_negated("WARNINGS"))
		qualif->qlf &= ~CQ_WARNINGS;
	else if (cli_present("WARNINGS"))
	{
		assert(CLI_PRESENT == cli_present("WARNINGS"));
		qualif->qlf |= CQ_WARNINGS;
	}

	if (cli_negated("LIST"))
		qualif->qlf &= (~CQ_LIST & ~CQ_MACHINE_CODE);
	else if (CLI_PRESENT == cli_present("LIST"))
	{
		qualif->qlf |= CQ_LIST;
		qualif->list_file.mvtype = MV_STR;
		s = &qualif->list_file.str;	/* 4SCA: list_file is allocated MAX_FN_LEN bytes */
		len = s->len;
		if (FALSE == cli_get_str("LIST", s->addr, &len))
		{
			s->len = 0;
			if (glb_cmd_qlf.list_file.mvtype == MV_STR  &&  glb_cmd_qlf.list_file.str.len > 0)
			{
				s->len = MIN(glb_cmd_qlf.list_file.str.len, MAX_FN_LEN);
				memcpy(s->addr, glb_cmd_qlf.list_file.str.addr, s->len);
			}
		} else
			s->len = len;
	} else if (!(qualif->qlf & CQ_LIST))
		qualif->qlf &= ~CQ_MACHINE_CODE;
	if (FALSE == cli_get_int("LENGTH",&temp_int))
		temp_int = 66;
	lst_param.lines_per_page = temp_int;
	if ((FALSE == cli_get_int("SPACE",&temp_int)) || (0 >= temp_int) || (temp_int >= lst_param.lines_per_page))
		temp_int = 1;
	lst_param.space = temp_int;
	if (CLI_PRESENT == cli_present("LABELS"))
	{
		len = SIZEOF(inbuf);
		if (cli_get_str("LABELS", (char *)&inbuf[0], &len))
		{
			if (len == SIZEOF(upper) - 1)
			{
				if (!memcmp(upper, &inbuf[0], len))
					qualif->qlf &= ~CQ_LOWER_LABELS;
				else if (!memcmp(lower, &inbuf[0], len))
					qualif->qlf |= CQ_LOWER_LABELS;
			}
		}
	}
	if (CLI_PRESENT == cli_present("NAMEOFRTN"))
		qualif->qlf |= CQ_NAMEOFRTN;
	if (CLI_PRESENT == cli_present("CE_PREPROCESS"))
        {
		qualif->qlf |= CQ_CE_PREPROCESS;
		qualif->ceprep_file.mvtype = MV_STR;
		s = &qualif->ceprep_file.str;	/* 4SCA: ceprep_file is allocated MAX_FN_LEN bytes */
		len = s->len;
		if (FALSE == cli_get_str("CE_PREPROCESS", s->addr, &len))
		{
			s->len = 0;
			if (glb_cmd_qlf.ceprep_file.mvtype == MV_STR  &&  glb_cmd_qlf.ceprep_file.str.len > 0)
			{
				s->len = MIN(glb_cmd_qlf.ceprep_file.str.len, MAX_FN_LEN);
				memcpy(s->addr, glb_cmd_qlf.ceprep_file.str.addr, s->len);
			}
		} else
			s->len = len;
	} else if (cli_negated("CE_PREPROCESS"))
		qualif->qlf &= ~CQ_CE_PREPROCESS;
#	ifdef USHBIN_SUPPORTED
	if (CLI_PRESENT == cli_present("DYNAMIC_LITERALS"))
		qualif->qlf |= CQ_DYNAMIC_LITERALS;
#	endif
	if (CLI_PRESENT == cli_present("EMBED_SOURCE"))
		qualif->qlf |= CQ_EMBED_SOURCE;
	else if (cli_negated("EMBED_SOURCE"))
		qualif->qlf &= ~CQ_EMBED_SOURCE;
}
