/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gtmdbglvl.h"
#include "compiler.h"
#include "opcode.h"
#include "mvalconv.h"
#include "cdbg_dump.h"

GBLREF char *oc_tab_graphic[];

LITDEF char *oprtype_names[] =
{
	"Operand[0]",
	"Operand[1]",
	"Destination"
};
LITDEF char *oprtype_type_names[] =
{
	"NIL",
	"TVAR_REF",
	"TVAL_REF",
	"TINT_REF",
	"TVAD_REF",
	"TCAD_REF",
	"VREG_REF",
	"MLIT_REF",
	"MVAR_REF",
	"TRIP_REF",
	"TNXT_REF",
	"TJMP_REF",
	"INDR_REF",
	"MLAB_REF",
	"ILIT_REF",
	"CDLT_REF",
	"TEMP_REF",
	"MFUN_REF",
	"MNXL_REF",
	"TSIZ_REF",
	"OCNT_REF"
};

LITDEF char *indents[11] =
{
	"",
	"  ",
	"    ",
	"      ",
	"        ",
	"          ",
	"            ",
	"              ",
	"                ",
	"                  ",
	"                    "
};

GBLREF int4	sa_temps_offset[];
GBLREF int4	sa_temps[];
LITREF int4	sa_class_sizes[];

static char	indent_str[128];
static int	last_indent = 0;

void cdbg_dump_triple(triple *dtrip, int indent)
{
	int		len;

	PRINTF("%s Triple %s [0x%08lx]   fwdptr: 0x%08lx   bkwdptr: 0x%08lx  srcline: %d  colmn: %d  rtaddr: %d\n",
	       cdbg_indent(indent), oc_tab_graphic[dtrip->opcode], dtrip,
	       dtrip->exorder.fl, dtrip->exorder.bl, dtrip->src.line, dtrip->src.column, dtrip->rtaddr);
	/*switch(dtrip->opcode)
	{
		case OC_CDLIT:
			PRINTF("%s  OC_CDLT ptr: 0x%08lx  len: 0x%08lx\n",
			       cdbg_indent(indent), opr->oprval.cdlt->addr, opr->oprval.cdlt->len);
			if (opr->oprval.cdlt->len)
				cdbg_dump_mstr(cdbg_indent(indent), opr->oprval.cdlt);
			break;
	} */
	cdbg_dump_operand(indent + 1, &dtrip->operand[0], OP_0);
	cdbg_dump_operand(indent + 1, &dtrip->operand[1], OP_1);
	if (dtrip->destination.oprclass)
		cdbg_dump_operand(indent + 1, &dtrip->destination, OP_DEST);
	fflush(stdout);
}

void cdbg_dump_shrunk_triple(triple *dtrip, int old_size, int new_size)
{
	PRINTF("Shrunken triple %s [0x%08lx]   fwdptr: 0x%08lx   bkwdptr: 0x%08lx  srcline: %d  colmn: %d  rtaddr: %d\n",
	       oc_tab_graphic[dtrip->opcode], dtrip, dtrip->exorder.fl, dtrip->exorder.bl, dtrip->src.line,
	       dtrip->src.column, dtrip->rtaddr);
	PRINTF("    old size: %d  new size: %d  shrinkage: %d\n", old_size, new_size, (old_size - new_size));
	fflush(stdout);
}

