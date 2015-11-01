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

#include <errno.h>
#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"

#include "hashdef.h"
#include "rtnhdr.h"
#include "zroutines.h"
#include "compiler.h"
#include "srcline.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "op.h"
#include "zbreak.h"

#define RT_TBL_SZ 20

GBLDEF htab_desc rt_name_tbl;
GBLREF mident zlink_mname;

int get_src_line(mval *routine, mval *label, int offset, mstr **srcret)
{
	int		fd, n, status, *lt_ptr, size;
	uint4		checksum, srcint;
	bool		badfmt, found, not_present;
	mstr		src;
	rhdtyp		*rtn_vector;
	zro_ent		*srcdir;
	ht_entry	*ht;
	mstr		*base, *current, *top;
	uint4		srcstat, *src_tbl;
	char		buff[MAX_SRCLINE], *c1, *c2, *c, *chkcalc;
	char		srcnamebuf[ sizeof(mident) + 2];
	error_def(ERR_TXTSRCFMT);

	srcstat = 0;
	*srcret = (mstr *)0;

	if (!rt_name_tbl.base)
		ht_init(&rt_name_tbl, RT_TBL_SZ);
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

	ht = ht_put(&rt_name_tbl, (mname *)&rtn_vector->routine_name, &not_present);
	src_tbl = (uint4 *)ht->ptr;
	if (not_present || ht->ptr == 0)
	{
		c = malloc(rtn_vector->src_full_name.len + 1);
		memcpy(c,rtn_vector->src_full_name.addr, rtn_vector->src_full_name.len);
		*(c + rtn_vector->src_full_name.len) = 0;	/* ensure string is null terminated */
		fd = OPEN(c, O_RDONLY);
		free(c);
		if (fd == -1)
		{
			n = mid_len(&rtn_vector->routine_name);
			memcpy (srcnamebuf, &rtn_vector->routine_name.c[0], n);
			if (srcnamebuf[0] == '%')	/* percents are translated to _ on filenames */
				srcnamebuf[0] = '_';
			srcnamebuf[n] = '.';
			srcnamebuf[n + 1] = 'm';
			src.addr = srcnamebuf;
			src.len = n + 2;
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
				fd = OPEN(c1, O_RDONLY);
				free(c1);
				if (fd == -1)
					rts_error(VARLSTCNT(1) errno);
				found = TRUE;
			} else
				found = FALSE;
		} else
			found = TRUE;

		if (!found)
			srcstat |= SRCNOTFND;
		n = found ? rtn_vector->lnrtab_len : 0;
		assert((found && n >= 1) || (n == 0));
		/* first two words are the status code and the number of entries */
		src_tbl = (uint4 *)malloc(n * sizeof(mstr) + sizeof(uint4) * 2);
		base = (mstr *)(src_tbl + 2);
		*(src_tbl + 1) = n;
		badfmt = FALSE;
		checksum = 0;
		for (current = base + 1, top = base + n ; current < top ; current++)
		{
			for (c = buff, c1 = buff + MAX_SRCLINE; c < c1; )
			{
				DOREAD_A_NOINT(fd, c, 1, status);
				if (-1 == status)
				{
					close(fd);
					rts_error(VARLSTCNT(3) ERR_TXTSRCFMT, 0, errno);
				} else if (!status)
				{
					break;
				}
				if (*c == '\n')
					break;
				++c;
			}
			if (c == c1)
				rts_error(VARLSTCNT(1) ERR_TXTSRCFMT);
			size = c - buff;
			if (size)
			{
				for (c2 = buff + size, c1 = buff;
					c1 < c2 && *c1 != ' ' && *c1 != '\t'; c1++)
						;
				/* calculate checksum */
				for (chkcalc = buff; chkcalc < c2; )
				{
					srcint = 0;
					if (c2 - chkcalc < sizeof (int4))
					{
						memcpy(&srcint, chkcalc, c2 - chkcalc);
						chkcalc = c2;
					} else
					{
						srcint = *(int4 *)chkcalc;
						chkcalc += sizeof(int4);
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
			if (!badfmt)
			{
				status = read(fd,buff,1);
				if (status || checksum != rtn_vector->checksum)
					badfmt = TRUE;
			}
			close(fd);
			if (badfmt)
				srcstat |= CHECKSUMFAIL;
		}
		*src_tbl = srcstat;
		ht->ptr = (char *) src_tbl;
	}
	srcstat |= *src_tbl;
	lt_ptr = (int *)find_line_addr(rtn_vector, &label->str, 0);
	if (!lt_ptr)
		srcstat |= LABELNOTFOUND;
	else if (!(srcstat & (SRCNOTFND | SRCNOTAVAIL)))
	{
		n = (int) (lt_ptr - (int *)LNRTAB_ADR(rtn_vector));
		n += offset;
		if (n == 0)
			srcstat |= ZEROLINE;
		else if (n < 0)
			srcstat |= NEGATIVELINE;
		else if (n >= rtn_vector->lnrtab_len)
			srcstat |= AFTERLASTLINE;
		else	/* successfully located line */
			*srcret = ((mstr *) (src_tbl + 2)) + n;
	}
	return srcstat;
}
