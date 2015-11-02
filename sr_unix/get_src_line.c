/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"

#include "rtnhdr.h"
#include "zroutines.h"
#include "compiler.h"
#include "srcline.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "op.h"
#include "zbreak.h"
#include "hashtab_mname.h"
#include "hashtab.h"

#define RT_TBL_SZ 20

GBLREF hash_table_mname rt_name_tbl;

int get_src_line(mval *routine, mval *label, int offset, mstr **srcret)
{
	int		n, *lt_ptr, size;
	uint4		checksum, srcint;
	boolean_t	found, added, eof_seen;
	mstr		src;
	rhdtyp		*rtn_vector;
	zro_ent		*srcdir;
	mstr		*base, *current, *top;
	uint4		srcstat, *src_tbl;
	char		buff[MAX_SRCLINE], *c1, *c2, *c, *chkcalc;
	char		srcnamebuf[SIZEOF(mident_fixed) + STR_LIT_LEN(DOTM)];
	ht_ent_mname	*tabent;
	var_tabent	rtnent;
	int		rc, fclose_res;
	char		*fgets_rc;
	FILE		*fp;

	error_def(ERR_TXTSRCFMT);

	srcstat = 0;
	*srcret = (mstr *)0;

	if (!rt_name_tbl.base)
		init_hashtab_mname(&rt_name_tbl, RT_TBL_SZ);
	assert (routine->mvtype & MV_STR);
	if (!(rtn_vector = find_rtn_hdr(&routine->str)))
	{
		op_zlink (routine, 0);
		rtn_vector = find_rtn_hdr(&routine->str);
		if (!rtn_vector)
			return OBJMODMISS;
	}
	if (!rtn_vector->src_full_name.len)
		return SRCNOTAVAIL;

	rtnent.var_name = rtn_vector->routine_name;
	COMPUTE_HASH_MNAME(&rtnent);
	added = add_hashtab_mname(&rt_name_tbl, &rtnent, NULL, &tabent);
	src_tbl = (uint4 *)tabent->value;
	if (added || tabent->value == 0)
	{
		c = malloc(rtn_vector->src_full_name.len + 1);
		memcpy(c,rtn_vector->src_full_name.addr, rtn_vector->src_full_name.len);
		*(c + rtn_vector->src_full_name.len) = 0;	/* ensure string is null terminated */
		/* At this point, it is not clear if Fopen will handle zos tagging correctly in all cases.
		 * especially when tagged with other than ISO8859-1 or IBM-1047. When we resurrect the zOS
		 * platform, we need to test this out.
		 */
		fp = Fopen(c, "r");
		free(c);
		if (NULL == fp)
		{
			n = (int)rtn_vector->routine_name.len;
			memcpy(srcnamebuf, rtn_vector->routine_name.addr, n);
			if (srcnamebuf[0] == '%')	/* percents are translated to _ on filenames */
				srcnamebuf[0] = '_';
			MEMCPY_LIT(&srcnamebuf[n], DOTM);
			src.addr = srcnamebuf;
			src.len = INTCAST(n + STR_LIT_LEN(DOTM));
			zro_search (0, 0, &src, &srcdir, TRUE);
			if (srcdir)
			{
				c1 = malloc(src.len + srcdir->str.len + 2);
				memcpy(c1,srcdir->str.addr,srcdir->str.len);
				c2 = c1 + srcdir->str.len;
				*c2++ = '/';
				memcpy(c2,src.addr,src.len);
				c2 += src.len;
				*c2++ = 0;
				fp = Fopen(c1, "r");
				free(c1);
				if (NULL == fp)
				{
					rts_error(VARLSTCNT(1) errno);
					assert(FALSE);
				}
				found = TRUE;
			} else
				found = FALSE;
		} else
			found = TRUE;

		if (!found)
		{
			srcstat |= SRCNOTFND;
			n = 0;
		} else
			n = (int)rtn_vector->lnrtab_len;
		assert((found && n >= 1) || (n == 0));
		/* first two words are the status code and the number of entries */
		src_tbl = (uint4 *)malloc(n * SIZEOF(mstr) + SIZEOF(uint4) * 2);
		base = RECAST(mstr *)(src_tbl + 2);
		*(src_tbl + 1) = n;
		checksum = 0;
		eof_seen = FALSE;
		for (current = base + 1, top = base + n ; current < top ; current++)
		{
			assert(found && (NULL != fp));
			if (!eof_seen)
			{
				FGETS(buff, MAX_SRCLINE, fp, fgets_rc);
				if (NULL == fgets_rc)
				{
					if (ferror(fp))
					{
						FCLOSE(fp, fclose_res);
						assert(!fclose_res);
						rts_error(VARLSTCNT(3) ERR_TXTSRCFMT, 0, errno);
						assert(FALSE);
					} else
					{
						eof_seen = TRUE;
						assert(feof(fp));
						size = 0;
					}
				} else
				{
					size = (int)STRLEN(buff);
					/* Strip trailing '\n' if any (if at least one byte was read in) */
					if (size && ('\n' == buff[size - 1]))
						size--;
				}
			} else	/* eof seen; nothing more to read in file */
				size = 0;
			if (size)
			{	/* Calculate checksum */
				for (chkcalc = buff, c2 = buff + size; chkcalc < c2; )
				{
					srcint = 0;
					if (c2 - chkcalc < SIZEOF(int4))
					{
						memcpy(&srcint, chkcalc, c2 - chkcalc);
						chkcalc = c2;
					} else
					{
						srcint = *(int4 *)chkcalc;
						chkcalc += SIZEOF(int4);
					}
					checksum ^= srcint;
					checksum >>= 1;
				}
				current->len = size;
				current->addr = malloc(size);
				memcpy(current->addr, buff, size);
			} else
			{
				current->addr = malloc(1);
				current->addr[0] = ' ';
				current->len = 1;
			}
		}
		if (found)
		{
	 		*base = *(base + 1);
			/* Ensure we have reached the end of the source file. If not, we need to issue a CHECKSUMFAIL error. Most
			 * often the !eof_seen part of the check is not needed since the checksums will not match. But if it so
			 * happens that the checksums do match, then this extra check helps us correctly identify a TXTSRCMAT error.
			 */
			if (!eof_seen)
			{
				FGETS(buff, MAX_SRCLINE, fp, fgets_rc);
				if ((NULL == fgets_rc) && !ferror(fp))
				{
					eof_seen = TRUE;
					assert(feof(fp));
				}
			}
			if (!eof_seen || (checksum != rtn_vector->checksum))
				srcstat |= CHECKSUMFAIL;
			FCLOSE(fp, fclose_res);
			assert(!fclose_res);
		}
		*src_tbl = srcstat;
		tabent->value = (char *) src_tbl;
	}
	srcstat |= *src_tbl;
	lt_ptr = (int *)find_line_addr(rtn_vector, &label->str, 0, NULL);
	if (!lt_ptr)
		srcstat |= LABELNOTFOUND;
	else if (!(srcstat & (SRCNOTFND | SRCNOTAVAIL)))
	{
		n = (int)(lt_ptr - (int *)LNRTAB_ADR(rtn_vector));
		n += offset;
		if (n == 0)
			srcstat |= ZEROLINE;
		else if (n < 0)
			srcstat |= NEGATIVELINE;
		else if (n >= rtn_vector->lnrtab_len)
			srcstat |= AFTERLASTLINE;
		else	/* successfully located line */
			*srcret = (RECAST(mstr *)(src_tbl + 2)) + n;
	}
	return srcstat;
}