void cdbg_dump_operand(int indent, oprtype *opr, int opnum)
{
	triple	*rtrip;
	int	offset, len;
	char	*buff;

	if (opr)
		PRINTF("%s %s  [0x%08lx]  Type: %s\n", cdbg_indent(indent), oprtype_names[opnum], opr,
		       oprtype_type_names[opr->oprclass]);
	else
		PRINTF("%s ** Warning ** Null opr passed as operand\n", cdbg_indent(indent));
	if (!opr->oprclass)
		return;

	/* We have a real oprclass, dump it's info */
	switch(opr->oprclass)
	{
		case TVAR_REF:
			PRINTF("%s  Temporary variable index %d\n", cdbg_indent(indent), opr->oprval.temp);
			break;
		case TCAD_REF:
		case TVAD_REF:
			PRINTF("%s  %s reference - whatever it means: value is %d\n", cdbg_indent(indent),
			       ((TCAD_REF == opr->oprclass) ? "TCAD_REF" : "TVAD_REF"), opr->oprval.temp);
			break;
		case MVAR_REF:
			if (opr->oprval.vref)
			{
				PRINTF("%s   LS vref: 0x%08lx  RS vref: 0x%08lx  index: %d  varname: %s  last triple: 0x%08lx\n",
				       cdbg_indent(indent), opr->oprval.vref->lson, opr->oprval.vref->rson, opr->oprval.vref->mvidx,
				       cdbg_makstr(opr->oprval.vref->mvname.c, &buff, sizeof(opr->oprval.vref->mvname)),
				       opr->oprval.vref->last_fetch);
				free(buff);
			}
			else
				PRINTF("%s   ** Warning ** oprval.vref is NULL\n", cdbg_indent(indent));
			break;
		case TINT_REF:
		case TVAL_REF:
			offset = sa_temps_offset[opr->oprclass];
			offset -= (sa_temps[opr->oprclass] - opr->oprval.temp) * sa_class_sizes[opr->oprclass];
			PRINTF("%s   temp index: %d  offset: 0x%08lx\n", cdbg_indent(indent), opr->oprval.temp, offset);
			break;
		case ILIT_REF:
			PRINTF("%s   ilit value: %d [0x%08lx]\n", cdbg_indent(indent), opr->oprval.ilit, opr->oprval.ilit);
			break;
		case MLIT_REF:
			if (opr->oprval.mlit)
				PRINTF("%s   lit-ref fwdptr: 0x%08lx  bkwdptr: 0x%08lx  rtaddr: 0x%08lx\n",
				       cdbg_indent(indent), opr->oprval.mlit->que.fl, opr->oprval.mlit->que.bl,
				       opr->oprval.mlit->rt_addr);
			else
				PRINTF("%s   ** Warning ** oprval.mlit is NULL\n", cdbg_indent(indent));
			cdbg_dump_mval(indent, &opr->oprval.mlit->v);
			break;
		case TJMP_REF:
			if (opr->oprval.tref)
				PRINTF("%s   tjmp-ref jump list ptr: 0x%08lx\n", cdbg_indent(indent), &opr->oprval.tref->jmplist);
			else
				PRINTF("%s   ** Warning ** oprval.tref is NULL\n", cdbg_indent(indent));
			break;
		case TRIP_REF:
			rtrip = opr->oprval.tref;
			PRINTF("%s   Trip reference:\n", cdbg_indent(indent));
			cdbg_dump_triple(rtrip, indent + 1);
			break;
		case INDR_REF:
			cdbg_dump_operand(indent, opr->oprval.indr, opnum);
			break;
		case TSIZ_REF:
			if (opr->oprval.tsize)
				PRINTF("%s   triple at 0x%08lx has size %d\n", cdbg_indent(indent), opr->oprval.tsize->ct,
				       opr->oprval.tsize->size);
			else
				PRINTF("%s   ** Warning ** oprval.tsize is NULL\n", cdbg_indent(indent));
			break;
		case OCNT_REF:
			PRINTF("%s   offset from call to next triple: %d\n", cdbg_indent(indent), opr->oprval.offset);
			break;
		case MFUN_REF:
			if (opr->oprval.lab)
			{
				unsigned char mid[sizeof(mident) + 1];

				len = mid_len(&opr->oprval.lab->mvname);
				memcpy(mid, &opr->oprval.lab->mvname, len);
				mid[len] = 0;
				PRINTF("%s   mlabel name: %s\n", cdbg_indent(indent), mid);
			} else
				PRINTF("%s   ** Warning ** oprval.lab is NULL\n", cdbg_indent(indent));
			break;
		default:
			PRINTF("%s   %s bogus reference\n", cdbg_indent(indent), oprtype_type_names[opr->oprclass]);
	}
}

void cdbg_dump_mval(int indent, mval *mv)
{
	boolean_t	first;
	double		mvf;
	int4		mvd;

	PRINTF("%s   Type: 0x%1x  (", cdbg_indent(indent), mv->mvtype);
	first = TRUE;
	if (mv->mvtype & MV_NM)
	{
		PRINTF("Number");
		first = FALSE;
	}
	if (mv->mvtype & MV_INT)
	{
		if (!first)
			PRINTF(", ");
		PRINTF("Integer");
		first = FALSE;
	}
	if (mv->mvtype & MV_STR)
	{
		if (!first)
			PRINTF(", ");
		PRINTF("String");
		first = FALSE;
	}
	if (first)
	{
		PRINTF("Undefined MVAL)\n");
		return;
	}
	PRINTF(")  Sign: %d  Exp: %d\n", mv->sgn, mv->e);
	if (mv->mvtype & MV_NUM_MASK)
	{
		if (mv->mvtype & MV_INT)
		{
			mvd = mval2i(mv);
			PRINTF("%s   Integer value: %d\n", cdbg_indent(indent), mvd);
		} else
		{
			mvf = mval2double(mv);
			PRINTF("%s   Double value: %f\n", cdbg_indent(indent), mvf);
		}
	}
	if (mv->mvtype & MV_STR)
	{
		if (!mv->str.len)
			PRINTF("%s   String value: <null value>\n", cdbg_indent(indent));
		else
			cdbg_dump_mstr(indent, &mv->str);
	}
}

/* Dump value of a given mstr. Assumes length is non-zero */
void cdbg_dump_mstr(int indent, mstr *ms)
{
	unsigned char	*buffer;
	int		len;

	len = ms->len;
	buffer = malloc(len + 1);
	memcpy(buffer, ms->addr, len);
	buffer[len] = 0;
	PRINTF("%s   String value: %s\n", cdbg_indent(indent), buffer);
	free(buffer);
}

/* Provide string to do indenting of formatted output */
char *cdbg_indent(int indent)
{
	if (10 >= indent)
		return (char *)indents[indent];

	if (sizeof(indent_str) < indent * 2)
		GTMASSERT;
	if (indent > last_indent)
		memset(indent_str, ' ', indent * 2);
	indent_str[indent * 2] = 0;
	last_indent = indent;
	return indent_str;
}

/* Make a given addr/len string into a null terminate string */
char *cdbg_makstr(char *str, char **buf, int len)
{
	*buf = malloc(len + 1);
	memcpy(*buf, str, len);
	(*buf)[len] = 0;
	return *buf;
}
